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
# @author Mateusz Malicki (m.malicki2@samsung.com)
#

PROJECT(vasum-client)
MESSAGE(STATUS "")
MESSAGE(STATUS "Generating makefile for the Client...")
FILE(GLOB project_SRCS *.cpp *.hpp *.h)

SET(_LIB_VERSION_ "${VERSION}")
SET(_LIB_SOVERSION_ "0")
SET(PC_FILE "${PROJECT_NAME}.pc")

## Set the default ELF image symbol visibility to hidden. Using this feature
## can very substantially improve linking and load times of shared object
## libraries, produce more optimized code, provide near-perfect API export
## and prevent symbol clashes
ADD_DEFINITIONS(-fvisibility=hidden)

## Setup target ################################################################
ADD_LIBRARY(${PROJECT_NAME} SHARED ${project_SRCS})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES
        SOVERSION ${_LIB_SOVERSION_}
        VERSION ${_LIB_VERSION_}
        COMPILE_DEFINITIONS HOST_IPC_SOCKET="${VSM_SERVER_IPC_SOCKET_PATH}")

## Link libraries ##############################################################
PKG_CHECK_MODULES(CLIENT_DEPS REQUIRED libcargo-ipc libLogger)

INCLUDE_DIRECTORIES(${COMMON_FOLDER})
INCLUDE_DIRECTORIES(${SERVER_FOLDER})
INCLUDE_DIRECTORIES(SYSTEM ${CARGO_UTILS_INCLUDE_DIRS} ${CLIENT_DEPS_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CARGO_UTILS_LIBRARIES} ${CLIENT_DEPS_LIBRARIES})

IF(NOT WITHOUT_DBUS)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} SimpleDbus)
ENDIF(NOT WITHOUT_DBUS)

## Generate the pc file ########################################################
CONFIGURE_FILE(${PC_FILE}.in ${CMAKE_BINARY_DIR}/${PC_FILE} @ONLY)


## Install #####################################################################
INSTALL(FILES       ${CMAKE_BINARY_DIR}/${PC_FILE}
        DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

INSTALL(TARGETS     ${PROJECT_NAME}
        DESTINATION ${LIB_INSTALL_DIR}
        COMPONENT   RuntimeLibraries)

INSTALL(FILES       vasum-client.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/vasum)
