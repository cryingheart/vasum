/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Piotr Bartosiewicz <p.bartosiewi@partner.samsung.com>
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
 * @author  Piotr Bartosiewicz (p.bartosiewi@partner.samsung.com)
 * @brief   Implementation of a class for communication transport between container and server
 */

#include "config.hpp"

#include "container-connection-transport.hpp"
#include "exception.hpp"

#include "utils/file-wait.hpp"
#include "utils/fs.hpp"
#include "log/logger.hpp"

#include <boost/filesystem.hpp>
#include <boost/system/system_error.hpp>

namespace security_containers {

namespace {

// Timeout in ms for waiting for dbus transport.
// Should be very long to ensure dbus in container is ready.
// TODO: this should be in container's configuration file
const unsigned int TRANSPORT_READY_TIMEOUT = 2 * 60 * 1000;

} // namespace


ContainerConnectionTransport::ContainerConnectionTransport(const std::string& runMountPoint)
    : mRunMountPoint(runMountPoint), mDetachOnExit(false)
{
    if (runMountPoint.empty()) {
        return;
    }
    boost::system::error_code errorCode;
    boost::filesystem::create_directories(runMountPoint, errorCode);
    if (errorCode) {
        LOGE("Initialization failed: could not create '" << runMountPoint << "' :" << errorCode);
        throw ContainerConnectionException("Could not create: " + runMountPoint +
                                           " :" + errorCode.message());
    }

    bool isMount = false;
    if (!utils::isMountPoint(runMountPoint, isMount)) {
        LOGE("Failed to check if " << runMountPoint << " is a mount point.");
        throw ContainerConnectionException("Could not check if " + runMountPoint +
                                           " is a mount point.");
    }

    if (!isMount) {
        LOGD(runMountPoint << " not mounted - mounting.");

        if (!utils::mountRun(runMountPoint)) {
            LOGE("Initialization failed: could not mount " << runMountPoint);
            throw ContainerConnectionException("Could not mount: " + runMountPoint);
        }
    }

    // if there is no systemd in the container this dir won't be created automatically
    // TODO: will require chown with USER namespace enabled
    std::string dbusDirectory = runMountPoint + "/dbus";
    boost::filesystem::create_directories(dbusDirectory, errorCode);
    if (errorCode) {
        LOGE("Initialization failed: could not create '" << dbusDirectory << "' :" << errorCode);
        throw ContainerConnectionException("Could not create: " + dbusDirectory +
                                           " :" + errorCode.message());
    }
}


ContainerConnectionTransport::~ContainerConnectionTransport()
{
    if (!mDetachOnExit) {
        if (!mRunMountPoint.empty()) {
            if (!utils::umount(mRunMountPoint)) {
                LOGE("Deinitialization failed: could not umount " << mRunMountPoint);
            }
        }
    }
}


std::string ContainerConnectionTransport::acquireAddress()
{
    if (mRunMountPoint.empty()) {
        return std::string();
    }

    const std::string dbusPath = mRunMountPoint + "/dbus/system_bus_socket";

    // TODO This should be done asynchronously.
    LOGT("Waiting for " << dbusPath);
    utils::waitForFile(dbusPath, TRANSPORT_READY_TIMEOUT);

    return "unix:path=" + dbusPath;
}

void ContainerConnectionTransport::setDetachOnExit()
{
    mDetachOnExit = true;
}

} // namespace security_containers