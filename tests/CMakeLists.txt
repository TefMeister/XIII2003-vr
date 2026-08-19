# tests/CMakeLists.txt
add_executable(pose_math_tests test_pose_math.cpp)
# doctest is vendored as a single header alongside the test source.
target_include_directories(pose_math_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pose_math_tests PRIVATE pose_math)

add_executable(frame_decode_tests test_frame_decode.cpp)
target_include_directories(frame_decode_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(frame_decode_tests PRIVATE capture_core)

add_executable(perf_stats_tests test_perf_stats.cpp)
target_include_directories(perf_stats_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(perf_stats_tests PRIVATE capture_core)

enable_testing()
add_test(NAME pose_math_tests COMMAND pose_math_tests)
add_test(NAME frame_decode_tests COMMAND frame_decode_tests)
add_test(NAME perf_stats_tests COMMAND perf_stats_tests)
