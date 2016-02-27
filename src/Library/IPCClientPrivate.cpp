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
#include "IPCClientPrivate.hpp"
#include "IPCPrivate.hpp"
#include "LoggerPrivate.hpp"

#include <sys/poll.h>
#include <sys/eventfd.h>

namespace usbguard
{
  static int32_t qbPollEventFn(int32_t fd, int32_t revents, void *data)
  {
    return 0;
  }

  void IPCClientPrivate::processEvent()
  {
    const json j = receiveOne();
    processOne(j);
  }

  const json IPCClientPrivate::receiveOne()
  {
    char *data = new char[1<<20];
    ssize_t recv_size;

    if ((recv_size = qb_ipcc_event_recv(_qb_conn, data, 1<<20, 500)) < 0) {
      disconnect();
      throw IPCException(IPCException::ProtocolError, "Receive error");
    }

    if (recv_size < (ssize_t)sizeof(struct qb_ipc_response_header)) {
      disconnect();
      throw IPCException(IPCException::ProtocolError, "Message too small");
    }

    const struct qb_ipc_response_header *hdr = \
      (const struct qb_ipc_response_header *)data;

    if (hdr->size != recv_size) {
      disconnect();
      throw IPCException(IPCException::ProtocolError, "Invalid size in header");
    }

    const char *jdata = data + sizeof(struct qb_ipc_response_header);
    const size_t jsize = recv_size - sizeof(struct qb_ipc_response_header);
    const std::string json_string(jdata, jsize);
    const json jobj = json::parse(json_string);
    delete [] data;
    return jobj;
  }

  void IPCClientPrivate::processOne(const json& jobj)
  {
    if (jobj.count("_e")) {
      processExceptionJSON(jobj);
    }
    else if (jobj.count("_s")) {
      processSignalJSON(jobj);
    }
    else if (jobj.count("_r")) {
      processMethodReturnJSON(jobj);
    }
    else if (jobj.count("_m")) {
      processMethodCallJSON(jobj);
    }
    else {
      disconnect();
      throw IPCException(IPCException::ProtocolError, "Unknown message");
    }
    return;
  }

  void IPCClientPrivate::processReturnValue(const json& jobj)
  {
    std::unique_lock<std::mutex> lock(_rv_map_mutex);
    const uint64_t id = jobj["_i"];
    auto const& it = _rv_map.find(id);

    if (it == _rv_map.end()) {
      return;
    }

    auto& promise = it->second;
    promise.set_value(jobj);

    return;
  }

  void IPCClientPrivate::processExceptionJSON(const json& jobj)
  {
    if (jobj.count("_i")) {
      processReturnValue(jobj);
    }
    else {
    }
    return;
  }

  void IPCClientPrivate::processSignalJSON(const json& jobj)
  {
    try {
      const std::string name = jobj["_s"];
      if (name == "DeviceInserted") {
	const json attributes_json = jobj.at("attributes");
	std::map<std::string,std::string> attributes;

	for (auto it = attributes_json.begin(); it != attributes_json.end(); ++it) {
	  const std::string key = it.key();
	  const std::string value = it.value();
	  attributes[key] = value;
	}

	std::vector<USBInterfaceType> interfaces;
	for (auto const& jitem : jobj["interfaces"]) {
	  const std::string type_string = jitem;
	  interfaces.push_back(USBInterfaceType(type_string));
	}

	_p_instance.DeviceInserted(jobj["id"],
				   attributes,
				   interfaces,
				   jobj["rule_match"],
				   jobj["rule_id"]);
      }
      else if (name == "DevicePresent") {
	const json attributes_json = jobj.at("attributes");
	std::map<std::string,std::string> attributes;

	for (auto it = attributes_json.begin(); it != attributes_json.end(); ++it) {
	  const std::string key = it.key();
	  const std::string value = it.value();
	  attributes[key] = value;
	}

	std::vector<USBInterfaceType> interfaces;
	for (auto const& jitem : jobj["interfaces"]) {
	  const std::string type_string = jitem;
	  interfaces.push_back(USBInterfaceType(type_string));
	}

	_p_instance.DevicePresent(jobj["id"],
				  attributes,
				  interfaces,
				  Rule::targetFromString(jobj["target"]));
      }
      else if (name == "DeviceRemoved") {
	const json attributes_json = jobj.at("attributes");
	std::map<std::string,std::string> attributes;

	for (auto it = attributes_json.begin(); it != attributes_json.end(); ++it) {
	  const std::string key = it.key();
	  const std::string value = it.value();
	  attributes[key] = value;
	}

	_p_instance.DeviceRemoved(jobj["id"],
				  attributes);
      }
      else if (name == "DeviceAllowed") {
	const json attributes_json = jobj.at("attributes");
	std::map<std::string,std::string> attributes;

	for (auto it = attributes_json.begin(); it != attributes_json.end(); ++it) {
	  const std::string key = it.key();
	  const std::string value = it.value();
	  attributes[key] = value;
	}

	_p_instance.DeviceAllowed(jobj["id"],
				  attributes,
				  jobj["rule_match"],
				  jobj["rule_id"]);
      }
      else if (name == "DeviceBlocked") {
	const json attributes_json = jobj.at("attributes");
	std::map<std::string,std::string> attributes;

	for (auto it = attributes_json.begin(); it != attributes_json.end(); ++it) {
	  const std::string key = it.key();
	  const std::string value = it.value();
	  attributes[key] = value;
	}

	_p_instance.DeviceBlocked(jobj["id"],
				  attributes,
				  jobj["rule_match"],
				  jobj["rule_id"]);
      }
      else if (name == "DeviceRejected") {
	const json attributes_json = jobj.at("attributes");
	std::map<std::string,std::string> attributes;

	for (auto it = attributes_json.begin(); it != attributes_json.end(); ++it) {
	  const std::string key = it.key();
	  const std::string value = it.value();
	  attributes[key] = value;
	}

	_p_instance.DeviceRejected(jobj["id"],
				   attributes,
				   jobj["rule_match"],
				   jobj["rule_id"]);
      }
      else {
	throw 0;
      }
    } catch(...) {
      disconnect();
      throw IPCException(IPCException::ProtocolError, "Invalid IPC signal data");
    }
    return;
  }

  void IPCClientPrivate::processMethodReturnJSON(const json& jobj)
  {
    if (jobj.count("_i")) {
      processReturnValue(jobj);
    }
    else {
      logger->warn("Missing return value id in the response");
    }
    return;
  }

  void IPCClientPrivate::processMethodCallJSON(const json& jobj)
  {
    return;
  }

  static int32_t qbIPCMessageProcessFn(int32_t fd, int32_t revents, void *data)
  {
    IPCClientPrivate *client = static_cast<IPCClientPrivate*>(data);
    client->processEvent();
    return 0;
  }

  IPCClientPrivate::IPCClientPrivate(IPCClient& p_instance, bool connected)
    : _p_instance(p_instance),
      _thread(this, &IPCClientPrivate::thread)
  {
    _qb_conn = nullptr;
    _qb_conn_fd = -1;
    _eventfd = eventfd(0, 0);
    _qb_loop = qb_loop_create();
    qb_loop_poll_add(_qb_loop, QB_LOOP_HIGH, _eventfd, POLLIN, NULL, qbPollEventFn);
    _thread.start();

    if (connected) {
      try {
        connect();
      }
      catch(...) {
        destruct();
        throw;
      }
    }
    return;
  }

  void IPCClientPrivate::destruct()
  {
    stop();
    qb_loop_poll_del(_qb_loop, _eventfd);
    close(_eventfd);
    qb_loop_destroy(_qb_loop);
  }

  IPCClientPrivate::~IPCClientPrivate()
  {
    disconnect();
    destruct();
    return;
  }

  void IPCClientPrivate::connect()
  {
    _qb_conn = qb_ipcc_connect("usbguard", 1<<20);

    if (_qb_conn == nullptr) {
      throw IPCException(IPCException::ConnectionError, "IPC Connection not established");
    }

    qb_ipcc_fd_get(_qb_conn, &_qb_conn_fd);

    if (_qb_conn_fd < 0) {
      qb_ipcc_disconnect(_qb_conn);
      _qb_conn = nullptr;
      _qb_conn_fd = -1;
      throw IPCException(IPCException::ConnectionError, "Bad file descriptor");
    }

    qb_loop_poll_add(_qb_loop, QB_LOOP_HIGH, _qb_conn_fd, POLLIN, this, qbIPCMessageProcessFn);
    _p_instance.IPCConnected();
    return;
  }

  void IPCClientPrivate::disconnect()
  {
    if (_qb_conn != nullptr && _qb_conn_fd != -1) {
      qb_loop_poll_del(_qb_loop, _qb_conn_fd);
      qb_ipcc_disconnect(_qb_conn);
      _qb_conn = nullptr;
      _qb_conn_fd = -1;
      _p_instance.IPCDisconnected();
    }
    return;
  }

  bool IPCClientPrivate::isConnected() const
  {
    return _qb_conn_fd != -1;
  }

  void IPCClientPrivate::wait()
  {
    _thread.wait();
  }

  uint32_t IPCClientPrivate::appendRule(const std::string& rule_spec, uint32_t parent_id, uint32_t timeout_sec)
  {
    const json jreq = {
      {          "_m", "appendRule" },
      {   "rule_spec", rule_spec },
      { "parent_id", parent_id },
      { "timeout_sec", timeout_sec },
      {          "_i", IPC::uniqueID() }
    };

    const json jrep = qbIPCSendRecvJSON(jreq);

    try {
      const uint32_t retval = jrep["retval"];
      return retval;
    } catch(...) {
      throw IPCException(IPCException::ProtocolError,
			 "Invalid or missing return value after calling appendRule");
    }
  }

  void IPCClientPrivate::removeRule(uint32_t id)
  {
    const json jreq = {
      {   "_m", "removeRule" },
      { "id", id },
      {   "_i", IPC::uniqueID() }
    };

    qbIPCSendRecvJSON(jreq);
    return;
  }

  const RuleSet IPCClientPrivate::listRules()
  {
    const json jreq = {
      { "_m", "listRules" },
      { "_i", IPC::uniqueID() }
    };

    const json jrep = qbIPCSendRecvJSON(jreq);

    try {
      RuleSet ruleset(&_p_instance);

      for (auto it = jrep["retval"].begin(); it != jrep["retval"].end(); ++it) {
        const json rule_json = it.value(); 
        const uint32_t rule_id = rule_json["id"];
        const std::string rule_string = rule_json["rule"];
        Rule rule = Rule::fromString(rule_string);
        rule.setID(rule_id);
        ruleset.appendRule(rule);
      }

      return ruleset;
    } catch(...) {
      throw IPCException(IPCException::ProtocolError,
                         "Invalid or missing return value after calling listRules");
    }
  }

  void IPCClientPrivate::allowDevice(uint32_t id, bool append, uint32_t timeout_sec)
  {
    const json jreq = {
      {          "_m", "allowDevice" },
      {        "id", id },
      {      "append", append },
      { "timeout_sec", timeout_sec },
      {          "_i", IPC::uniqueID() }
    };

    qbIPCSendRecvJSON(jreq);
    return;
  }

  void IPCClientPrivate::blockDevice(uint32_t id, bool append, uint32_t timeout_sec)
  {
    const json jreq = {
      {          "_m", "blockDevice" },
      {        "id", id },
      {      "append", append },
      { "timeout_sec", timeout_sec },
      {          "_i", IPC::uniqueID() }
    };

    qbIPCSendRecvJSON(jreq);
    return;
  }

  void IPCClientPrivate::rejectDevice(uint32_t id, bool append, uint32_t timeout_sec)
  {
    const json jreq = {
      {          "_m", "rejectDevice" },
      {        "id", id },
      {      "append", append },
      { "timeout_sec", timeout_sec },
      {          "_i", IPC::uniqueID() }
    };

    qbIPCSendRecvJSON(jreq);
    return;
  }

  const std::vector<Rule> IPCClientPrivate::listDevices(const std::string& query)
  {
    const json jreq = {
      { "_m", "listDevices" },
      { "query", query },
      { "_i", IPC::uniqueID() }
    };

    const json jrep = qbIPCSendRecvJSON(jreq);

    try {
      std::vector<Rule> devices;

      for (auto it = jrep["retval"].begin(); it != jrep["retval"].end(); ++it) {
        const json device_json = it.value();
        const uint32_t device_id = device_json["id"];
        const std::string device_string = device_json["device"];
        Rule device_rule = Rule::fromString(device_string);
        device_rule.setID(device_id);
        devices.push_back(device_rule);
      }

      return devices;
    } catch(...) {
      throw IPCException(IPCException::ProtocolError,
                         "Invalid or missing return value after calling listDevices");
    }
  }

  void IPCClientPrivate::thread()
  {
    qb_loop_run(_qb_loop);
  }

  void IPCClientPrivate::stop()
  {
    _thread.stop(/*do_wait=*/false);
    qb_loop_stop(_qb_loop);
    const uint64_t one = 1;
    write(_eventfd, &one, sizeof one);
    _thread.wait();
    return;
  }

  json IPCClientPrivate::qbIPCSendRecvJSON(const json& jval)
  {
    if (!isConnected()) {
      throw IPCException(IPCException::ConnectionError, "Not connected");
    }

    const std::string json_string = jval.dump();

    struct qb_ipc_request_header hdr;
    hdr.id = 0;
    hdr.size = sizeof hdr + json_string.size();

    struct iovec iov[2];
    iov[0].iov_base = &hdr;
    iov[0].iov_len = sizeof hdr;
    iov[1].iov_base = (void *)json_string.c_str();
    iov[1].iov_len = json_string.size();

    const uint64_t id = jval["_i"];

    /* Lock the return value slot map */
    std::unique_lock<std::mutex> rv_map_lock(_rv_map_mutex);

    /*
     * Create the promise and future objects.
     * The promise will be fullfiled by the message
     * processing handlers after they process
     * a reply from the server.
     */
    auto& promise = _rv_map[id];
    auto future = promise.get_future();

    qb_ipcc_sendv(_qb_conn, iov, 2);

    /* 
     * Unlock the return value map so that the message
     * processing handler aren't blocked.
     */
    rv_map_lock.unlock();

    /* Wait for some time for the reply to be received */
    const std::chrono::milliseconds timeout_ms(5*1000);
    const bool timed_out = \
      future.wait_for(timeout_ms) == std::future_status::timeout;

    json retval;

    if (!timed_out) {
      retval = future.get();
    }

    /* Remove the slot from the return value slot map */
    rv_map_lock.lock();
    _rv_map.erase(id);
    rv_map_lock.unlock();

    if (timed_out) {
      throw IPCException(IPCException::TransientError, "Timed out while waiting for IPC reply");
    }
    else {
      /*
       * We might have caused an exception. Check whether
       * that's the case and if true, throw it here.
       */
      if (IPCPrivate::isExceptionJSON(retval)) {
        throw IPCPrivate::jsonToIPCException(retval);
      }
      else {
        return retval;
      }
    }
    return json();
  }

  bool IPCClientPrivate::isExceptionJSON(const json& jval) const
  {
    return (jval.count("_e") == 1);
  }
} /* namespace usbguard */
