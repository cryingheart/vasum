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
 * @brief   Unit tests of SignalFD
 */

#include "config.hpp"
#include "ut.hpp"

#include "utils/signalfd.hpp"
#include "ipc/epoll/event-poll.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <csignal>


using namespace utils;

namespace {

volatile sig_atomic_t gAsyncSignal;

struct Fixture {
    Fixture()
    {
        gAsyncSignal = 0;
        ::signal(SIGINT, &Fixture::signalHandler);
    }
    ~Fixture()
    {
        ::signal(SIGINT, SIG_DFL);
        gAsyncSignal = 0;
    }

    static void signalHandler(int s)
    {
        gAsyncSignal = s;
    }

    static bool isAsyncHandlerCalled() {
        return gAsyncSignal != 0;
    }
};


} // namespace

BOOST_FIXTURE_TEST_SUITE(SignalFDSuite, Fixture)

const int TIMEOUT = 100;

BOOST_AUTO_TEST_CASE(ConstructorDesctructor)
{
    ipc::epoll::EventPoll poll;
    SignalFD s(poll);
}

BOOST_AUTO_TEST_CASE(BlockingSignalHandler)
{
    ipc::epoll::EventPoll poll;
    SignalFD s(poll);
    s.setHandlerAndBlock(SIGUSR1, [](const int) {});
    s.setHandlerAndBlock(SIGINT, [](const int) {});

    ::raise(SIGINT);
    std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
    BOOST_REQUIRE(!isAsyncHandlerCalled());
}

BOOST_AUTO_TEST_CASE(SignalHandler)
{
    ipc::epoll::EventPoll poll;
    SignalFD s(poll);

    bool isSignalCalled = false;
    s.setHandler(SIGINT, [&](const int) {
        isSignalCalled = true;
    });

    ::raise(SIGINT);
    poll.dispatchIteration(TIMEOUT);
    BOOST_REQUIRE(isSignalCalled);
    BOOST_REQUIRE(!isAsyncHandlerCalled());
}

BOOST_AUTO_TEST_SUITE_END()