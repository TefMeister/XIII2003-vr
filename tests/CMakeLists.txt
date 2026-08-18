# tests/CMakeLists.txt
add_executable(pose_math_tests test_pose_math.cpp)
# doctest is vendored as a single header alongside the test source.
target_include_directories(pose_math_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pose_math_tests PRIVATE pose_math)

enable_testing()
add_test(NAME pose_math_tests COMMAND pose_math_tests)
