camera-window-manager
=============

Summary
-------
The camera-window-manager module provides interfaces for communicating with LSM for GAV.

Description
-----------
* The camera-window-manager module is a sub-module of render connector.
* The camera-window-manager module takes charge of communicating with LSM for GAV.
* The camera-window-manager module provides these interfaces.
1. Registering video/media window ID
2. Attaching punch through
3. Detaching punch through
4. Attaching surface
5. Detaching surface

Reference
---------
http://collab.lge.com/main/pages/viewpage.action?pageId=879424980

Test
----
### Punch through
stop sam
./camera_window_manager_exporter &
./camera_window_manager_importer $windowID PunchThrough
### Texture rendering
stop sam
./camera_window_manager_exporter &
./camera_window_manager_importer $windowID Surface

Dependencies
============
- wayland
- luna-surfacemanager-extensions
- gpu-libs(For test tool : camera_window_manager_exporter, camera_window_manager_importer)


