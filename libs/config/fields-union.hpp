/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Mateusz Malicki (m.malicki2@samsung.com)
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
 * @brief   Macros for declare configuration fields
 */

#ifndef COMMON_CONFIG_FIELDS_UNION_HPP
#define COMMON_CONFIG_FIELDS_UNION_HPP

#include "config/fields.hpp"
#include "config/exception.hpp"

#include <utility>
#include <boost/any.hpp>

/**
 * Use this macro to declare and register config fields
 *
 * Example:
 *  struct Foo {
 *      std::string bar;
 *
 *      CONFIG_REGISTER
 *      (
 *          bar,
 *      )
 *  };
 *
 *  struct Config
 *  {
 *      CONFIG_DECLARE_UNION
 *      (
 *          Foo,
 *          int
 *      )
 *  };
 *
 *  Example of valid configuration:
 *   1. {
 *        "type": "Foo",
 *        "value": { "bar": "some string" }
 *      }
 *   2. {
 *        "type": "int",
 *        "value": 1
 *      }
 *
 *
 *  Usage:
 *   Config config;
 *   // ...
 *   if (config.isSet()) {
 *      if (config.is<Foo>()) {
 *          Foo& foo = config.as<Foo>();
 *          // ...
 *      }
 *      if (config.is<int>())) {
 *          int field = config.as<int>();
 *          // ...
 *      }
 *   } else {
 *     // Config is not set
 *     Foo foo({"some string"});
 *     config.set(foo);
 *     config.set(std::move(foo));         //< copy sic!
 *     config.set(Foo({"some string"}));
 *   }
 */

class DisbaleMoveAnyWrapper : public boost::any
{
    public:
        DisbaleMoveAnyWrapper() {}
        DisbaleMoveAnyWrapper(const DisbaleMoveAnyWrapper& any)
            : boost::any(static_cast<const boost::any&>(any)) {};
        DisbaleMoveAnyWrapper& operator=(DisbaleMoveAnyWrapper&& any) = delete;
        DisbaleMoveAnyWrapper& operator=(const DisbaleMoveAnyWrapper& any) {
            static_cast<boost::any&>(*this) = static_cast<const boost::any&>(any);
            return *this;
        }
};

#define CONFIG_DECLARE_UNION(...)                                                               \
private:                                                                                        \
    DisbaleMoveAnyWrapper mConfigDeclareField;                                                  \
                                                                                                \
    template<typename Visitor>                                                                  \
    void visitOption(Visitor& v, const std::string& name) {                                     \
        GENERATE_CODE(GENERATE_UNION_VISIT__, __VA_ARGS__)                                      \
        throw config::ConfigException("Union type error. Unsupported type");                    \
    }                                                                                           \
    template<typename Visitor>                                                                  \
    void visitOption(Visitor& v, const std::string& name) const {                               \
        GENERATE_CODE(GENERATE_UNION_VISIT_CONST__, __VA_ARGS__)                                \
        throw config::ConfigException("Union type error. Unsupported type");                    \
    }                                                                                           \
    std::string getOptionName() const {                                                         \
        GENERATE_CODE(GENERATE_UNION_NAME__, __VA_ARGS__)                                       \
        return std::string();                                                                   \
    }                                                                                           \
    boost::any& getHolder() {                                                                   \
        return static_cast<boost::any&>(mConfigDeclareField);                                   \
    }                                                                                           \
    const boost::any& getHolder() const {                                                       \
        return static_cast<const boost::any&>(mConfigDeclareField);                             \
    }                                                                                           \
public:                                                                                         \
                                                                                                \
    template<typename Visitor>                                                                  \
    void accept(Visitor v) {                                                                    \
        std::string name;                                                                       \
        v.visit("type", name);                                                                  \
        visitOption(v, name);                                                                   \
    }                                                                                           \
                                                                                                \
    template<typename Visitor>                                                                  \
    void accept(Visitor v) const {                                                              \
        const std::string name = getOptionName();                                               \
        if (name.empty()) {                                                                     \
           throw config::ConfigException("Type is not set");                                    \
        }                                                                                       \
        v.visit("type", name);                                                                  \
        visitOption(v, name);                                                                   \
    }                                                                                           \
                                                                                                \
    template<typename Type>                                                                     \
    bool is() const {                                                                           \
        return boost::any_cast<Type>(&getHolder()) != NULL;                                     \
    }                                                                                           \
    template<typename Type>                                                                     \
    typename std::enable_if<!std::is_const<Type>::value, Type>::type& as() {                    \
        if (getHolder().empty()) {                                                              \
            throw config::ConfigException("Type is not set");                                   \
        }                                                                                       \
        return boost::any_cast<Type&>(getHolder());                                             \
    }                                                                                           \
    template<typename Type>                                                                     \
    const Type& as() const {                                                                    \
        if (getHolder().empty()) {                                                              \
            throw config::ConfigException("Type is not set");                                   \
        }                                                                                       \
        return boost::any_cast<const Type&>(getHolder());                                       \
    }                                                                                           \
    bool isSet() {                                                                              \
        return !getOptionName().empty();                                                        \
    }                                                                                           \
    template<typename Type>                                                                     \
    Type& set(const Type& src) {                                                                \
        getHolder() = std::forward<const Type>(src);                                            \
        if (getOptionName().empty()) {                                                          \
           throw config::ConfigException("Unsupported type");                                   \
        }                                                                                       \
        return as<Type>();                                                                      \
    }                                                                                           \

#define GENERATE_CODE(MACRO, ...)                                                               \
    BOOST_PP_LIST_FOR_EACH(MACRO, _, BOOST_PP_VARIADIC_TO_LIST(__VA_ARGS__))

#define GENERATE_UNION_VISIT__(r, _, TYPE_)                                                     \
    if (#TYPE_ == name) {                                                                       \
        v.visit("value", set(std::move(TYPE_())));                                              \
        return;                                                                                 \
    }

#define GENERATE_UNION_VISIT_CONST__(r, _, TYPE_)                                               \
    if (#TYPE_ == name) {                                                                       \
        v.visit("value", as<const TYPE_>());                                                    \
        return;                                                                                 \
    }

#define GENERATE_UNION_NAME__(r, _, TYPE_)                                                      \
    if (is<TYPE_>()) {                                                                          \
        return #TYPE_;                                                                          \
    }

#endif // COMMON_CONFIG_FIELDS_UNION_HPP