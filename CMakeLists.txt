cmake_minimum_required(VERSION 3.0)

# set project name from current directory
get_filename_component(BASENAME ${CMAKE_SOURCE_DIR} NAME)
project(${BASENAME})

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

# define executable to build
include_directories("include")
add_executable(${PROJECT_NAME} src/v4l2compress.cpp)

# v4l2wrapper
add_subdirectory(v4l2wrapper)
target_include_directories(${PROJECT_NAME} PUBLIC "v4l2wrapper/inc")
target_link_libraries (${PROJECT_NAME} v4l2wrapper)

#libjpeg
option (WITH_JPEG "Enable JPEG support" ON)
if (WITH_JPEG)
  include(FindJPEG)
  if (JPEG_FOUND)
    target_include_directories(${PROJECT_NAME} PUBLIC ${JPEG_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${JPEG_LIBRARY})
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DHAVE_JPEG)
  else()
    message(WARNING "JPEG not found")
  endif()
endif()

#neon support
MESSAGE("CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DLIBYUV_DISABLE_NEON)
endif()

#yuv
add_subdirectory(libyuv EXCLUDE_FROM_ALL)
target_include_directories(${PROJECT_NAME} PUBLIC "libyuv/include")
target_link_libraries (${PROJECT_NAME} yuv)

find_package(PkgConfig)

# x264
option (WITH_X264 "Enable x264 support" ON)
if (WITH_X264)
  pkg_check_modules(X264 x264)
  if (X264_FOUND)
    target_include_directories(${PROJECT_NAME} PUBLIC ${X264_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${X264_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DHAVE_X264)
  else()
    message(INFO "x264 not found")
  endif()
endif()

# x265
option (WITH_X265 "Enable x265 support" ON)
if (WITH_X265)
  pkg_check_modules(X265 x265)
  if (X265_FOUND)
    target_include_directories(${PROJECT_NAME} PUBLIC ${X265_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${X265_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DHAVE_X265)
  else()
    message(INFO "x265 not found")
  endif()
endif()

# vpx
option (WITH_VPX "enable VPX support" ON)
if (WITH_VPX)
  pkg_check_modules(VPX vpx)
  if (VPX_FOUND)
    target_include_directories(${PROJECT_NAME} PUBLIC ${VPX_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${VPX_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DHAVE_VPX)
  else()
    message(INFO "VPX not found")
  endif()
endif()

# install
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
