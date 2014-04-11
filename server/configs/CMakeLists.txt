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
# @author Lukasz Pawelczyk (l.pawelczyk@partner.samsung.com)
#

MESSAGE(STATUS "Installing configs to " ${SC_CONFIG_INSTALL_DIR})

FILE(GLOB container_CONF     containers/*.conf)
FILE(GLOB admin_CONF         libvirt-config/*.xml)


## Installations ###############################################################
INSTALL(FILES       daemon.conf
        DESTINATION ${SC_CONFIG_INSTALL_DIR})

INSTALL(FILES       ${container_CONF}
        DESTINATION ${SC_CONFIG_INSTALL_DIR}/containers)

INSTALL(FILES       ${admin_CONF}
        DESTINATION ${SC_CONFIG_INSTALL_DIR}/libvirt-config)