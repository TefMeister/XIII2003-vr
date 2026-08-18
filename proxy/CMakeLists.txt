add_library(D3DDrv SHARED dllmain.cpp frame_capture.cpp camera_hook.cpp)
set_target_properties(D3DDrv PROPERTIES
    OUTPUT_NAME "D3DDrv"
    SUFFIX ".dll"
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/proxy.def"
)
