add_library(D3DDrv SHARED
    dllmain.cpp frame_capture.cpp camera_hook.cpp vr_host.cpp
    openxr_host.cpp steamvr_host.cpp shutdown_hook.cpp focus_hook.cpp)

target_include_directories(D3DDrv PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/openxr/include
    ${CMAKE_SOURCE_DIR}/third_party/openvr/headers)

# openxr_loader and openvr_api are BOTH delay-loaded (see LINK_FLAGS) so they are
# only pulled in when their host actually runs ([VR] OpenXR=1 / [VR] SteamVR=1);
# machines without the respective runtime are unaffected.
target_link_libraries(D3DDrv PRIVATE
    d3d11 dxgi pose_math capture_core delayimp
    ${CMAKE_SOURCE_DIR}/third_party/openxr/lib/openxr_loader.lib
    ${CMAKE_SOURCE_DIR}/third_party/openvr/lib/openvr_api.lib)

set_target_properties(D3DDrv PROPERTIES
    OUTPUT_NAME "D3DDrv"
    SUFFIX ".dll"
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/proxy.def /DELAYLOAD:openxr_loader.dll /DELAYLOAD:openvr_api.dll"
)
