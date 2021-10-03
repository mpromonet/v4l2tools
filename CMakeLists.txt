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
aux_source_directory(v4l2wrapper/src LIBSRC_FILES)
include_directories("v4l2wrapper/inc")
add_library(v4l2wrapper STATIC ${LIBSRC_FILES})
target_link_libraries (${PROJECT_NAME} v4l2wrapper)

#libyuv
include(FindJPEG)
if (JPEG_FOUND)
  include_directories(${JPEG_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${JPEG_LIBRARY})
  add_definitions(-DHAVE_JPEG)
endif()

add_subdirectory(libyuv EXCLUDE_FROM_ALL)
include_directories("libyuv/include")
target_link_libraries (${PROJECT_NAME} yuv)

# install
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
