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
 * @brief   Unit tests of the ZoneProvision class
 */

#include "config.hpp"
#include "ut.hpp"

#include "utils/scoped-dir.hpp"
#include "utils/exception.hpp"
#include "config/manager.hpp"
#include "zone-provision.hpp"
#include "zone-provision-config.hpp"
#include "vasum-client.h"

#include <string>
#include <fstream>

#include <boost/filesystem.hpp>
#include <sys/mount.h>
#include <fcntl.h>

using namespace vasum;
using namespace config;

namespace fs = boost::filesystem;

namespace {

const std::string PROVISON_CONFIG_FILE = "provision.conf";
const std::string ZONE = "ut-zone-provision-test";
const fs::path ZONES_PATH = "/tmp/ut-zones";
const fs::path LXC_TEMPLATES_PATH = VSM_TEST_LXC_TEMPLATES_INSTALL_DIR;
const fs::path ZONE_PATH = ZONES_PATH / fs::path(ZONE);
const fs::path PROVISION_FILE_PATH = ZONE_PATH / fs::path(PROVISON_CONFIG_FILE);
const fs::path ROOTFS_PATH = ZONE_PATH / fs::path("rootfs");

struct Fixture {
    utils::ScopedDir mZonesPathGuard;
    utils::ScopedDir mRootfsPath;

    Fixture()
        : mZonesPathGuard(ZONES_PATH.string())
        , mRootfsPath(ROOTFS_PATH.string())
    {
    }
};

} // namespace


BOOST_FIXTURE_TEST_SUITE(ZoneProvisionSuite, Fixture)

BOOST_AUTO_TEST_CASE(DestructorTest)
{
    const fs::path mountTarget = fs::path("/opt/usr/data/ut-from-host-provision");
    const fs::path mountSource = fs::path("/tmp/ut-provision");
    {
        utils::ScopedDir provisionfs(mountSource.string());

        ZoneProvisioning config;
        ZoneProvisioning::Unit unit;
        unit.set(ZoneProvisioning::File({VSMFILE_DIRECTORY,
                                        mountTarget.string(),
                                        0,
                                        0777}));
        config.units.push_back(unit);
        unit.set(ZoneProvisioning::Mount({mountSource.string(),
                                          mountTarget.string(),
                                          "",
                                          MS_BIND,
                                          ""}));
        config.units.push_back(unit);

        config::saveToFile(PROVISION_FILE_PATH.string(), config);
        ZoneProvision zoneProvision(ZONE_PATH.string(), {});
        zoneProvision.start();
    }
    BOOST_CHECK(!fs::exists(mountSource));
}

BOOST_AUTO_TEST_CASE(FileTest)
{
    //TODO: Test Fifo
    const fs::path regularFile = fs::path("/opt/usr/data/ut-regular-file");
    const fs::path copyFile = PROVISION_FILE_PATH;

    ZoneProvisioning config;
    ZoneProvisioning::Unit unit;
    unit.set(ZoneProvisioning::File({VSMFILE_DIRECTORY,
                                    regularFile.parent_path().string(),
                                    0,
                                    0777}));
    config.units.push_back(unit);

    unit.set(ZoneProvisioning::File({VSMFILE_REGULAR,
                                    regularFile.string(),
                                    O_CREAT,
                                    0777}));
    config.units.push_back(unit);

    unit.set(ZoneProvisioning::File({VSMFILE_DIRECTORY,
                                    copyFile.parent_path().string(),
                                    0,
                                    0777}));
    config.units.push_back(unit);
    unit.set(ZoneProvisioning::File({VSMFILE_REGULAR,
                                    copyFile.string(),
                                    0,
                                    0777}));
    config.units.push_back(unit);
    config::saveToFile(PROVISION_FILE_PATH.string(), config);

    ZoneProvision zoneProvision(ZONE_PATH.string(), {});
    zoneProvision.start();

    BOOST_CHECK(fs::exists(ROOTFS_PATH / regularFile.parent_path()));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / regularFile));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / copyFile.parent_path()));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / copyFile));

    zoneProvision.stop();
}

BOOST_AUTO_TEST_CASE(MountTest)
{
    //TODO: Test Fifo
    const fs::path mountTarget = fs::path("/opt/usr/data/ut-from-host-provision");
    const fs::path mountSource = fs::path("/tmp/ut-provision");
    const fs::path sharedFile = fs::path("ut-regular-file");

    utils::ScopedDir provisionfs(mountSource.string());


    ZoneProvisioning config;
    ZoneProvisioning::Unit unit;
    unit.set(ZoneProvisioning::File({VSMFILE_DIRECTORY,
                                    mountTarget.string(),
                                    0,
                                    0777}));
    config.units.push_back(unit);
    unit.set(ZoneProvisioning::Mount({mountSource.string(),
                                      mountTarget.string(),
                                      "",
                                      MS_BIND,
                                      ""}));
    config.units.push_back(unit);
    unit.set(ZoneProvisioning::File({VSMFILE_REGULAR,
                                    (mountTarget / sharedFile).string(),
                                    O_CREAT,
                                    0777}));
    config.units.push_back(unit);
    config::saveToFile(PROVISION_FILE_PATH.string(), config);

    ZoneProvision zoneProvision(ZONE_PATH.string(), {});
    zoneProvision.start();

    BOOST_CHECK(fs::exists(ROOTFS_PATH / mountTarget));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / mountTarget / sharedFile));
    BOOST_CHECK(fs::exists(mountSource / sharedFile));

    zoneProvision.stop();
}

BOOST_AUTO_TEST_CASE(LinkTest)
{
    const fs::path linkFile = fs::path("/ut-from-host-provision.conf");

    ZoneProvisioning config;
    ZoneProvisioning::Unit unit;

    unit.set(ZoneProvisioning::Link({PROVISION_FILE_PATH.string(),
                                     linkFile.string()}));
    config.units.push_back(unit);
    config::saveToFile(PROVISION_FILE_PATH.string(), config);
    {
        ZoneProvision zoneProvision(ZONE_PATH.string(), {});
        zoneProvision.start();

        BOOST_CHECK(!fs::exists(ROOTFS_PATH / linkFile));

        zoneProvision.stop();
    }
    {
        ZoneProvision zoneProvision(ZONE_PATH.string(), {"/tmp/"});
        zoneProvision.start();

        BOOST_CHECK(fs::exists(ROOTFS_PATH / linkFile));

        zoneProvision.stop();
    }
}

BOOST_AUTO_TEST_CASE(DeclareFileTest)
{
    ZoneProvision zoneProvision(ZONE_PATH.string(), {});
    zoneProvision.declareFile(1, "path", 0747, 0777);
    zoneProvision.declareFile(2, "path", 0747, 0777);

    ZoneProvisioning config;
    BOOST_REQUIRE_NO_THROW(loadFromFile(PROVISION_FILE_PATH.string(), config));
    BOOST_REQUIRE_EQUAL(config.units.size(), 2);
    BOOST_REQUIRE(config.units[0].is<ZoneProvisioning::File>());
    BOOST_REQUIRE(config.units[1].is<ZoneProvisioning::File>());
    const ZoneProvisioning::File& unit = config.units[0].as<ZoneProvisioning::File>();
    BOOST_CHECK_EQUAL(unit.type, 1);
    BOOST_CHECK_EQUAL(unit.path, "path");
    BOOST_CHECK_EQUAL(unit.flags, 0747);
    BOOST_CHECK_EQUAL(unit.mode, 0777);
}

BOOST_AUTO_TEST_CASE(DeclareMountTest)
{
    ZoneProvision zoneProvision(ZONE_PATH.string(), {});
    zoneProvision.declareMount("/fake/path1", "/fake/path2", "tmpfs", 077, "fake");
    zoneProvision.declareMount("/fake/path2", "/fake/path2", "tmpfs", 077, "fake");

    ZoneProvisioning config;
    BOOST_REQUIRE_NO_THROW(loadFromFile(PROVISION_FILE_PATH.string(), config));
    BOOST_REQUIRE_EQUAL(config.units.size(), 2);
    BOOST_REQUIRE(config.units[0].is<ZoneProvisioning::Mount>());
    BOOST_REQUIRE(config.units[1].is<ZoneProvisioning::Mount>());
    const ZoneProvisioning::Mount& unit = config.units[0].as<ZoneProvisioning::Mount>();
    BOOST_CHECK_EQUAL(unit.source, "/fake/path1");
    BOOST_CHECK_EQUAL(unit.target, "/fake/path2");
    BOOST_CHECK_EQUAL(unit.type, "tmpfs");
    BOOST_CHECK_EQUAL(unit.flags, 077);
    BOOST_CHECK_EQUAL(unit.data, "fake");
}

BOOST_AUTO_TEST_CASE(DeclareLinkTest)
{
    ZoneProvision zoneProvision(ZONE_PATH.string(), {});
    zoneProvision.declareLink("/fake/path1", "/fake/path2");
    zoneProvision.declareLink("/fake/path2", "/fake/path2");

    ZoneProvisioning config;
    BOOST_REQUIRE_NO_THROW(loadFromFile(PROVISION_FILE_PATH.string(), config));
    BOOST_REQUIRE_EQUAL(config.units.size(), 2);
    BOOST_REQUIRE(config.units[0].is<ZoneProvisioning::Link>());
    BOOST_REQUIRE(config.units[1].is<ZoneProvisioning::Link>());
    const ZoneProvisioning::Link& unit = config.units[0].as<ZoneProvisioning::Link>();
    BOOST_CHECK_EQUAL(unit.source, "/fake/path1");
    BOOST_CHECK_EQUAL(unit.target, "/fake/path2");
}

BOOST_AUTO_TEST_CASE(ProvisionedAlreadyTest)
{
    const fs::path dir = fs::path("/opt/usr/data/ut-from-host-provision");
    const fs::path linkFile = fs::path("/ut-from-host-provision.conf");
    const fs::path regularFile = fs::path("/opt/usr/data/ut-regular-file");

    ZoneProvisioning config;
    ZoneProvisioning::Unit unit;
    unit.set(ZoneProvisioning::File({VSMFILE_DIRECTORY,
                                    dir.string(),
                                    0,
                                    0777}));
    config.units.push_back(unit);
    unit.set(ZoneProvisioning::Link({PROVISION_FILE_PATH.string(),
                                     linkFile.string()}));
    config.units.push_back(unit);
    unit.set(ZoneProvisioning::File({VSMFILE_REGULAR,
                                    regularFile.string(),
                                    O_CREAT,
                                    0777}));
    config.units.push_back(unit);

    config::saveToFile(PROVISION_FILE_PATH.string(), config);

    ZoneProvision zoneProvision(ZONE_PATH.string(), {"/tmp/"});
    zoneProvision.start();

    BOOST_CHECK(fs::exists(ROOTFS_PATH / dir));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / regularFile));
    BOOST_CHECK(fs::is_empty(ROOTFS_PATH / regularFile));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / linkFile));

    std::fstream file((ROOTFS_PATH / regularFile).string(), std::fstream::out);
    BOOST_REQUIRE(file.is_open());
    file << "touch" << std::endl;
    file.close();
    BOOST_REQUIRE(!fs::is_empty(ROOTFS_PATH / regularFile));

    zoneProvision.stop();

    BOOST_CHECK(fs::exists(ROOTFS_PATH / dir));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / regularFile));
    BOOST_CHECK(!fs::is_empty(ROOTFS_PATH / regularFile));
    BOOST_CHECK(fs::exists(ROOTFS_PATH / linkFile));

    zoneProvision.start();

    BOOST_CHECK(!fs::is_empty(ROOTFS_PATH / regularFile));

    zoneProvision.stop();
}

BOOST_AUTO_TEST_SUITE_END()