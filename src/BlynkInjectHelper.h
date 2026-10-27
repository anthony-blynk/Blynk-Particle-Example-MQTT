#pragma once
#include "Particle.h"
#include <BlynkInject.h>
#include <BlynkSysUtils.h>
#include <httpc.h>
#include "BlynkMqtt.h"

#ifndef BLYNK_VENDOR_PREFIX
#define BLYNK_VENDOR_PREFIX "Blynk"
#endif
#ifndef BLYNK_DEFAULT_SERVER
#define BLYNK_DEFAULT_SERVER "blynk.cloud"
#endif
// Cellular can take much longer than WiFi to associate (poor coverage, SIM
// activation, carrier registration) - bound the wait so a device that never
// gets online during provisioning reports a failure and retries instead of
// hanging forever with no feedback.
#ifndef PARTICLE_CONNECT_TIMEOUT_MS
#define PARTICLE_CONNECT_TIMEOUT_MS (180000UL) // 3 minutes
#endif
// MQTT connection attempts made while the app is still waiting over BLE,
// before reporting failure. Each one blocks for up to BLYNK_MQTT_KEEPALIVE.
#ifndef PROVISIONING_MQTT_ATTEMPTS
#define PROVISIONING_MQTT_ATTEMPTS 3
#endif

static bool headerNameIs(const char* name, const char* expected) {
   while (*name && *expected) {
      char a = *name++, b = *expected++;
      if (a >= 'a' && a <= 'z') a -= 32;
      if (b >= 'a' && b <= 'z') b -= 32;
      if (a != b) return false;
   }
   return *name == 0 && *expected == 0;
}

// Splits an absolute "https://host[:port]/path?query" URL into host and
// path+query. Returns false if it's not an absolute https:// URL (in which
// case it's a path-only/relative redirect, to be used against the same host).
static bool splitAbsoluteUrl(const String& url, String& host, String& path) {
   if (!url.startsWith("https://")) return false;
   int hostStart = 8; // strlen("https://")
   int slash = url.indexOf('/', hostStart);
   if (slash < 0) {
      host = url.substring(hostStart);
      path = "/";
   } else {
      host = url.substring(hostStart, slash);
      path = url.substring(slash);
   }
   return host.length() > 0;
}

// Blynk's external API redirects the generic "blynk.cloud" host to the
// actual regional server (e.g. via HTTP 308), so this follows redirects
// rather than treating them as failures. Returns 0 on a final HTTP 200,
// -1 otherwise. On success, finalHost is set to whichever host actually
// answered - that's the regional server the MQTT connection should use.
static int do_https_get(const char* host, const int port, const char* path, String& finalHost) {
   String curHost = host;
   String curPath = path;

   for (int redirects = 0; redirects < 3; redirects++) {
      HttpsClient client;
      int ret = client.initTls(blynk_root_ca_cert, strlen(blynk_root_ca_cert) + 1);
      if (ret != 0) {
         LOG_E("initTls failed: %d", ret);
         return ret;
      }

      ret = client.connect((char*)curHost.c_str(), port);
      if (ret != 0) {
         LOG_E("connect failed: %d", ret);
         return ret;
      }

      ret = client.get(curPath.c_str());

      uint16_t status = client.resp->status_code;
      LOG_I("HTTP status %d, %d byte(s) received", status, ret);
      if (ret > 0) {
         LOG_I("body: %.*s", ret, client.resp->body); // (dynamic length)
      }

      if (status == 301 || status == 302 || status == 307 || status == 308) {
         String location;
         bool found = false;
         for (int i = 0; i < client.resp->headers.size(); i++) {
            if (headerNameIs(client.resp->headers.at(i).name, "Location")) {
               location = client.resp->headers.at(i).value;
               found = true;
               break;
            }
         }
         client.disconnect();

         if (!found) {
            LOG_E("Redirect (%d) with no Location header", status);
            return -1;
         }

         String newHost, newPath;
         if (splitAbsoluteUrl(location, newHost, newPath)) {
            LOG_I("Redirected to host %s path %s", newHost.c_str(), newPath.c_str());
            curHost = newHost;
            curPath = newPath;
         } else {
            LOG_I("Redirected to path %s", location.c_str());
            curPath = location;
         }
         continue;
      }

      client.disconnect();
      if (status != 200) {
         return -1;
      }
      finalHost = curHost;
      return 0;
   }

   LOG_E("Too many redirects");
   return -1;
}

// Already provisioned on a previous boot? Also loads the saved token/host
// into blynkConfig for the MQTT connection.
bool isAlreadyProvisioned() {
    return loadBlynkConfig();
}

// Set by clearProvisioning() and acted on from loop() - resetting directly
// inside a cloud function handler tears the device down before the RPC
// return value can be sent back to the caller, so every call would time
// out even though the underlying work already succeeded.
bool resetRequested = false;

int clearProvisioning(String args) {
    LOG_W("clearProvisioning() called (args=\"%s\")", args.c_str());
    clearBlynkConfig();
    resetRequested = true;
    return 1;
}

BlynkInject   _inject;

// Returns false if the app disconnected before completing provisioning
// (i.e. before sending "connect"). BLE is left open either way - the
// caller decides when to end() it.
bool doBlynkInject() {
    LOG_I("Starting BLE provisioning");

    static bool injectDone = false;
    injectDone = false;
    _inject.setProvisionCallback([]() {
        injectDone = true;
    });

    _inject._config.host = BLYNK_DEFAULT_SERVER;

    systemInit(BLYNK_VENDOR_PREFIX, "Device");
    _inject.begin(systemGetDeviceName(), BLYNK_VENDOR_PREFIX, BLYNK_TEMPLATE_ID, "001", BLYNK_FIRMWARE_VERSION);

    while (!injectDone) {
        _inject.run();
        if (_inject.isAppDisconnected()) {
            _inject.setUserFinishedConfiguring();
            LOG_W("App disconnected before completing provisioning");
            return false;
        }
        // clearProvisioning() can be called (and its handler run) while
        // still stuck in this loop, well before setup() ever returns and
        // loop() gets a chance to run - check for it here too, not just in
        // loop(), otherwise the reset request just sits unnoticed.
        if (resetRequested) {
            LOG_W("Reset requested while waiting for BLE provisioning");
            System.reset();
        }
        delay(10);
    }

    LOG_I("BLE provisioning complete");
    return true;
}

// Claims the auth token the app handed over via BLE, and finds out which
// regional server this device lives on (the generic host redirects there).
//  curl -v "https://blynk.cloud/external/api/provision?token=...&templateId=..."
int provisionToken(String& regionalHost) {
    String path = String::format(
        "/external/api/provision?token=%s&templateId=%s",
        _inject._config.auth.c_str(), BLYNK_TEMPLATE_ID);

    LOG_I("Claiming token: host=%s path=%s", _inject._config.host.c_str(), path.c_str());

    return do_https_get(_inject._config.host.c_str(), 443, path.c_str(), regionalHost);
}

// Returns true once the device is provisioned and ready for normal
// operation (either just now, or already, previously). Returns false if
// provisioning failed or was abandoned - caller should not proceed into
// normal operation in that case.
bool doBlynkProvisioning() {
    Particle.connect();

    if (isAlreadyProvisioned()) {
        LOG_I("Already provisioned, Blynk server: %s", blynkConfig.host);
        return true;
    }

    if (!doBlynkInject()) {
        _inject.end();
        return false;
    }

    _inject.reportStatus(BlynkInject::STATUS_CONNECTING_CLOUD);
    LOG_I("Connecting to Particle...");
    unsigned long connectStart = millis();
    while (!Particle.connected()) {
        if (millis() - connectStart >= PARTICLE_CONNECT_TIMEOUT_MS) {
            LOG_E("Timed out waiting for Particle cloud connection");
            _inject.reportFailure(BlynkInject::ERROR_NETWORK_TIMEOUT, "Timed out connecting to Particle");
            _inject.end();
            return false;
        }
        if (resetRequested) {
            LOG_W("Reset requested while waiting for Particle cloud connection");
            System.reset();
        }
        delay(100);
    }

    String regionalHost;
    if (provisionToken(regionalHost) != 0) {
        _inject.reportFailure(BlynkInject::ERROR_CLOUD_TOKEN, "Blynk provisioning failed");
        _inject.end();
        return false;
    }

    strlcpy(blynkConfig.token, _inject._config.auth.c_str(), sizeof(blynkConfig.token));
    strlcpy(blynkConfig.host, regionalHost.c_str(), sizeof(blynkConfig.host));

    // Only tell the app it worked once the device is actually online in
    // Blynk over MQTT - and only save the config then, so a failure here
    // sends the device back into BLE provisioning on the next boot rather
    // than leaving it stuck retrying a config that never worked.
    bool connected = false;
    for (int attempt = 1; attempt <= PROVISIONING_MQTT_ATTEMPTS && !connected; attempt++) {
        connected = blynkMqttConnect();
        if (resetRequested) {
            LOG_W("Reset requested while connecting to Blynk MQTT");
            System.reset();
        }
    }
    if (!connected) {
        _inject.reportFailure(BlynkInject::ERROR_CLOUD_TIMEOUT, "Could not connect to Blynk MQTT");
        _inject.end();
        return false;
    }

    saveBlynkConfig();
    _inject.reportStatus(BlynkInject::STATUS_CONNECTED);
    _inject.end();
    return true;
}
