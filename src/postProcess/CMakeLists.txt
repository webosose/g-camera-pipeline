# Copyright(c) 2023 LG Electronics Inc.
set(LIB_NAME CameraPostProcess)
set(MODULE_NAME ${LIB_NAME})

file(GLOB_RECURSE LIB_SRC_LIST
  ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(${LIB_NAME} SHARED ${LIB_SRC_LIST})

if (ENABLE_SIC_DRVINF)
        set(EXT_HAL_LIBS
                hal_lg115x
                hal_photo
        )
else()
        set(EXT_HAL_LIBS
        )
endif()

target_link_libraries(
    ${LIB_NAME}
    ${PMLOGLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${GSTPBUTIL_LIBRARIES}
    ${GST_COOL_LIBRARIES}
    ${PBNJSON_LIBRARIES}
    ${Boost_REGEX_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${LIBAV-PROXY_LDFLAGS}
    ${LS2_LIBRARY}
    ${EXT_HAL_LIBS}
    ${AVCAPTURE_LIBRARIES}
    ums_connector
    ${RESOURCE-CALCULATOR_LDFLAGS}
    resource_mgr_client
    resource_mgr_client_c
    pthread
    )

webos_build_library(NAME ${LIB_NAME})
webos_build_pkgconfig(${PROJECT_SOURCE_DIR}/files/pkgconfig/StarfishCameraPlayer)
