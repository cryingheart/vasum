/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Mateusz Malicki <m.malicki2@samsung.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */


/**
 * @file
 * @author  Mateusz Malicki (m.malicki2@samsung.com)
 * @brief   This file contains security-containers-server's client definition
 */

#ifndef SECURITY_CONTAINERS_CLIENT_IMPL_HPP
#define SECURITY_CONTAINERS_CLIENT_IMPL_HPP

#include "security-containers-client.h"
#include <dbus/connection.hpp>
#include <exception>

/**
 * Structure which defines the dbus interface.
 */
struct DbusInterfaceInfo {
    DbusInterfaceInfo(const std::string& busName,
                      const std::string& objectPath,
                      const std::string& interface)
        : busName(busName),
          objectPath(objectPath),
          interface(interface) {}
    const std::string busName;
    const std::string objectPath;
    const std::string interface;
};

/**
 * security-containers's client definition.
 *
 * Client uses dbus API.
 */
class Client {
private:
    typedef std::function<void(GVariant* parameters)> SignalCallback;
    struct Status {
        Status();
        Status(VsmStatus status, const std::string& msg);
        VsmStatus mVsmStatus;
        std::string mMsg;
    };

    dbus::DbusConnection::Pointer mConnection;
    Status mStatus;

    VsmStatus callMethod(const DbusInterfaceInfo& info,
                         const std::string& method,
                         GVariant* args_in,
                         const std::string& args_spec_out = std::string(),
                         GVariant** args_out = NULL);
    VsmStatus signalSubscribe(const DbusInterfaceInfo& info,
                              const std::string& name,
                              SignalCallback signalCallback);

public:
    Client() noexcept;
    ~Client() noexcept;

    /**
     * Create client with system dbus address.
     *
     * @return status of this function call
     */
    VsmStatus createSystem() noexcept;

    /**
     * Create client.
     *
     * @param address Dbus socket address
     * @return status of this function call
     */
    VsmStatus create(const std::string& address) noexcept;

    /**
     *  @see ::vsm_get_status_message
     */
    const char* vsm_get_status_message() noexcept;

    /**
     *  @see ::vsm_get_status
     */
    VsmStatus vsm_get_status() noexcept;

    /**
     *  @see ::vsm_get_container_dbuses
     */
    VsmStatus vsm_get_container_dbuses(VsmArrayString* keys, VsmArrayString* values) noexcept;

    /**
     *  @see ::vsm_get_domain_ids
     */
    VsmStatus vsm_get_domain_ids(VsmArrayString* array) noexcept;

    /**
     *  @see ::vsm_get_active_container_id
     */
    VsmStatus vsm_get_active_container_id(VsmString* id) noexcept;

    /**
     *  @see ::vsm_lookup_domain_by_pid
     */
    VsmStatus vsm_lookup_domain_by_pid(int pid, VsmString* id) noexcept;

    /**
     *  @see ::vsm_set_active_container
     */
    VsmStatus vsm_set_active_container(const char* id) noexcept;

    /**
     *  @see ::vsm_create_domain
     */
    VsmStatus vsm_create_domain(const char* id) noexcept;

    /**
     *  @see ::vsm_add_state_callback
     */
    VsmStatus vsm_add_state_callback(VsmContainerDbusStateCallback containerDbusStateCallback,
                                     void* data) noexcept;

    /**
     *  @see ::vsm_notify_active_container
     */
    VsmStatus vsm_notify_active_container(const char* application, const char* message) noexcept;

    /**
     *  @see ::vsm_file_move_request
     */
    VsmStatus vsm_file_move_request(const char* destContainer, const char* path) noexcept;
    /**
     *  @see ::vsm_notification
     */
    VsmStatus vsm_notification(VsmNotificationCallback notificationCallback, void* data) noexcept;
    /**
     *  @see ::vsm_start_glib_loop
     */
    static VsmStatus vsm_start_glib_loop() noexcept;

    /**
     *  @see ::vsm_stop_glib_loop
     */
    static VsmStatus vsm_stop_glib_loop() noexcept;
};

#endif /* SECURITY_CONTAINERS_CLIENT_IMPL_HPP */
