/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Mateusz Malicki  <m.malicki2@samsung.com>
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
 * @brief   Vasum base exception implementation
 */

#include "base-exception.hpp"

#include <string>
#include <cstring>
#include <cerrno>

namespace vasum {

const int ERROR_MESSAGE_BUFFER_CAPACITY = 256;

std::string getSystemErrorMessage()
{
    char buf[ERROR_MESSAGE_BUFFER_CAPACITY];
    return strerror_r(errno, buf, sizeof(buf));
}

} // namespace vasum