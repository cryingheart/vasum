/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Piotr Bartosiewicz <p.bartosiewi@partner.samsung.com>
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
 * @author  Piotr Bartosiewicz (p.bartosiewi@partner.samsung.com)
 * @brief   Dbus connection class
 */

#ifndef DBUS_CONNECTION_HPP
#define DBUS_CONNECTION_HPP

#include "utils/callback-guard.hpp"

#include <memory>
#include <string>
#include <functional>
#include <map>
#include <gio/gio.h>


namespace dbus {


typedef std::unique_ptr<GVariant, void(*)(GVariant*)> GVariantPtr;

/**
 * An interface used to set a result to a method call.
 */
class MethodResultBuilder {
public:
    typedef std::shared_ptr<MethodResultBuilder> Pointer;

    virtual ~MethodResultBuilder() {}
    virtual void set(GVariant* parameters) = 0;
    virtual void setVoid() = 0;
    virtual void setError(const std::string& name, const std::string& message) = 0;
    virtual std::string getPeerName() = 0;
};

/**
 * An interface used to get result from async response.
 */
class AsyncMethodCallResult {
public:
    virtual ~AsyncMethodCallResult() {}
    virtual GVariant* get() = 0; // throws DbusException on error
};

/**
 * Dbus connection.
 * Provides a functionality that allows to call dbus methods,
 * register dbus interfaces, etc.
 *
 * TODO divide to interface and implementation header
 */
class DbusConnection {
public:
    typedef std::unique_ptr<DbusConnection> Pointer;

    typedef std::function<void()> VoidCallback;

    typedef std::function<void(const std::string& objectPath,
                               const std::string& interface,
                               const std::string& methodName,
                               GVariant* parameters,
                               MethodResultBuilder::Pointer result
                              )> MethodCallCallback;

    typedef std::function<void(const std::string& name
                              )> ClientVanishedCallback;

    typedef std::function<void(const std::string& senderBusName,
                               const std::string& objectPath,
                               const std::string& interface,
                               const std::string& signalName,
                               GVariant* parameters
                              )> SignalCallback;

    typedef std::function<void(AsyncMethodCallResult& asyncMethodCallResult
                              )> AsyncMethodCallCallback;

    typedef unsigned int SubscriptionId;

    /**
     * Creates a connection to the dbus with given address.
     */
    static Pointer create(const std::string& address);

    /**
     * Creates a connection to the system dbus.
     */
    static Pointer createSystem();

    ~DbusConnection();

    /**
     * Sets a name to the dbus connection.
     * It allows other client to call methods using this name.
     */
    void setName(const std::string& name,
                 const VoidCallback& onNameAcquired,
                 const VoidCallback& onNameLost);

    /**
     * Emits dbus signal.
     */
    void emitSignal(const std::string& objectPath,
                    const std::string& interface,
                    const std::string& name,
                    GVariant* parameters);

    /**
     * Subscribes to a signal.
     * Empty sender means subscribe to all signals
     * Returns a subscription identifier that can be used to unsubscribe signal
     */
    SubscriptionId signalSubscribe(const SignalCallback& callback,
                                   const std::string& senderBusName = "",
                                   const std::string& interface = "",
                                   const std::string& objectPath ="",
                                   const std::string& member = "");

    /**
     * Unsubscribes from a signal.
     */
    void signalUnsubscribe(SubscriptionId subscriptionId);

    /**
     * Registers an object with given definition.
     * Api calls will be handled by given callback.
     */
    void registerObject(const std::string& objectPath,
                        const std::string& objectDefinitionXml,
                        const MethodCallCallback& method,
                        const ClientVanishedCallback& vanish);

    /**
     * Call a dbus method
     */
    GVariantPtr callMethod(const std::string& busName,
                           const std::string& objectPath,
                           const std::string& interface,
                           const std::string& method,
                           GVariant* parameters,
                           const std::string& replyType,
                           int timeoutMs = -1);

    /**
     * Async call a dbus method
     */
    void callMethodAsync(const std::string& busName,
                         const std::string& objectPath,
                         const std::string& interface,
                         const std::string& method,
                         GVariant* parameters,
                         const std::string& replyType,
                         const AsyncMethodCallCallback& callback,
                         int timeoutMs = -1);

    /**
     * Returns an xml with meta description of specified dbus object.
     */
    std::string introspect(const std::string& busName, const std::string& objectPath);

private:
    struct NameCallbacks {
        VoidCallback nameAcquired;
        VoidCallback nameLost;

        NameCallbacks(const VoidCallback& acquired, const VoidCallback& lost)
            : nameAcquired(acquired), nameLost(lost) {}
    };

    typedef std::map<std::string, guint> ClientsMap;

    struct MethodCallbacks {
        MethodCallCallback methodCall;
        ClientVanishedCallback clientVanished;
        DbusConnection *dbusConn;

        MethodCallbacks(const MethodCallCallback& method,
                        const ClientVanishedCallback& vanish,
                        DbusConnection *dbus)
            : methodCall(method), clientVanished(vanish), dbusConn(dbus) {}
    };

    struct VanishedCallbacks {
        ClientVanishedCallback clientVanished;
        ClientsMap &watchedClients;

        VanishedCallbacks(const ClientVanishedCallback& vanish, ClientsMap& clients)
            : clientVanished(vanish), watchedClients(clients) {}
    };

    GDBusConnection* mConnection;
    guint mNameId;
    // It's only ever modified in the glib thread
    ClientsMap mWatchedClients;
    // Guard must be destroyed before objects it protects
    // (e.g. all of the above, see the destructor)
    utils::CallbackGuard mGuard;

    DbusConnection(const std::string& address);

    static void onNameAcquired(GDBusConnection* connection, const gchar* name, gpointer userData);
    static void onNameLost(GDBusConnection* connection, const gchar* name, gpointer userData);
    static void onSignal(GDBusConnection* connection,
                         const gchar* sender,
                         const gchar* object,
                         const gchar* interface,
                         const gchar* name,
                         GVariant* parameters,
                         gpointer userData);
    static void onMethodCall(GDBusConnection* connection,
                             const gchar* sender,
                             const gchar* objectPath,
                             const gchar* interface,
                             const gchar* method,
                             GVariant* parameters,
                             GDBusMethodInvocation* invocation,
                             gpointer userData);
    static void onAsyncReady(GObject* source,
                             GAsyncResult* asyncResult,
                             gpointer userData);
    static void onClientVanish(GDBusConnection* connection,
                               const gchar *name,
                               gpointer userData);
};


} // namespace dbus

#endif // DBUS_CONNECTION_HPP
