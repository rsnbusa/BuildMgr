cmake_minimum_required(VERSION 3.5)

get_filename_component(DEV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

set(PROJECT_ROOT "${DEV_ROOT}/")

set(SUBMODULE_ROOT "${DEV_ROOT}/../../../")

set(PROJECT_NAME "mqtt_ssl")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(MAIN_SRCS ${PROJECT_ROOT}/main/buildMgr.cpp)

set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} ${SUBMODULE_ROOT}")
set(BUILD_COMPONENTS "${BUILD_COMPONENTS} espmqtt")

project(${PROJECT_NAME})

