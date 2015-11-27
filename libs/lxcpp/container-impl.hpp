/*
 *  Copyright (C) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 * @author  Mateusz Malicki (m.malicki2@samsung.com)
 * @brief   ContainerImpl main class
 */

#ifndef LXCPP_CONTAINER_IMPL_HPP
#define LXCPP_CONTAINER_IMPL_HPP

#include <sys/types.h>
#include <memory>

#include "lxcpp/container-config.hpp"
#include "lxcpp/container.hpp"
#include "lxcpp/namespace.hpp"
#include "lxcpp/guard/api.hpp"

#include "utils/inotify.hpp"

#include "cargo-ipc/epoll/thread-dispatcher.hpp"
#include "cargo-ipc/client.hpp"
#include "cargo-ipc/exception.hpp"

namespace lxcpp {

class ContainerImpl : public virtual Container {
public:
    ContainerImpl(const std::string &name,
                  const std::string &rootPath,
                  const std::string &workPath);
    ~ContainerImpl();

    // Configuration
    const std::string& getName() const;
    const std::string& getRootPath() const;
    void setHostName(const std::string& hostname);

    pid_t getGuardPid() const;
    pid_t getInitPid() const;

    const std::vector<std::string>& getInit();
    void setInit(const std::vector<std::string> &init);

    void setLogger(const logger::LogType type,
                   const logger::LogLevel level,
                   const std::string &arg);

    void setTerminalCount(const unsigned int count);

    void addUIDMap(unsigned min, unsigned max, unsigned num);
    void addGIDMap(unsigned min, unsigned max, unsigned num);

    // Execution actions
    void start();
    void stop();
    void freeze();
    void unfreeze();
    void reboot();

    // State
    Container::State getState();
    void setStartedCallback(const Container::Callback& callback);
    void setStoppedCallback(const Container::Callback& callback);

    // Other
    int attach(const std::vector<std::string>& argv,
               const uid_t uid,
               const gid_t gid,
               const std::string& ttyPath,
               const std::vector<gid_t>& supplementaryGids,
               const int capsToKeep,
               const std::string& workDirInContainer,
               const std::vector<std::string>& envToKeep,
               const std::vector<std::pair<std::string, std::string>>& envToSet);
    void console();

    // Network interfaces setup/config
    /**
     * adds interface configration.
     * throws NetworkException if zoneif name already on list
     */
    void addInterfaceConfig(InterfaceConfigType type,
                            const std::string& hostif,
                            const std::string& zoneif,
                            const std::vector<InetAddr>& addrs,
                            MacVLanMode mode);
    void addInetConfig(const std::string& ifname, const InetAddr& addr);

    // Network interfaces (runtime)
    std::vector<std::string> getInterfaces() const;
    NetworkInterfaceInfo getInterfaceInfo(const std::string& ifname) const;
    void createInterface(const std::string& hostif,
                         const std::string& zoneif,
                         InterfaceType type,
                         MacVLanMode mode);
    void moveInterface(const std::string& ifname);
    void destroyInterface(const std::string& ifname);
    void setUpInterface(const std::string& ifname);
    void setDownInterface(const std::string& ifname);
    void addInetAddr(const std::string& ifname, const InetAddr& addr);
    void delInetAddr(const std::string& ifname, const InetAddr& addr);

    // Provisioning
    void declareFile(const provision::File::Type type,
                     const std::string& path,
                     const int32_t flags,
                     const int32_t mode);
    const FileVector& getFiles() const;
    void removeFile(const provision::File& item);

    void declareMount(const std::string& source,
                      const std::string& target,
                      const std::string& type,
                      const int64_t flags,
                      const std::string& data);
    const MountVector& getMounts() const;
    void removeMount(const provision::Mount& item);

    void declareLink(const std::string& source,
                     const std::string& target);
    const LinkVector& getLinks() const;
    void removeLink(const provision::Link& item);

    // CGroups
    void addSubsystem(const std::string& name, const std::string& path);
    void addCGroup(const std::string& subsys,
                   const std::string& grpname,
                   const std::vector<CGroupParam>& comm,
                   const std::vector<CGroupParam>& params);

    // Environment variables
    void setEnv(const std::vector<std::pair<std::string, std::string>>& variables);

    // Linux capabilities
    void setCaps(const int caps);

    // System Property (sysctl)
    void setSystemProperty(const std::string& name, const std::string& value);

    // Rlimit
    void setRlimit(const std::string& type, const uint64_t hard, const uint64_t soft);

    // Namespaces
    void setNamespaces(const int namespaces);

    // UID/GIDS
    void setUser(const int uid, const int gid, const std::vector<int> additionalGids);

    // Device
    void addDevice(const std::string& path,
                   const char type,
                   const int64_t major,
                   const int64_t minor,
                   const std::string& permissions,
                   const uint32_t fileMode,
                   const uint32_t uid,
                   const uint32_t gid);

    // Hooks
    void addHook(const std::string& type,
                 const std::vector<std::string>& hook,
                 const std::vector<std::pair<std::string, std::string>>& env);


private:
    typedef std::unique_lock<std::mutex> Lock;
    mutable std::mutex mStateMutex;

    std::shared_ptr<ContainerConfig> mConfig;

    cargo::ipc::epoll::ThreadDispatcher mDispatcher;

    std::shared_ptr<cargo::ipc::Client> mClient;
    utils::Inotify mInotify;

    // Callbacks
    Container::Callback mStartedCallback;
    Container::Callback mStoppedCallback;

    void onWorkFileEvent(const std::string& name, const uint32_t mask);

    /**
     * Guards tells that it's ready to receive commands.
     *
     * This is a method handler, not signal to avoid races.
     */
    bool onGuardReady(const cargo::ipc::PeerID,
                      std::shared_ptr<api::Void>&,
                      cargo::ipc::MethodResult::Pointer);

    /**
     * Guards tells that Init exited with some status.
     * Init could have been:
     * - stopped with stop()
     * - exited by itself
     */
    bool onInitStopped(const cargo::ipc::PeerID,
                       std::shared_ptr<api::ExitStatus>& data,
                       cargo::ipc::MethodResult::Pointer);
};

} // namespace lxcpp

#endif // LXCPP_CONTAINER_IMPL_HPP
