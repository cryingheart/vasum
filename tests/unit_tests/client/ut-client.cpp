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
 * @brief   Unit tests of the client C API
 */

#include "config.hpp"

#include "ut.hpp"
#include "vasum-client.h"
#include "utils/latch.hpp"
#include "utils/scoped-dir.hpp"
#include "zones-manager.hpp"
#include "host-ipc-connection.hpp"
#include "host-ipc-definitions.hpp"
#include "cargo-ipc/epoll/thread-dispatcher.hpp"
#include "logger/logger.hpp"

#ifdef DBUS_CONNECTION
#include "utils/glib-loop.hpp"
#endif //DBUS_CONNECTION

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <map>
#include <string>
#include <utility>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

#include <sys/epoll.h>

using namespace vasum;
using namespace utils;

namespace fs = boost::filesystem;

namespace {

const std::string TEST_CONFIG_PATH =
    VSM_TEST_CONFIG_INSTALL_DIR "/test-daemon.conf";
const std::string ZONES_PATH = "/tmp/ut-zones"; // the same as in daemon.conf
const std::string TEMPLATE_NAME = "console-ipc";
const int EVENT_TIMEOUT = 500; // ms

struct Fixture {
    utils::ScopedDir mZonesPathGuard;
    utils::ScopedDir mRunGuard;
    cargo::ipc::epoll::ThreadDispatcher mDispatcher;
#ifdef DBUS_CONNECTION
    utils::ScopedGlibLoop mLoop;
#endif //DBUS_CONNECTION

    std::unique_ptr<ZonesManager> cm;

    Fixture()
        : mZonesPathGuard(ZONES_PATH)
        , mRunGuard("/tmp/ut-run")
        , cm(new ZonesManager(mDispatcher.getPoll(), TEST_CONFIG_PATH))
    {
        cm->start();
        cm->createZone("zone1", TEMPLATE_NAME);
        cm->createZone("zone2", TEMPLATE_NAME);
        cm->createZone("zone3", TEMPLATE_NAME);
        cm->restoreAll();
        LOGI("------- setup complete --------");
    }

    ~Fixture()
    {
        LOGI("------- cleanup --------");
    }
};

class SimpleEventLoop {
public:
    SimpleEventLoop(VsmClient client)
        : mClient(client)
        , mIsProcessing(true)
        , mThread([this] { loop(); })
    {
    }
    ~SimpleEventLoop()
    {
        mIsProcessing = false;
        mThread.join();
    }

private:
    VsmClient mClient;
    std::atomic_bool mIsProcessing;
    std::thread mThread;
    void loop() {
        while (mIsProcessing) {
            vsm_enter_eventloop(mClient, 0, EVENT_TIMEOUT);
        }
    }
};

class AggragatedEventLoop {
public:
    AggragatedEventLoop()
        : mIsProcessing(true)
        , mThread([this] { loop(); })
    {
    }

    ~AggragatedEventLoop()
    {
        mIsProcessing = false;
        mThread.join();
    }

    VsmStatus addEventSource(VsmClient client) {
        int fd = -1;
        VsmStatus ret = vsm_get_poll_fd(client, &fd);
        if (VSMCLIENT_SUCCESS != ret) {
            return ret;
        }
        mEventPoll.addFD(fd, EPOLLIN | EPOLLHUP | EPOLLRDHUP, [client] (int, cargo::ipc::epoll::Events) {
            vsm_enter_eventloop(client, 0, 0);
        });
        mFds.push_back(fd);
        return VSMCLIENT_SUCCESS;
    }
private:
    std::atomic_bool mIsProcessing;
    std::thread mThread;
    cargo::ipc::epoll::EventPoll mEventPoll;
    std::vector<int> mFds;

    void loop() {
        while (mIsProcessing) {
            mEventPoll.dispatchIteration(EVENT_TIMEOUT);
        }
        for (int fd : mFds) {
            mEventPoll.removeFD(fd);
        }
    }
};

const std::set<std::string> EXPECTED_ZONES = { "zone1", "zone2", "zone3" };

void convertArrayToSet(VsmArrayString values, std::set<std::string>& ret)
{
    for (VsmArrayString iValues = values; *iValues; iValues++) {
        ret.insert(*iValues);
    }
}

int getArrayStringLength(VsmArrayString astring, int max_len = -1)
{
    int i = 0;
    for (i = 0; astring[i];  i++) {
        if (i == max_len) {
            return max_len;
        }
    }
    return i;
}

VsmStatus makeSimpleRequest(VsmClient client)
{
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_connect(client));
    //make a simple call
    VsmString zone = NULL;
    VsmStatus status = vsm_get_active_zone_id(client, &zone);
    vsm_string_free(zone);
    // disconnect but not destroy
    vsm_disconnect(client);
    return status;
}

} // namespace

// make nice BOOST_*_EQUAL output
// (does not work inside anonymous namespace)
std::ostream& operator<<(std::ostream& out, VsmStatus status)
{
    switch(status) {
    case VSMCLIENT_CUSTOM_ERROR: return out << "CUSTOM_ERROR";
    case VSMCLIENT_IO_ERROR: return out << "IO_ERROR";
    case VSMCLIENT_OPERATION_FAILED: return out << "OPERATION_FAILED";
    case VSMCLIENT_INVALID_ARGUMENT: return out << "INVALID_ARGUMENT";
    case VSMCLIENT_OTHER_ERROR: return out << "OTHER_ERROR";
    case VSMCLIENT_SUCCESS: return out << "SUCCESS";
    default: return out << "UNKNOWN(" << (int)status << ")";
    };
}

BOOST_FIXTURE_TEST_SUITE(ClientSuite, Fixture)

BOOST_AUTO_TEST_CASE(NotRunningServer)
{
    cm.reset();

    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_CHECK_EQUAL(VSMCLIENT_IO_ERROR, status);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetZoneIds)
{
    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    VsmArrayString values;
    status = vsm_get_zone_ids(client, &values);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    BOOST_CHECK_EQUAL(getArrayStringLength(values, EXPECTED_ZONES.size() + 1u),
                      EXPECTED_ZONES.size());

    std::set<std::string> zones;
    convertArrayToSet(values, zones);

    for (const auto& zone : zones) {
        BOOST_CHECK(EXPECTED_ZONES.find(zone) != EXPECTED_ZONES.cend());
    }
    vsm_array_string_free(values);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetActiveZoneId)
{
    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    VsmString zone;
    status = vsm_get_active_zone_id(client, &zone);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);

    BOOST_CHECK_EQUAL(zone, cm->getRunningForegroundZoneId());

    vsm_string_free(zone);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(LookupZoneById)
{
    const std::string activeZoneId = "zone1";

    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    VsmZone info;
    status = vsm_lookup_zone_by_id(client, activeZoneId.c_str(), &info);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);

    BOOST_CHECK_EQUAL(vsm_zone_get_id(info), activeZoneId);
    BOOST_CHECK_EQUAL(vsm_zone_get_state(info), RUNNING);
    BOOST_CHECK_EQUAL(vsm_zone_get_terminal(info), -1);
    BOOST_CHECK_EQUAL(vsm_zone_get_rootfs(info), "/tmp/ut-zones/" + activeZoneId + "/rootfs");

    vsm_zone_free(info);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(SetActiveZone)
{
    const std::string newActiveZoneId = "zone2";

    BOOST_REQUIRE_NE(newActiveZoneId, cm->getRunningForegroundZoneId());

    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    status = vsm_set_active_zone(client, newActiveZoneId.c_str());
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    BOOST_CHECK_EQUAL(newActiveZoneId, cm->getRunningForegroundZoneId());
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(CreateZone)
{
    const std::string newActiveZoneId = "";

    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    status = vsm_create_zone(client, newActiveZoneId.c_str(), NULL);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_CUSTOM_ERROR, status);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(StartShutdownZone)
{
    const std::string newActiveZoneId = "zone1";

    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    status = vsm_shutdown_zone(client, newActiveZoneId.c_str());
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    status = vsm_start_zone(client, newActiveZoneId.c_str());
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(LockUnlockZone)
{
    const std::string newActiveZoneId = "zone2";

    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    status = vsm_lock_zone(client, newActiveZoneId.c_str());
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    status = vsm_unlock_zone(client, newActiveZoneId.c_str());
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetZoneIdByPidTestSingle)
{
    VsmClient client = vsm_client_create();
    VsmString zone;
    VsmStatus status = vsm_lookup_zone_by_pid(client, 1, &zone);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);

    BOOST_CHECK_EQUAL(zone, std::string("host"));

    vsm_string_free(zone);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetZoneIdByPidTestMultiple)
{
    std::vector<int> pids;
    fs::directory_iterator end_iter;

    for (fs::directory_iterator iter("/proc"); iter != end_iter; ++iter) {
        const std::string name = iter->path().filename().string();
        try {
            if (fs::is_directory(iter->status()) && std::isdigit(name[0])) {
                pids.push_back(std::stoi(name));
            }
        } catch (...) {
            LOGE(name << "is not a process ");
        }
    }

    std::set<std::string> ids;
    VsmClient client = vsm_client_create();

    for (int n : pids) {
        VsmString zone;
        VsmStatus status = vsm_lookup_zone_by_pid(client, n, &zone);
        if (status == VSMCLIENT_SUCCESS) {
            ids.insert(zone);
            vsm_string_free(zone);
        } else {
            BOOST_WARN_MESSAGE(status == VSMCLIENT_INVALID_ARGUMENT, vsm_get_status_message(client));
        }
    }
    vsm_client_free(client);

    BOOST_CHECK(ids.count("host") == 1);

    for (const auto& zone : EXPECTED_ZONES) {
        BOOST_CHECK_MESSAGE(ids.count(zone) == 1, zone + ".count=" + std::to_string(ids.count(zone)));
    }
}

BOOST_AUTO_TEST_CASE(GrantRevoke)
{
    const std::string zoneId = "zone2";
    const std::string dev = "tty3";

    VsmClient client = vsm_client_create();
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_connect(client));

    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, vsm_grant_device(client, zoneId.c_str(), dev.c_str(), 0));
    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, vsm_revoke_device(client, zoneId.c_str(), dev.c_str()));

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_lock_zone(client, zoneId.c_str()));
    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, vsm_grant_device(client, zoneId.c_str(), dev.c_str(), 0));
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_unlock_zone(client, zoneId.c_str()));

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_shutdown_zone(client, zoneId.c_str()));
    BOOST_CHECK_EQUAL(VSMCLIENT_CUSTOM_ERROR, vsm_grant_device(client, zoneId.c_str(), dev.c_str(), 0));
    BOOST_CHECK_EQUAL(VSMCLIENT_CUSTOM_ERROR, vsm_revoke_device(client, zoneId.c_str(), dev.c_str()));

    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(Provision)
{
    VsmClient client = vsm_client_create();
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_connect(client));
    const std::string zone = cm->getRunningForegroundZoneId();
    VsmArrayString declarations;
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_list_declarations(client, zone.c_str(), &declarations));
    BOOST_REQUIRE(declarations != NULL && declarations[0] == NULL);
    vsm_array_string_free(declarations);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_declare_link(client, "/tmp/fake", zone.c_str(), "/tmp/fake/"));
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_list_declarations(client, zone.c_str(), &declarations));
    BOOST_REQUIRE(declarations != NULL && declarations[0] != NULL && declarations[1] == NULL);
    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, vsm_remove_declaration(client, zone.c_str(), declarations[0]));
    vsm_array_string_free(declarations);
    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, vsm_list_declarations(client, zone.c_str(), &declarations));
    BOOST_CHECK(declarations != NULL && declarations[0] == NULL);
    vsm_array_string_free(declarations);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(ZoneGetNetdevs)
{
    const std::string activeZoneId = "zone1";

    VsmClient client = vsm_client_create();
    VsmStatus status = vsm_connect(client);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    VsmArrayString netdevs;
    status = vsm_zone_get_netdevs(client, activeZoneId.c_str(), &netdevs);
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, status);
    BOOST_REQUIRE(netdevs != NULL);
    vsm_array_string_free(netdevs);
    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(DefaultDispatcher)
{
    VsmClient client = vsm_client_create();

    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, makeSimpleRequest(client));

    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(SetDispatcher)
{
    VsmClient client = vsm_client_create();

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_set_dispatcher_type(client, VSMDISPATCHER_INTERNAL));
    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, makeSimpleRequest(client));

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_set_dispatcher_type(client, VSMDISPATCHER_EXTERNAL));
    {
        SimpleEventLoop loop(client);
        BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, makeSimpleRequest(client));
    }

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_set_dispatcher_type(client, VSMDISPATCHER_INTERNAL));
    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, makeSimpleRequest(client));

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_set_dispatcher_type(client, VSMDISPATCHER_EXTERNAL));
    {
        SimpleEventLoop loop(client);
        BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, makeSimpleRequest(client));
    }

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_set_dispatcher_type(client, VSMDISPATCHER_INTERNAL));
    BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, makeSimpleRequest(client));

    vsm_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetPollFd)
{
    VsmClient client1 = vsm_client_create();
    VsmClient client2 = vsm_client_create();

    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_set_dispatcher_type(client1, VSMDISPATCHER_EXTERNAL));
    BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_set_dispatcher_type(client2, VSMDISPATCHER_EXTERNAL));
    {
        AggragatedEventLoop loop;
        BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, loop.addEventSource(client1));
        BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, loop.addEventSource(client2));

        BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_connect(client1));
        BOOST_REQUIRE_EQUAL(VSMCLIENT_SUCCESS, vsm_connect(client2));

        VsmString zone;
        //make a simple call
        zone = NULL;
        BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, vsm_get_active_zone_id(client1, &zone));
        vsm_string_free(zone);
        zone = NULL;
        BOOST_CHECK_EQUAL(VSMCLIENT_SUCCESS, vsm_get_active_zone_id(client2, &zone));
        vsm_string_free(zone);

        // disconnect but not destroy
        vsm_disconnect(client1);
        vsm_disconnect(client2);
    }
    vsm_client_free(client1);
    vsm_client_free(client2);
}

//TODO: We need createBridge from vasum::netdev
//BOOST_AUTO_TEST_CASE(CreateDestroyNetdev)
//BOOST_AUTO_TEST_CASE(LookupNetdev)
//BOOST_AUTO_TEST_CASE(GetSetDeleteIpAddr)
//BOOST_AUTO_TEST_CASE(NetdevUpDown)



BOOST_AUTO_TEST_SUITE_END()
