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
 * @brief   Class for storing result of a method - data or exception
 */

#ifndef COMMON_IPC_RESULT_HPP
#define COMMON_IPC_RESULT_HPP

#include <functional>
#include <exception>
#include <memory>

namespace ipc {

template<typename Data>
class Result {
public:
    Result()
        : mData(nullptr),
          mExceptionPtr(nullptr)
    {}

    Result(std::shared_ptr<Data>&& data, std::exception_ptr&& exceptionPtr)
        : mData(std::move(data)),
          mExceptionPtr(std::move(exceptionPtr))
    {}

    std::shared_ptr<Data> get() const
    {
        if (mExceptionPtr) {
            std::rethrow_exception(mExceptionPtr);
        }
        return mData;
    }

    bool isValid() const
    {
        return (bool)mExceptionPtr || (bool)mData;
    }

private:
    std::shared_ptr<Data> mData;
    std::exception_ptr mExceptionPtr;
};

template<typename Data>
struct ResultHandler {
    typedef std::function < void(Result<Data>&&) > type;
};

} // namespace ipc

#endif // COMMON_IPC_RESULT_HPP