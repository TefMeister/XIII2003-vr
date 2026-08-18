add_library(D3DDrv SHARED dllmain.cpp frame_capture.cpp camera_hook.cpp vr_host.cpp)
target_link_libraries(D3DDrv PRIVATE d3d11)
set_target_properties(D3DDrv PROPERTIES
    OUTPUT_NAME "D3DDrv"
    SUFFIX ".dll"
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/proxy.def"
)
