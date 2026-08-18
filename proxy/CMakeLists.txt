add_library(D3DDrv SHARED
    dllmain.cpp frame_capture.cpp camera_hook.cpp vr_host.cpp openxr_host.cpp)

target_include_directories(D3DDrv PRIVATE ${CMAKE_SOURCE_DIR}/third_party/openxr/include)

# openxr_loader is DELAY-loaded (see LINK_FLAGS) so it is only pulled in when the
# OpenXR host actually runs ([VR] OpenXR=1); machines without it are unaffected.
target_link_libraries(D3DDrv PRIVATE
    d3d11 dxgi pose_math delayimp
    ${CMAKE_SOURCE_DIR}/third_party/openxr/lib/openxr_loader.lib)

set_target_properties(D3DDrv PROPERTIES
    OUTPUT_NAME "D3DDrv"
    SUFFIX ".dll"
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/proxy.def /DELAYLOAD:openxr_loader.dll"
)
