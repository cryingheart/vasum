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
 * @brief   Container interface
 */

#ifndef LXCPP_CONTAINER_HPP
#define LXCPP_CONTAINER_HPP

#include "lxcpp/network-config.hpp"

#include <string>
#include <functional>
#include <vector>

namespace lxcpp {

enum class NetStatus {
    DOWN,
    UP
};

struct NetworkInterfaceInfo {
    const std::string ifname;
    const NetStatus status;
    const std::vector<InetAddr> addrs;
};

class Container {
public:
    typedef std::function<int()> AttachCall;

    virtual ~Container() {};

    virtual std::string getName() = 0;
    virtual void setName(const std::string& name) = 0;

    //Execution actions
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void freeze() = 0;
    virtual void unfreeze() = 0;
    virtual void reboot() = 0;
    virtual int getInitPid() = 0;

    //Filesystem actions
    virtual void create() = 0;
    virtual void destroy() = 0;
    virtual void setRootPath(const std::string& path) = 0;
    virtual std::string getRootPath() = 0;

    // Other
    virtual void attach(AttachCall& attachCall) = 0;

    // Network interfaces setup/config
    virtual void addInterfaceConfig(const std::string& hostif,
                                 const std::string& zoneif,
                                 InterfaceType type,
                                 MacVLanMode mode) = 0;
    virtual void addAddrConfig(const std::string& ifname, const InetAddr& addr) = 0;

    // Network interfaces (runtime)
    virtual std::vector<std::string> getInterfaces() = 0;
    virtual NetworkInterfaceInfo getInterfaceInfo(const std::string& ifname) = 0;
    virtual void createInterface(const std::string& hostif,
                                 const std::string& zoneif,
                                 InterfaceType type,
                                 MacVLanMode mode) = 0;
    virtual void destroyInterface(const std::string& ifname) = 0;
    virtual void setUp(const std::string& ifname) = 0;
    virtual void setDown(const std::string& ifname) = 0;
    virtual void addAddr(const std::string& ifname, const InetAddr& addr) = 0;
    virtual void delAddr(const std::string& ifname, const InetAddr& addr) = 0;
};

} // namespace lxcpp

#endif // LXCPP_CONTAINER_HPP
