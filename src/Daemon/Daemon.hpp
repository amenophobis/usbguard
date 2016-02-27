//
// Copyright (C) 2015 Red Hat, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors: Daniel Kopecek <dkopecek@redhat.com>
//
#pragma once

#include "Typedefs.hpp"
#include "ConfigFile.hpp"
#include "IPC.hpp"
#include "RuleSet.hpp"
#include "Rule.hpp"
#include "Device.hpp"
#include "DeviceManager.hpp"
#include "DeviceManagerHooks.hpp"

#include "Common/Thread.hpp"
#include "Common/JSON.hpp"

#include <mutex>
#include <qb/qbipcs.h>
#include <qb/qbloop.h>

namespace usbguard
{
  class Daemon : public IPC, public DeviceManagerHooks
  {
  public:
    enum PresentDevicePolicy {
      Allow,
      Block,
      Reject,
      Keep,
      ApplyPolicy
    };

    Daemon();
    ~Daemon();

    void loadConfiguration(const String& path);
    void loadRules(const String& path);

    void setImplicitPolicyTarget(Rule::Target target);
    void setPresentDevicePolicy(PresentDevicePolicy policy);
    void setPresentControllerPolicy(PresentDevicePolicy policy);

    /* Start the daemon */
    void run();
    /* Stop the daemon */
    void quit();

    uint32_t assignID();

    /* IPC methods */
    uint32_t appendRule(const std::string& rule_spec, uint32_t parent_id, uint32_t timeout_sec);
    void removeRule(uint32_t id);
    const RuleSet listRules();

    void allowDevice(uint32_t id, bool append,  uint32_t timeout_sec);
    void blockDevice(uint32_t id, bool append, uint32_t timeout_sec);
    void rejectDevice(uint32_t id, bool append, uint32_t timeout_sec);
    const std::vector<Rule> listDevices(const std::string& query);

    /* IPC Signals */
    void DeviceInserted(uint32_t id,
			const std::map<std::string,std::string>& attributes,
			const std::vector<USBInterfaceType>& interfaces,
			bool rule_match,
			uint32_t rule_id);

    void DevicePresent(uint32_t id,
		       const std::map<std::string,std::string>& attributes,
		       const std::vector<USBInterfaceType>& interfaces,
		       Rule::Target target);

    void DeviceRemoved(uint32_t id,
		       const std::map<std::string,std::string>& attributes);

    void DeviceAllowed(uint32_t id,
		       const std::map<std::string,std::string>& attributes,
		       bool rule_match,
		       uint32_t rule_id);

    void DeviceBlocked(uint32_t id,
		       const std::map<std::string,std::string>& attributes,
		       bool rule_match,
		       uint32_t rule_id);

    void DeviceRejected(uint32_t id,
			const std::map<std::string,std::string>& attributes,
			bool rule_match,
			uint32_t rule_id);

    /* Device manager hooks */
    void dmHookDeviceInserted(Pointer<Device> device);
    void dmHookDevicePresent(Pointer<Device> device);
    void dmHookDeviceRemoved(Pointer<Device> device);
    void dmHookDeviceAllowed(Pointer<Device> device);
    void dmHookDeviceBlocked(Pointer<Device> device);
    void dmHookDeviceRejected(Pointer<Device> device);
    uint32_t dmHookAssignID();

    json processJSON(const json& jobj);
    json processMethodCallJSON(const json& jobj);
    bool qbIPCConnectionAllowed(uid_t uid, gid_t gid);

    static PresentDevicePolicy presentDevicePolicyFromString(const String& policy_string);
  protected:
    static void qbIPCSendJSON(qb_ipcs_connection_t *qb_conn, const json& jobj);
    static int32_t qbSignalHandlerFn(int32_t signal, void *arg);
    static int32_t qbUDevEventFn(int32_t fd, int32_t revents, void *arg);
    static int32_t qbIPCConnectionAcceptFn(qb_ipcs_connection_t *, uid_t, gid_t);
    static void qbIPCConnectionCreatedFn(qb_ipcs_connection_t *);
    static void qbIPCConnectionDestroyedFn(qb_ipcs_connection_t *);
    static int32_t qbIPCConnectionClosedFn(qb_ipcs_connection_t *);
    static int32_t qbIPCMessageProcessFn(qb_ipcs_connection_t *, void *, size_t);
    static int32_t qbIPCJobAdd(enum qb_loop_priority p, void *data, qb_loop_job_dispatch_fn fn);
    static int32_t qbIPCDispatchAdd(enum qb_loop_priority p, int32_t fd, int32_t evts, void *data, qb_ipcs_dispatch_fn_t fn);
    static int32_t qbIPCDispatchMod(enum qb_loop_priority p, int32_t fd, int32_t evts, void *data, qb_ipcs_dispatch_fn_t fn);
    static int32_t qbIPCDispatchDel(int32_t fd);

    void initIPC();
    void finiIPC();

    void qbIPCBroadcastData(const struct iovec *iov, size_t iov_len);
    void qbIPCBroadcastString(const std::string& s);
    void qbIPCBroadcastJSON(const json& jobj);

    void allowDevice(uint32_t id, Pointer<const Rule> matched_rule);
    void blockDevice(uint32_t id, Pointer<const Rule> matched_rule);
    void rejectDevice(uint32_t id, Pointer<const Rule> matched_rule);

    Pointer<const Rule> appendDeviceRule(uint32_t id, Rule::Target target, uint32_t timeout_sec);

    bool DACAuthenticateIPCConnection(uid_t uid, gid_t gid);
    void DACAddAllowedUID(uid_t uid);
    void DACAddAllowedGID(gid_t gid);
    void DACAddAllowedUID(const String& username);
    void DACAddAllowedGID(const String& groupname);

  private:
    ConfigFile _config;
    RuleSet _ruleset;
    Pointer<DeviceManager> _dm;
    qb_loop_t *_qb_loop;
    qb_ipcs_service_t *_qb_service;
    
    /*
     * == Runtime parameters ==
     */
    bool _ipc_dac_acl;
    std::vector<uid_t> _ipc_allowed_uids;
    std::vector<gid_t> _ipc_allowed_gids;

    Rule::Target _implicit_policy_target;
    PresentDevicePolicy _present_device_policy;
    PresentDevicePolicy _present_controller_policy;
  };
} /* namespace usbguard */
