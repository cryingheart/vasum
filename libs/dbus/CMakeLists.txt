# Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#
#
# @file   CMakeLists.txt
# @author Dariusz Michaluk (d.michaluk@samsung.com)
#

PROJECT(SimpleDbus)

MESSAGE(STATUS "")
MESSAGE(STATUS "Generating makefile for the libSimpleDbus...")
FILE(GLOB HEADERS *.hpp ${COMMON_FOLDER}/config.hpp)
FILE(GLOB HEADERS_UTILS ${COMMON_FOLDER}/utils/callback-guard.hpp)
FILE(GLOB SRCS          *.cpp *.hpp)

SET(_LIB_VERSION_ "${VERSION}")
SET(_LIB_SOVERSION_ "0")
SET(PC_FILE "lib${PROJECT_NAME}.pc")

## Setup target ################################################################
ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRCS})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES
    SOVERSION   ${_LIB_SOVERSION_}
    VERSION     ${_LIB_VERSION_}
)

## Link libraries ##############################################################
PKG_CHECK_MODULES(DBUS_DEPS REQUIRED glib-2.0 gio-2.0 libLogger)
INCLUDE_DIRECTORIES(${LIBS_FOLDER})
INCLUDE_DIRECTORIES(${COMMON_FOLDER})
INCLUDE_DIRECTORIES(SYSTEM ${CARGO_UTILS_INCLUDE_DIRS} ${DBUS_DEPS_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CARGO_UTILS_LIBRARIES} ${DBUS_DEPS_LIBRARIES})

## Generate the pc file ########################################################
CONFIGURE_FILE(${PC_FILE}.in ${CMAKE_CURRENT_BINARY_DIR}/${PC_FILE} @ONLY)

## Install #####################################################################
INSTALL(FILES       ${CMAKE_CURRENT_BINARY_DIR}/${PC_FILE}
        DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

INSTALL(TARGETS     ${PROJECT_NAME}
        DESTINATION ${LIB_INSTALL_DIR}
        COMPONENT   RuntimeLibraries)

INSTALL(FILES       ${HEADERS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/vasum-tools/dbus)

INSTALL(FILES       ${HEADERS_UTILS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/vasum-tools/dbus/utils)
