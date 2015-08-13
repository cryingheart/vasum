/*
*  Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
*
*  Contact: Jan Olszak <j.olszak@samsung.com>
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
 * @author  Jan Olszak (j.olszak@samsung.com)
 * @brief   Types definitions and helper functions
 */

#include "config.hpp"

#include "ipc/types.hpp"
#include "ipc/unique-id.hpp"
#include "logger/logger.hpp"

#include <mutex>

namespace ipc {

namespace {
// complex types cannot be easily used with std::atomic - these require libatomic to be linked
// instead, secure the ID getters with mutex
MessageID gLastMessageID;
PeerID gLastPeerID;

std::mutex gMessageIDMutex;
std::mutex gPeerIDMutex;
} // namespace

MessageID getNextMessageID()
{
    std::unique_lock<std::mutex> lock(gMessageIDMutex);
    UniqueID uid;
    uid.generate();
    gLastMessageID = uid;
    return gLastMessageID;
}

PeerID getNextPeerID()
{
    std::unique_lock<std::mutex> lock(gPeerIDMutex);
    UniqueID uid;
    uid.generate();
    gLastPeerID = uid;
    return gLastPeerID;
}


} // namespace ipc
