# capture_core: pure, Windows-free capture-path logic (unit-tested).
add_library(capture_core STATIC frame_decode.cpp perf_stats.cpp readback_ring.cpp)
target_include_directories(capture_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
