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
 * @brief   Tests of the IPC
 */

// TODO: Test connection limit
// TODO: Refactor tests - function for setting up env


#include "config.hpp"
#include "ut.hpp"

#include "ipc/service.hpp"
#include "ipc/client.hpp"
#include "ipc/types.hpp"
#include "ipc/result.hpp"
#include "utils/glib-loop.hpp"
#include "utils/latch.hpp"
#include "utils/value-latch.hpp"

#include "config/fields.hpp"
#include "logger/logger.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <chrono>
#include <utility>
#include <boost/filesystem.hpp>

using namespace vasum;
using namespace vasum::ipc;
using namespace vasum::utils;
using namespace std::placeholders;
namespace fs = boost::filesystem;

namespace {

// Timeout for sending one message
const int TIMEOUT = 1000 /*ms*/;

// Time that won't cause "TIMEOUT" methods to throw
const int SHORT_OPERATION_TIME = TIMEOUT / 100;

// Time that will cause "TIMEOUT" methods to throw
const int LONG_OPERATION_TIME = 1000 + TIMEOUT;

struct Fixture {
    std::string socketPath;

    Fixture()
        : socketPath(fs::unique_path("/tmp/ipc-%%%%.socket").string())
    {
    }
    ~Fixture()
    {
        fs::remove(socketPath);
    }
};

struct SendData {
    int intVal;
    SendData(int i): intVal(i) {}

    CONFIG_REGISTER
    (
        intVal
    )
};

struct RecvData {
    int intVal;
    RecvData(): intVal(-1) {}

    CONFIG_REGISTER
    (
        intVal
    )
};

struct LongSendData {
    LongSendData(int i, int waitTime): mSendData(i), mWaitTime(waitTime), intVal(i) {}

    template<typename Visitor>
    void accept(Visitor visitor)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(mWaitTime));
        mSendData.accept(visitor);
    }
    template<typename Visitor>
    void accept(Visitor visitor) const
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(mWaitTime));
        mSendData.accept(visitor);
    }

    SendData mSendData;
    int mWaitTime;
    int intVal;
};

struct EmptyData {
    CONFIG_REGISTER_EMPTY
};

struct ThrowOnAcceptData {
    template<typename Visitor>
    void accept(Visitor)
    {
        throw std::runtime_error("intentional failure in accept");
    }
    template<typename Visitor>
    void accept(Visitor) const
    {
        throw std::runtime_error("intentional failure in accept const");
    }
};

std::shared_ptr<EmptyData> returnEmptyCallback(const FileDescriptor, std::shared_ptr<EmptyData>&)
{
    return std::make_shared<EmptyData>();
}

std::shared_ptr<SendData> returnDataCallback(const FileDescriptor, std::shared_ptr<RecvData>&)
{
    return std::make_shared<SendData>(1);
}

std::shared_ptr<SendData> echoCallback(const FileDescriptor, std::shared_ptr<RecvData>& data)
{
    return std::make_shared<SendData>(data->intVal);
}

std::shared_ptr<SendData> longEchoCallback(const FileDescriptor, std::shared_ptr<RecvData>& data)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(LONG_OPERATION_TIME));
    return std::make_shared<SendData>(data->intVal);
}

FileDescriptor connect(Service& s, Client& c, bool isServiceGlib = false, bool isClientGlib = false)
{
    // Connects the Client to the Service and returns Clients FileDescriptor
    ValueLatch<FileDescriptor> peerFDLatch;
    auto newPeerCallback = [&peerFDLatch](const FileDescriptor newFD) {
        peerFDLatch.set(newFD);
    };

    s.setNewPeerCallback(newPeerCallback);

    if (!s.isStarted()) {
        s.start(isServiceGlib);
    }

    c.start(isClientGlib);

    FileDescriptor peerFD = peerFDLatch.get(TIMEOUT);
    s.setNewPeerCallback(nullptr);
    BOOST_REQUIRE_NE(peerFD, 0);
    return peerFD;
}

FileDescriptor connectServiceGSource(Service& s, Client& c)
{
    return connect(s, c, true, false);
}

FileDescriptor connectClientGSource(Service& s, Client& c)
{
    return connect(s, c, false, true);
}

void testEcho(Client& c, const MethodID methodID)
{
    std::shared_ptr<SendData> sentData(new SendData(34));
    std::shared_ptr<RecvData> recvData = c.callSync<SendData, RecvData>(methodID, sentData, TIMEOUT);
    BOOST_REQUIRE(recvData);
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

void testEcho(Service& s, const MethodID methodID, const FileDescriptor peerFD)
{
    std::shared_ptr<SendData> sentData(new SendData(56));
    std::shared_ptr<RecvData> recvData = s.callSync<SendData, RecvData>(methodID, peerFD, sentData, TIMEOUT);
    BOOST_REQUIRE(recvData);
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

} // namespace


BOOST_FIXTURE_TEST_SUITE(IPCSuite, Fixture)

BOOST_AUTO_TEST_CASE(ConstructorDestructor)
{
    Service s(socketPath);
    Client c(socketPath);
}

BOOST_AUTO_TEST_CASE(ServiceAddRemoveMethod)
{
    Service s(socketPath);
    s.setMethodHandler<EmptyData, EmptyData>(1, returnEmptyCallback);
    s.setMethodHandler<SendData, RecvData>(1, returnDataCallback);

    s.start();

    s.setMethodHandler<SendData, RecvData>(1, echoCallback);
    s.setMethodHandler<SendData, RecvData>(2, returnDataCallback);

    Client c(socketPath);
    connect(s, c);
    testEcho(c, 1);

    s.removeMethod(1);
    s.removeMethod(2);

    BOOST_CHECK_THROW(testEcho(c, 2), IPCException);
}

BOOST_AUTO_TEST_CASE(ClientAddRemoveMethod)
{
    Service s(socketPath);
    Client c(socketPath);
    c.setMethodHandler<EmptyData, EmptyData>(1, returnEmptyCallback);
    c.setMethodHandler<SendData, RecvData>(1, returnDataCallback);

    FileDescriptor peerFD = connect(s, c);

    c.setMethodHandler<SendData, RecvData>(1, echoCallback);
    c.setMethodHandler<SendData, RecvData>(2, returnDataCallback);

    testEcho(s, 1, peerFD);

    c.removeMethod(1);
    c.removeMethod(2);

    BOOST_CHECK_THROW(testEcho(s, 1, peerFD), IPCException);
}

BOOST_AUTO_TEST_CASE(ServiceStartStop)
{
    Service s(socketPath);

    s.setMethodHandler<SendData, RecvData>(1, returnDataCallback);

    s.start();
    s.stop();
    s.start();
    s.stop();

    s.start();
    s.start();
}

BOOST_AUTO_TEST_CASE(ClientStartStop)
{
    Service s(socketPath);
    Client c(socketPath);
    c.setMethodHandler<SendData, RecvData>(1, returnDataCallback);

    c.start();
    c.stop();
    c.start();
    c.stop();

    c.start();
    c.start();

    c.stop();
    c.stop();
}

BOOST_AUTO_TEST_CASE(SyncClientToServiceEcho)
{
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, echoCallback);
    s.setMethodHandler<SendData, RecvData>(2, echoCallback);

    Client c(socketPath);
    connect(s, c);

    testEcho(c, 1);
    testEcho(c, 2);
}

BOOST_AUTO_TEST_CASE(Restart)
{
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, echoCallback);
    s.start();
    s.setMethodHandler<SendData, RecvData>(2, echoCallback);

    Client c(socketPath);
    c.start();
    testEcho(c, 1);
    testEcho(c, 2);

    c.stop();
    c.start();

    testEcho(c, 1);
    testEcho(c, 2);

    s.stop();
    s.start();

    testEcho(c, 1);
    testEcho(c, 2);
}

BOOST_AUTO_TEST_CASE(SyncServiceToClientEcho)
{
    Service s(socketPath);
    Client c(socketPath);
    c.setMethodHandler<SendData, RecvData>(1, echoCallback);
    FileDescriptor peerFD = connect(s, c);

    std::shared_ptr<SendData> sentData(new SendData(56));
    std::shared_ptr<RecvData> recvData = s.callSync<SendData, RecvData>(1, peerFD, sentData);
    BOOST_REQUIRE(recvData);
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

BOOST_AUTO_TEST_CASE(AsyncClientToServiceEcho)
{
    std::shared_ptr<SendData> sentData(new SendData(34));
    ValueLatch<std::shared_ptr<RecvData>> recvDataLatch;

    // Setup Service and Client
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, echoCallback);
    s.start();
    Client c(socketPath);
    c.start();

    //Async call
    auto dataBack = [&recvDataLatch](Result<RecvData> && r) {
        recvDataLatch.set(r.get());
    };
    c.callAsync<SendData, RecvData>(1, sentData, dataBack);

    // Wait for the response
    std::shared_ptr<RecvData> recvData(recvDataLatch.get(TIMEOUT));
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

BOOST_AUTO_TEST_CASE(AsyncServiceToClientEcho)
{
    std::shared_ptr<SendData> sentData(new SendData(56));
    ValueLatch<std::shared_ptr<RecvData>> recvDataLatch;

    Service s(socketPath);
    Client c(socketPath);
    c.setMethodHandler<SendData, RecvData>(1, echoCallback);
    FileDescriptor peerFD = connect(s, c);

    // Async call
    auto dataBack = [&recvDataLatch](Result<RecvData> && r) {
        recvDataLatch.set(r.get());
    };

    s.callAsync<SendData, RecvData>(1, peerFD, sentData, dataBack);

    // Wait for the response
    std::shared_ptr<RecvData> recvData(recvDataLatch.get(TIMEOUT));
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}


BOOST_AUTO_TEST_CASE(SyncTimeout)
{
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, longEchoCallback);

    Client c(socketPath);
    connect(s, c);

    std::shared_ptr<SendData> sentData(new SendData(78));
    BOOST_REQUIRE_THROW((c.callSync<SendData, RecvData>(1, sentData, TIMEOUT)), IPCException);
}

BOOST_AUTO_TEST_CASE(SerializationError)
{
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, echoCallback);

    Client c(socketPath);
    connect(s, c);

    std::shared_ptr<ThrowOnAcceptData> throwingData(new ThrowOnAcceptData());

    BOOST_CHECK_THROW((c.callSync<ThrowOnAcceptData, RecvData>(1, throwingData)), IPCSerializationException);

}

BOOST_AUTO_TEST_CASE(ParseError)
{
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, echoCallback);
    s.start();

    Client c(socketPath);
    c.start();

    std::shared_ptr<SendData> sentData(new SendData(78));
    BOOST_CHECK_THROW((c.callSync<SendData, ThrowOnAcceptData>(1, sentData, 10000)), IPCParsingException);
}

BOOST_AUTO_TEST_CASE(DisconnectedPeerError)
{
    ValueLatch<Result<RecvData>> retStatusLatch;
    Service s(socketPath);

    auto method = [](const FileDescriptor, std::shared_ptr<ThrowOnAcceptData>&) {
        return std::shared_ptr<SendData>(new SendData(1));
    };

    // Method will throw during serialization and disconnect automatically
    s.setMethodHandler<SendData, ThrowOnAcceptData>(1, method);
    s.start();

    Client c(socketPath);
    c.start();

    auto dataBack = [&retStatusLatch](Result<RecvData> && r) {
        retStatusLatch.set(std::move(r));
    };

    std::shared_ptr<SendData> sentData(new SendData(78));
    c.callAsync<SendData, RecvData>(1, sentData, dataBack);

    // Wait for the response
    Result<RecvData> result = retStatusLatch.get(TIMEOUT);

    // The disconnection might have happened:
    // - after sending the message (PEER_DISCONNECTED)
    // - during external serialization (SERIALIZATION_ERROR)
    BOOST_CHECK_THROW(result.get(), IPCException);
}


BOOST_AUTO_TEST_CASE(ReadTimeout)
{
    Service s(socketPath);
    auto longEchoCallback = [](const FileDescriptor, std::shared_ptr<RecvData>& data) {
        return std::shared_ptr<LongSendData>(new LongSendData(data->intVal, LONG_OPERATION_TIME));
    };
    s.setMethodHandler<LongSendData, RecvData>(1, longEchoCallback);

    Client c(socketPath);
    connect(s, c);

    // Test timeout on read
    std::shared_ptr<SendData> sentData(new SendData(334));
    BOOST_CHECK_THROW((c.callSync<SendData, RecvData>(1, sentData, TIMEOUT)), IPCException);
}


BOOST_AUTO_TEST_CASE(WriteTimeout)
{
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, echoCallback);
    s.start();

    Client c(socketPath);
    c.start();

    // Test echo with a minimal timeout
    std::shared_ptr<LongSendData> sentDataA(new LongSendData(34, SHORT_OPERATION_TIME));
    std::shared_ptr<RecvData> recvData = c.callSync<LongSendData, RecvData>(1, sentDataA, TIMEOUT);
    BOOST_REQUIRE(recvData);
    BOOST_CHECK_EQUAL(recvData->intVal, sentDataA->intVal);

    // Test timeout on write
    std::shared_ptr<LongSendData> sentDataB(new LongSendData(34, LONG_OPERATION_TIME));
    BOOST_CHECK_THROW((c.callSync<LongSendData, RecvData>(1, sentDataB, TIMEOUT)), IPCTimeoutException);
}


BOOST_AUTO_TEST_CASE(AddSignalInRuntime)
{
    ValueLatch<std::shared_ptr<RecvData>> recvDataLatchA;
    ValueLatch<std::shared_ptr<RecvData>> recvDataLatchB;

    Service s(socketPath);
    Client c(socketPath);
    connect(s, c);

    auto handlerA = [&recvDataLatchA](const FileDescriptor, std::shared_ptr<RecvData>& data) {
        recvDataLatchA.set(data);
    };

    auto handlerB = [&recvDataLatchB](const FileDescriptor, std::shared_ptr<RecvData>& data) {
        recvDataLatchB.set(data);
    };

    c.setSignalHandler<RecvData>(1, handlerA);
    c.setSignalHandler<RecvData>(2, handlerB);

    // Wait for the signals to propagate to the Service
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * TIMEOUT));

    auto sendDataA = std::make_shared<SendData>(1);
    auto sendDataB = std::make_shared<SendData>(2);
    s.signal<SendData>(2, sendDataB);
    s.signal<SendData>(1, sendDataA);

    // Wait for the signals to arrive
    std::shared_ptr<RecvData> recvDataA(recvDataLatchA.get(TIMEOUT));
    std::shared_ptr<RecvData> recvDataB(recvDataLatchB.get(TIMEOUT));
    BOOST_CHECK_EQUAL(recvDataA->intVal, sendDataA->intVal);
    BOOST_CHECK_EQUAL(recvDataB->intVal, sendDataB->intVal);
}


BOOST_AUTO_TEST_CASE(AddSignalOffline)
{
    ValueLatch<std::shared_ptr<RecvData>> recvDataLatchA;
    ValueLatch<std::shared_ptr<RecvData>> recvDataLatchB;

    Service s(socketPath);
    Client c(socketPath);

    auto handlerA = [&recvDataLatchA](const FileDescriptor, std::shared_ptr<RecvData>& data) {
        recvDataLatchA.set(data);
    };

    auto handlerB = [&recvDataLatchB](const FileDescriptor, std::shared_ptr<RecvData>& data) {
        recvDataLatchB.set(data);
    };

    c.setSignalHandler<RecvData>(1, handlerA);
    c.setSignalHandler<RecvData>(2, handlerB);

    connect(s, c);

    // Wait for the information about the signals to propagate
    std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));

    auto sendDataA = std::make_shared<SendData>(1);
    auto sendDataB = std::make_shared<SendData>(2);
    s.signal<SendData>(2, sendDataB);
    s.signal<SendData>(1, sendDataA);

    // Wait for the signals to arrive
    std::shared_ptr<RecvData> recvDataA(recvDataLatchA.get(TIMEOUT));
    std::shared_ptr<RecvData> recvDataB(recvDataLatchB.get(TIMEOUT));
    BOOST_CHECK_EQUAL(recvDataA->intVal, sendDataA->intVal);
    BOOST_CHECK_EQUAL(recvDataB->intVal, sendDataB->intVal);
}


BOOST_AUTO_TEST_CASE(ServiceGSource)
{
    utils::Latch l;
    ScopedGlibLoop loop;

    auto signalHandler = [&l](const FileDescriptor, std::shared_ptr<RecvData>&) {
        l.set();
    };

    IPCGSource::Pointer serviceGSource;
    Service s(socketPath);
    s.setMethodHandler<SendData, RecvData>(1, echoCallback);

    Client c(socketPath);
    s.setSignalHandler<RecvData>(2, signalHandler);

    connectServiceGSource(s, c);

    testEcho(c, 1);

    auto data = std::make_shared<SendData>(1);
    c.signal<SendData>(2, data);

    BOOST_CHECK(l.wait(TIMEOUT));
}


BOOST_AUTO_TEST_CASE(ClientGSource)
{
    utils::Latch l;
    ScopedGlibLoop loop;

    auto signalHandler = [&l](const FileDescriptor, std::shared_ptr<RecvData>&) {
        l.set();
    };

    Service s(socketPath);
    s.start();

    IPCGSource::Pointer clientGSource;
    Client c(socketPath);
    c.setMethodHandler<SendData, RecvData>(1, echoCallback);
    c.setSignalHandler<RecvData>(2, signalHandler);

    FileDescriptor peerFD = connectClientGSource(s, c);

    testEcho(s, 1, peerFD);

    auto data = std::make_shared<SendData>(1);
    s.signal<SendData>(2, data);

    BOOST_CHECK(l.wait(TIMEOUT));
}

BOOST_AUTO_TEST_CASE(UsersError)
{
    const int TEST_ERROR_CODE = -234;
    const std::string TEST_ERROR_MESSAGE = "Ay, caramba!";

    Service s(socketPath);
    Client c(socketPath);
    auto clientID = connect(s, c);

    auto throwingMethodHandler = [&](const FileDescriptor, std::shared_ptr<RecvData>&) -> std::shared_ptr<SendData> {
        throw IPCUserException(TEST_ERROR_CODE, TEST_ERROR_MESSAGE);
    };

    s.setMethodHandler<SendData, RecvData>(1, throwingMethodHandler);
    c.setMethodHandler<SendData, RecvData>(1, throwingMethodHandler);

    std::shared_ptr<SendData> sentData(new SendData(78));

    auto hasProperData = [&](const IPCUserException & e) {
        return e.getCode() == TEST_ERROR_CODE && e.what() == TEST_ERROR_MESSAGE;
    };

    BOOST_CHECK_EXCEPTION((c.callSync<SendData, RecvData>(1, sentData, TIMEOUT)), IPCUserException, hasProperData);
    BOOST_CHECK_EXCEPTION((s.callSync<SendData, RecvData>(1, clientID, sentData, TIMEOUT)), IPCUserException, hasProperData);

}
// BOOST_AUTO_TEST_CASE(ConnectionLimitTest)
// {
//     unsigned oldLimit = ipc::getMaxFDNumber();
//     ipc::setMaxFDNumber(50);

//     // Setup Service and many Clients
//     Service s(socketPath);
//     s.setMethodHandler<SendData, RecvData>(1, echoCallback);
//     s.start();

//     std::list<Client> clients;
//     for (int i = 0; i < 100; ++i) {
//         try {
//             clients.push_back(Client(socketPath));
//             clients.back().start();
//         } catch (...) {}
//     }

//     unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
//     std::mt19937 generator(seed);
//     for (auto it = clients.begin(); it != clients.end(); ++it) {
//         try {
//             std::shared_ptr<SendData> sentData(new SendData(generator()));
//             std::shared_ptr<RecvData> recvData = it->callSync<SendData, RecvData>(1, sentData);
//             BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
//         } catch (...) {}
//     }

//     ipc::setMaxFDNumber(oldLimit);
// }



BOOST_AUTO_TEST_SUITE_END()
