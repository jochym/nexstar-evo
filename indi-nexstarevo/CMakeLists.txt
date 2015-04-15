########### NexStar Evolution INDI driver ##############
PROJECT(indi-nexstarevo C CXX)
cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
find_library(LIBEV_LIBRARY NAMES ev)

set(NSEVO_VERSION_MAJOR 0)
set(NSEVO_VERSION_MINOR 1)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR}/..)

add_executable(indi-nexstarevo nexstarevo.cpp NexStarAUXScope.cpp)
target_link_libraries(indi-nexstarevo indiAlignmentDriver ev
${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES})


