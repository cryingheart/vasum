/*
*  Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
*
*  Contact: Jan Olszak (j.olszak@samsung.com)
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
 * @brief   Class for sending the result of a method
 */

#include "config.hpp"

#include "api/ipc-method-result-builder.hpp"

namespace vasum {
namespace api {

IPCMethodResultBuilder::IPCMethodResultBuilder(const cargo::ipc::MethodResult::Pointer& methodResultPtr)
    : mMethodResultPtr(methodResultPtr)
{
}

void IPCMethodResultBuilder::setImpl(const std::shared_ptr<void>& data)
{
    mMethodResultPtr->set(data);
}

void IPCMethodResultBuilder::setVoid()
{
    mMethodResultPtr->setVoid();
}

void IPCMethodResultBuilder::setError(const std::string& , const std::string& message)
{
    // TODO: Change int codes to string names in IPC MethodResult
    mMethodResultPtr->setError(1, message);
}

std::string IPCMethodResultBuilder::getID() const
{
    return IPC_CONNECTION_PREFIX + mMethodResultPtr->getPeerID();
}

} // namespace result
} // namespace vasum


