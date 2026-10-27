/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <NetMgr.h>

#if defined(PARTICLE)
  #include "ConfigSparkBLE.h"
#elif defined(NRF5)
  #include "ConfigNimBLE.h"
#elif defined(ESP32)
  #include "ConfigNimBLE.h"
  //#include "ConfigBluedroid.h"
#endif

class BlynkInject {

public:

    typedef void (provisionCb_t)(void);

    enum InjectStatus {
        STATUS_UNKNOWN = 0,
        STATUS_ERROR,
        STATUS_CONNECTING_NETWORK,
        STATUS_CONNECTING_CLOUD,
        STATUS_CONNECTED,
    };

    enum InjectError {
        /* Note: 700-705 error codes kept for backward compatibility with old versions */
        ERROR_NONE                  =   0,   // All good
        ERROR_CONFIG                = 700,   // Invalid config from app (malformed token,etc)
        ERROR_NETWORK_TIMEOUT       = 701,   // Could not connect to the router
        ERROR_CLOUD_TIMEOUT         = 702,   // Could not connect to the cloud
        ERROR_CLOUD_TOKEN           = 703,   // Invalid auth token error (after connection)
        ERROR_NETWORK_GENERIC       = 704,   // Other issues (i.e. hardware failure, oom, etc)
        ERROR_CLOUD_GENERIC         = 705,   // Other issues (i.e. protocol error, oom, etc)

        ERROR_NETWORK_NOT_FOUND  = 720,   // Network not found
        ERROR_NETWORK_NO_CABLE   = 721,   // Cable is disconnected
        ERROR_NETWORK_AUTH_FAIL  = 722,   // Network authentication failure (wrong password?)
        ERROR_NETWORK_NO_ADDRESS = 723,   // Network address not assigned (use static IP address?)

        ERROR_SIMCARD_MISSING    = 730,   // SIM card is not inserted
        ERROR_SIMCARD_LOCKED     = 731,   // SIM card is locked
        ERROR_SIMCARD_WRONG_PIN  = 732,   // Wrong PIN Code
    };

    BlynkInject();

    void begin(String name, String vendor, String tmpl_id, String fw_type, String fw_ver);
    void run();
    void end();

    bool isUserConfiguring();
    bool isAppDisconnected();
    void setUserFinishedConfiguring() { _user_started_configuring = false; }

    void setProvisionCallback(provisionCb_t* cb);

    void reportStatus(InjectStatus status);
    void reportFailure(InjectError error, const String& msg = "");
    void reportNetStatus();

    void clearRuntimeConfig();

    struct Config {
        String    intf, ssid, pass, auth, host;
        String    ip, mask, gw, dns, dns2;
        bool      forceSave;
    } _config;

private:
    void sendError(const char* type, const char* reason, const String& msg);
    void parse_message();

    void sendMsg(const char* str) {
        _ble.write(str, strlen(str));
    }
    void sendMsg(const void* data, unsigned len) {
        _ble.write(data, len);
    }

private:
    ConfigBLE     _ble;

    bool          _started = false;
    String        _name;
    String        _vendor;
    String        _tmpl_id;
    String        _fw_type;
    String        _fw_ver;
    InjectStatus  _last_status = STATUS_UNKNOWN;
    bool          _user_started_configuring = false;

    provisionCb_t *provisionCb = nullptr;
};
