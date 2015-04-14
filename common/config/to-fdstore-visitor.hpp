/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
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
 * @brief   Visitor for saving to FDStore
 */

#ifndef COMMON_CONFIG_TO_FDSTORE_VISITOR_HPP
#define COMMON_CONFIG_TO_FDSTORE_VISITOR_HPP

#include "config/is-visitable.hpp"
#include "config/fdstore.hpp"

#include <string>

namespace config {

class ToFDStoreVisitor {

public:
    ToFDStoreVisitor(int fd)
        : mStore(fd)
    {
    }

    ToFDStoreVisitor(const ToFDStoreVisitor&) = default;

    ToFDStoreVisitor& operator=(const ToFDStoreVisitor&) = delete;

    template<typename T>
    void visit(const std::string&, const T& value)
    {
        writeInternal(value);
    }

private:
    FDStore mStore;

    void writeInternal(const std::string& value)
    {
        writeInternal(value.size());
        mStore.write(value.c_str(), value.size());
    }

    template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
    void writeInternal(const T& value)
    {
        mStore.write(&value, sizeof(T));
    }

    template<typename T, typename std::enable_if<isVisitable<T>::value, int>::type = 0>
    void writeInternal(const T& value)
    {
        ToFDStoreVisitor visitor(*this);
        value.accept(visitor);
    }

    template<typename T>
    void writeInternal(const std::vector<T>& values)
    {
        writeInternal(values.size());
        for (const T& value : values) {
            writeInternal(value);
        }
    }

};

} // namespace config

#endif // COMMON_CONFIG_TO_FDSTORE_VISITOR_HPP
