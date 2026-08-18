# pose_math/CMakeLists.txt
add_library(pose_math STATIC pose_math.cpp pose_math.h)
target_include_directories(pose_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
