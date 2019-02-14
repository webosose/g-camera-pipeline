/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2019 LG Electronics
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-camsrc
 *
 * FIXME:Describe camsrc here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! camsrc ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <stdio.h>
#include <poll.h>

#include "gstcamsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_camsrc_debug);
#define GST_CAT_DEFAULT gst_camsrc_debug

#define VERSION "1.0.0"
#define FRAME_SIZE 8294400

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_DEVICE,
    PROP_IOMODE
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS ("ANY")
        );

#define gst_camsrc_parent_class parent_class
G_DEFINE_TYPE (Gstcamsrc, gst_camsrc, GST_TYPE_PUSH_SRC);

GType gst_camsrc_iomode_get_type(void)
{
    static GType g_type=0;
    if(!g_type){
        static const GEnumValue iomode_types[] = {
            {GST_V4L2_IO_MMAP,"memory map mode","mmap"},
            {GST_V4L2_IO_USERPTR,"user pointer mode","userptr"},
            {GST_V4L2_IO_DMABUF_IMPORT,"dmabuf import mode","dmabuf"},
            {GST_V4L2_IO_DMABUF_EXPORT,"dmabuf export mode","dmabuf"},
            {0,NULL,NULL}
        };
        g_type = g_enum_register_static("Gstcamsrciomodetype",iomode_types);
    }
    return g_type;
}

static gboolean gst_camsrc_negotiate (GstBaseSrc * basesrc);
static void gst_camsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_camsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn
gst_camsrc_change_state (GstPushSrc * element, GstStateChange transition);
static gboolean
gst_camsrc_query (GstBaseSrc * bsrc, GstQuery * query);

static gboolean gst_camsrc_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_camsrc_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);
static GstFlowReturn gst_camsrc_create (GstPushSrc * src, GstBuffer ** out);
static GstFlowReturn gst_camsrc_fill (GstPushSrc * src, GstBuffer * out);

/* GObject vmethod implementations */

/* initialize the camsrc's class */
static void
gst_camsrc_class_init (GstcamsrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *basesrc_class;
  GstPushSrcClass *pushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  basesrc_class = (GstBaseSrcClass *)klass;
  pushsrc_class = (GstPushSrcClass *)klass;

  gobject_class->set_property = gst_camsrc_set_property;
  gobject_class->get_property = gst_camsrc_get_property;

  gstelement_class->change_state = gst_camsrc_change_state;

  g_object_class_install_property (gobject_class, PROP_DEVICE,
          g_param_spec_string ("device", "device", "Device location",
              DEFAULT_PROP_DEVICE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class,PROP_IOMODE,
          g_param_spec_enum("iomode","iomode","Memory mode",
              GST_TYPE_CAMSRC_IOMODE,DEFAULT_PROP_IOMODE,
              (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_camsrc_query);
  basesrc_class->negotiate = GST_DEBUG_FUNCPTR (gst_camsrc_negotiate);
  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_camsrc_create);

  gst_element_class_set_static_metadata (gstelement_class,
          "Camera Source",
          "webos camera source",
          "Read data from a camera device",
          "Gururaj Patil");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_camsrc_init (Gstcamsrc * filter)
{
  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  gst_base_src_set_format (GST_BASE_SRC (filter), GST_FORMAT_TIME);
  filter->device = NULL;
}

static void
gst_camsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstcamsrc *filter = GST_CAMSRC (object);

  if (NULL == filter)
  {
      return 0;
  }
  switch (prop_id) {
      case PROP_DEVICE:
          filter->device = g_value_dup_string (value);
          break;
      case PROP_IOMODE:
          filter->mode = g_value_get_enum(value);
          break;
      default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
  }
}

    static gboolean
gst_camsrc_query (GstBaseSrc * bsrc, GstQuery * query)
{
    gboolean res = FALSE;

    switch (GST_QUERY_TYPE (query)) {
        default:
            res = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
            break;
    }
}

static gboolean set_value (GQuark field, const GValue * value, gpointer pfx)
{
    gchar *str = gst_value_serialize (value);
    char temp[100];


    strcpy (temp,g_quark_to_string (field));

    if ((strcmp(temp,"width") == 0))
    {
        if (atoi(str) != 0)
        {
            streamformat.stream_width = atoi(str);
        }
    }
    else if ((strcasecmp(temp,"height") == 0))
    {
        if (atoi(str) != 0)
        {
            streamformat.stream_height = atoi(str);
        }
    }
    else if ((strcasecmp(temp,"format") == 0))
    {
        if ((strcasecmp(str,"YUY2") == 0))
        {
            streamformat.pixel_format = CAMERA_PIXEL_FORMAT_YUYV;
        }
    }
    g_free (str);
    return TRUE;
}

    static gboolean
gst_camsrc_negotiate (GstBaseSrc * basesrc)
{
    GstCaps *thiscaps;
    GstCaps *caps = NULL;
    GstCaps *peercaps = NULL;
    gboolean result = FALSE;
    const gchar * pfx = " ";
    GstStructure *pref = NULL;
    streamformat.stream_width = DEFAULT_VIDEO_WIDTH;
    streamformat.stream_height = DEFAULT_VIDEO_HEIGHT;
    streamformat.pixel_format = DEFAULT_PIXEL_FORMAT;

    /* first see what is possible on our source pad */
    thiscaps = gst_pad_query_caps (GST_BASE_SRC_PAD (basesrc), NULL);
    GST_DEBUG_OBJECT (basesrc, "caps of src: %" GST_PTR_FORMAT, thiscaps);

    /* query the peer caps*/
    peercaps = gst_pad_peer_query_caps (GST_BASE_SRC_PAD (basesrc), NULL);
    GST_DEBUG_OBJECT (basesrc, "caps of peer: %" GST_PTR_FORMAT, peercaps);

    if (peercaps && !gst_caps_is_any (peercaps)) {
        /* Prefer the first caps we are compatible with that the
         * peer proposed */
        caps = gst_caps_intersect_full (peercaps, thiscaps,
                GST_CAPS_INTERSECT_FIRST);

        GST_DEBUG_OBJECT (basesrc, "intersect: %" GST_PTR_FORMAT, caps);

        gst_caps_unref (thiscaps);
    } else {
        /* no peer or peer have ANY caps, work
         * with our own caps then */
        caps = thiscaps;
    }
    if (caps) {
        /* now fixate */
        if (!gst_caps_is_empty (caps)) {

            if (peercaps && !gst_caps_is_any (peercaps))
            {
                pref = gst_caps_get_structure (peercaps, 0);
                gst_structure_foreach (pref, set_value, (gpointer) pfx);
                result = gst_base_src_set_caps (basesrc, peercaps);
            }
        }
    }
    if (peercaps)
      gst_caps_unref (peercaps);
  return result;
}

    static GstFlowReturn
gst_camsrc_create (GstPushSrc * src, GstBuffer ** buf)
{
    GstFlowReturn ret;
    Gstcamsrc *camsrc = GST_CAMSRC (src);
    int retval = 0;
    GstMapInfo map;
    buffer_t frame_buffer;
    int check = 0;
    int fd = 0;

    /*We check if the camera is started. If camera is not started then we have
     * set format and set buffer and this will be done only once at the start*/
    if(!bStarted)
    {
        retval = camera_hal_if_set_format(camsrc->p_h_camera, streamformat);
        switch (camsrc->mode)
        {
            case GST_V4L2_IO_MMAP:
                retval = camera_hal_if_set_buffer(camsrc->p_h_camera, 4, IOMODE_MMAP);
                break;

            case GST_V4L2_IO_DMABUF_IMPORT:
                retval = camera_hal_if_set_buffer(camsrc->p_h_camera, 4, IOMODE_DMABUF);
                break;

            case GST_V4L2_IO_USERPTR:
                retval = camera_hal_if_set_buffer(camsrc->p_h_camera, 4, IOMODE_USERPTR);
                break;

            default:
                break;
        }

        retval = camera_hal_if_start_capture(camsrc->p_h_camera);
        bStarted = 1;
        frame_buffer.start = malloc(FRAME_SIZE);
        retval = camera_hal_if_get_buffer(camsrc->p_h_camera,&frame_buffer);
        length = frame_buffer.length;
        free(frame_buffer.start);

    }

    retval = camera_hal_if_get_fd(camsrc->p_h_camera,&fd);
    struct pollfd fds[] = {
        { .fd = fd, .events = POLLIN },
    };
    if((check = poll(fds, 2, 2000)) > 0)
    {

        ret = GST_BASE_SRC_CLASS (parent_class)->alloc (GST_BASE_SRC (src), 0,
                length, buf);
        gst_buffer_map (*buf, &map, GST_MAP_WRITE);
        frame_buffer.start=map.data;
        retval = camera_hal_if_get_buffer(camsrc->p_h_camera,&frame_buffer);
        gst_buffer_unmap(*buf,&map);

        retval = camera_hal_if_release_buffer(camsrc->p_h_camera,frame_buffer);
    }
    return GST_FLOW_OK;
}

    static GstStateChangeReturn
gst_camsrc_change_state (GstPushSrc * element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    Gstcamsrc *camsrc = GST_CAMSRC (element);
    int retval = 0;

    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
            {
                /* open the device */
                retval = camera_hal_if_init(&camsrc->p_h_camera, subsystem);
                retval = camera_hal_if_open_device(camsrc->p_h_camera, camsrc->device);
            }
        default:
            break;
    }

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    return ret;
}

    static void
gst_camsrc_get_property (GObject * object, guint prop_id,
        GValue * value, GParamSpec * pspec)
{
    Gstcamsrc *filter = GST_CAMSRC (object);

    switch (prop_id) {
        case PROP_DEVICE:
            g_value_set_string (value, filter->device);
            break;
        case PROP_IOMODE:
            g_value_set_enum(value,filter->mode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_camsrc_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  Gstcamsrc *filter;
  gboolean ret;

  filter = GST_CAMSRC (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
camsrc_init (GstPlugin * camsrc)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template camsrc' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_camsrc_debug, "camsrc",
      0, "Element for camera source");

  return gst_element_register (camsrc, "camsrc", GST_RANK_PRIMARY,
      GST_TYPE_CAMSRC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "camsrc"
#endif

/* gstreamer looks for this structure to register camsrcs
 *
 * exchange the string 'Template camsrc' with your camsrc description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    camsrc,
    "Template camsrc",
    camsrc_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
