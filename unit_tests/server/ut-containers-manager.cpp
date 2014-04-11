/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
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
 * @brief   Unit tests of the ContainersManager class
 */

#include "ut.hpp"

#include "containers-manager.hpp"
#include "exception.hpp"

#include <memory>
#include <string>


BOOST_AUTO_TEST_SUITE(ContainersManagerSuite)

using namespace security_containers;
using namespace security_containers::config;

const std::string TEST_CONFIG_PATH = "/etc/security-containers/tests/server/ut-containers-manager/test-daemon.conf";
const std::string BUGGY_CONFIG_PATH = "/etc/security-containers/tests/server/ut-containers-manager/buggy-daemon.conf";
const std::string BUGGY_FOREGROUND_CONFIG_PATH = "/etc/security-containers/tests/server/ut-containers-manager/buggy-foreground-daemon.conf";
const std::string MISSING_CONFIG_PATH = "/this/is/a/missing/file/path/missing-daemon.conf";


BOOST_AUTO_TEST_CASE(ConstructorTest)
{
    BOOST_REQUIRE_NO_THROW(ContainersManager cm(TEST_CONFIG_PATH););
}

BOOST_AUTO_TEST_CASE(DestructorTest)
{
    std::unique_ptr<ContainersManager> cm(new ContainersManager(TEST_CONFIG_PATH));
    BOOST_REQUIRE_NO_THROW(cm.reset());
}

BOOST_AUTO_TEST_CASE(BuggyConfigTest)
{
    BOOST_REQUIRE_THROW(ContainersManager cm(BUGGY_CONFIG_PATH), ConfigException);
}

BOOST_AUTO_TEST_CASE(MissingConfigTest)
{
    BOOST_REQUIRE_THROW(ContainersManager cm(MISSING_CONFIG_PATH), ConfigException);
}

BOOST_AUTO_TEST_CASE(StartAllTest)
{
    ContainersManager cm(TEST_CONFIG_PATH);
    BOOST_REQUIRE_NO_THROW(cm.startAll());
    BOOST_CHECK(cm.getRunningForegroundContainerId() == "console1");
}

BOOST_AUTO_TEST_CASE(BuggyForegroundTest)
{
    ContainersManager cm(BUGGY_FOREGROUND_CONFIG_PATH);
    BOOST_REQUIRE_NO_THROW(cm.startAll());
    BOOST_CHECK(cm.getRunningForegroundContainerId() == "console2");
}

BOOST_AUTO_TEST_CASE(StopAllTest)
{
    ContainersManager cm(TEST_CONFIG_PATH);
    BOOST_REQUIRE_NO_THROW(cm.startAll());
    BOOST_REQUIRE_NO_THROW(cm.stopAll());
    BOOST_CHECK(cm.getRunningForegroundContainerId().empty());
}

BOOST_AUTO_TEST_CASE(FocusTest)
{
    ContainersManager cm(TEST_CONFIG_PATH);
    BOOST_REQUIRE_NO_THROW(cm.startAll());
    BOOST_REQUIRE_NO_THROW(cm.focus("console2"));
    BOOST_CHECK(cm.getRunningForegroundContainerId() == "console2");
    BOOST_REQUIRE_NO_THROW(cm.focus("console1"));
    BOOST_CHECK(cm.getRunningForegroundContainerId() == "console1");
    BOOST_REQUIRE_NO_THROW(cm.focus("console3"));
    BOOST_CHECK(cm.getRunningForegroundContainerId() == "console3");
}

BOOST_AUTO_TEST_SUITE_END()