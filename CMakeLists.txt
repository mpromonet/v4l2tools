cmake_minimum_required(VERSION 3.0)

# set project name from current directory
get_filename_component(BASENAME ${CMAKE_SOURCE_DIR} NAME)
project(${BASENAME})

set (CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "-Wall")

# define executable to build
include_directories("include")
add_executable(${PROJECT_NAME} src/v4l2compress.cpp)

# v4l2wrapper
add_subdirectory(v4l2wrapper)
include_directories("v4l2wrapper/inc")
target_link_libraries (${PROJECT_NAME} v4l2wrapper)

#libjpeg
include(FindJPEG)
if (JPEG_FOUND)
  include_directories(${JPEG_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${JPEG_LIBRARY})
  add_definitions(-DHAVE_JPEG)
endif()

#neon support
MESSAGE("CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  add_definitions(-DLIBYUV_DISABLE_NEON)
endif()

#yuv
add_subdirectory(libyuv EXCLUDE_FROM_ALL)
include_directories("libyuv/include")
target_link_libraries (${PROJECT_NAME} yuv)

find_package(PkgConfig)

# x264
pkg_check_modules(X264 x264)
if (X264_FOUND) 
  include_directories(${X264_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${X264_LIBRARIES})
  add_definitions(-DHAVE_X264)
endif()

# x265
pkg_check_modules(X265 x265)
if (X265_FOUND) 
  include_directories(${X265_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${X265_LIBRARIES})
  add_definitions(-DHAVE_X265)
endif()

# vpx
pkg_check_modules(VPX vpx)
if (VPX_FOUND) 
  include_directories(${VPX_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${VPX_LIBRARIES})
  add_definitions(-DHAVE_VPX)
endif()

# install
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
