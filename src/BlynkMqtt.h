#pragma once
#include "Particle.h"
#include <MQTT-TLS.h>
#include "BlynkRootCA.h"

/*
 * Direct MQTT connection to Blynk, using the auth token and server host the
 * device got during BLE provisioning (see BlynkInjectHelper.h).
 *
 * Blynk MQTT API: https://docs.blynk.io/en/blynk.cloud-mqtt-api/device-mqtt-api
 *  - username "device", password = auth token, client ID can be anything
 *  - publish   ds/<Datastream>            single value
 *  - publish   batch_ds                   {"Datastream1":1,"Datastream2":2}
 *  - publish   info/mcu                   firmware info, sent on every connect
 *  - subscribe downlink/#                 everything Blynk sends the device
 */

#ifndef BLYNK_FIRMWARE_VERSION
#define BLYNK_FIRMWARE_VERSION "0.1.0"
#endif
#ifndef BLYNK_MQTT_PORT
#define BLYNK_MQTT_PORT 8883
#endif
// Seconds. Also bounds how long connect() blocks waiting for CONNACK - the
// MQTT-TLS library waits up to the full keepalive interval for it.
#ifndef BLYNK_MQTT_KEEPALIVE
#define BLYNK_MQTT_KEEPALIVE 45
#endif
#ifndef BLYNK_MQTT_RECONNECT_INTERVAL_MS
#define BLYNK_MQTT_RECONNECT_INTERVAL_MS (30000UL)
#endif

// Implemented by the application: called for every downlink/ds/<Datastream>
// message, i.e. whenever a datastream value is changed from the Blynk app,
// web dashboard, an automation or the HTTP API.
void blynkMqttWrite(const char* datastream, const char* value);

// Everything the device needs to reconnect after a reboot, saved once
// provisioning succeeds. The auth token is a secret - anyone holding it can
// act as this device in Blynk.
#define BLYNK_CONFIG_EEPROM_ADDR 0
#define BLYNK_CONFIG_MAGIC       0xB2 // arbitrary marker distinguishing "provisioned" from erased flash

struct BlynkConfig {
    uint8_t magic;
    char    token[33];
    char    host[64];
};

BlynkConfig blynkConfig = {};

bool loadBlynkConfig() {
    EEPROM.get(BLYNK_CONFIG_EEPROM_ADDR, blynkConfig);
    if (blynkConfig.magic != BLYNK_CONFIG_MAGIC) {
        memset(&blynkConfig, 0, sizeof(blynkConfig));
        return false;
    }
    blynkConfig.token[sizeof(blynkConfig.token) - 1] = 0;
    blynkConfig.host[sizeof(blynkConfig.host) - 1] = 0;
    return true;
}

void saveBlynkConfig() {
    blynkConfig.magic = BLYNK_CONFIG_MAGIC;
    EEPROM.put(BLYNK_CONFIG_EEPROM_ADDR, blynkConfig);
}

// Sends the device back into BLE provisioning on its next boot.
void clearBlynkConfig() {
    memset(&blynkConfig, 0, sizeof(blynkConfig));
    EEPROM.put(BLYNK_CONFIG_EEPROM_ADDR, blynkConfig);
    // Also drop Device OS's own stored WiFi credentials - otherwise its
    // Connection Manager will keep preferring WiFi over cellular on its
    // own, independent of anything the BLE provisioning flow does, making
    // it impossible to test a genuinely fresh cellular-only provision.
    WiFi.clearCredentials();
}

// The MQTT-TLS library takes the broker host in its constructor and has no
// way to change it afterwards, and the host isn't known until it's loaded
// from EEPROM (or handed over via BLE) - so it's created on first use.
MQTT* _mqtt = nullptr;

bool _mqttRebootRequested = false;
bool _mqttReconfigureRequested = false;
String _mqttRedirectHost;

// "mqtts://fra1.blynk.cloud:8883" -> "fra1.blynk.cloud"
static String hostFromUrl(const String& url) {
    int start = url.indexOf("://");
    start = (start < 0) ? 0 : start + 3;
    int end = start;
    while (end < (int)url.length() && url.charAt(end) != ':' && url.charAt(end) != '/') {
        end++;
    }
    return url.substring(start, end);
}

static void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    // payload points into the library's shared packet buffer (and isn't
    // null-terminated) - copy it out before doing anything else.
    static char value[MQTT_MAX_PACKET_SIZE];
    if (length >= sizeof(value)) {
        length = sizeof(value) - 1;
    }
    memcpy(value, payload, length);
    value[length] = 0;

    Log.info("MQTT <<< %s: %s", topic, value);

    if (strncmp(topic, "downlink/ds/", 12) == 0) {
        blynkMqttWrite(topic + 12, value);
    } else if (strcmp(topic, "downlink/reboot") == 0) {
        // Acted on from blynkMqttRun() - resetting in here would skip the
        // PUBACK the library sends once this callback returns.
        _mqttRebootRequested = true;
    } else if (strcmp(topic, "downlink/reconfigure") == 0) {
        // "Reconfigure" from the device menu in the Blynk app. Blynk expects
        // the device to go back into setup mode - for this device that
        // means BLE provisioning. Acted on from blynkMqttRun(), like reboot.
        _mqttReconfigureRequested = true;
    } else if (strcmp(topic, "downlink/diag") == 0) {
        // Blynk spotted something odd about how this device is behaving
        // (e.g. publishing too often) - meant for the developer to read.
        Log.warn("Blynk server diagnostic: %s", value);
    } else if (strcmp(topic, "downlink/redirect") == 0) {
        // Blynk wants this device on a different (regional) server.
        _mqttRedirectHost = hostFromUrl(value);
    }
    // downlink/ping (QoS 1) needs nothing here - the library sends the
    // PUBACK the broker is waiting for automatically.
}

bool blynkMqttConnected() {
    return _mqtt && _mqtt->isConnected();
}

bool blynkMqttPublish(const char* topic, const char* payload) {
    if (!blynkMqttConnected()) {
        return false;
    }
    bool ok = _mqtt->publish(topic, payload);
    Log.info("MQTT >>> %s: %s%s", topic, payload, ok ? "" : " (FAILED)");
    return ok;
}

// One connection attempt, blocking for up to BLYNK_MQTT_KEEPALIVE seconds.
bool blynkMqttConnect() {
    if (!_mqtt) {
        _mqtt = new MQTT(blynkConfig.host, BLYNK_MQTT_PORT, BLYNK_MQTT_KEEPALIVE, mqttCallback);
    }

    Log.info("Connecting to MQTT broker %s:%d...", blynkConfig.host, BLYNK_MQTT_PORT);

    // A failed connect() tears down the TLS context, so TLS has to be
    // re-enabled before every attempt, not just the first.
    _mqtt->disconnect();
    int ret = _mqtt->enableTls(blynk_root_ca_cert, strlen(blynk_root_ca_cert) + 1);
    if (ret != 0) {
        Log.error("MQTT enableTls failed: %d", ret);
        return false;
    }

    if (!_mqtt->connect("", "device", blynkConfig.token)) {
        Log.error("MQTT connect failed");
        return false;
    }
    Log.info("MQTT connected");

    _mqtt->subscribe("downlink/#", MQTT::QOS1);

    char info[128];
    snprintf(info, sizeof(info),
             "{\"tmpl\":\"%s\",\"ver\":\"%s\",\"build\":\"%s\"}",
             BLYNK_TEMPLATE_ID, BLYNK_FIRMWARE_VERSION, __DATE__ " " __TIME__);
    blynkMqttPublish("info/mcu", info);
    return true;
}

// Call from loop(): keeps the connection alive, reconnects when it drops,
// dispatches incoming messages to blynkMqttWrite(), and acts on
// reboot/reconfigure/redirect requests from Blynk.
void blynkMqttRun() {
    static unsigned long lastConnectAttempt = 0;
    static bool everAttempted = false;

    if (blynkMqttConnected()) {
        _mqtt->loop();
    } else if (Network.ready() &&
               (!everAttempted || millis() - lastConnectAttempt >= BLYNK_MQTT_RECONNECT_INTERVAL_MS)) {
        everAttempted = true;
        blynkMqttConnect();
        lastConnectAttempt = millis();
    }

    if (_mqttRedirectHost.length() > 0) {
        Log.warn("Redirected to %s, rebooting to reconnect there", _mqttRedirectHost.c_str());
        strlcpy(blynkConfig.host, _mqttRedirectHost.c_str(), sizeof(blynkConfig.host));
        saveBlynkConfig();
        _mqtt->disconnect();
        delay(1000); // let the log line flush over serial before reset
        System.reset();
    }

    if (_mqttReconfigureRequested) {
        Log.warn("Reconfigure requested from Blynk, rebooting into BLE provisioning");
        clearBlynkConfig();
        _mqtt->disconnect();
        delay(1000); // let the log line flush over serial before reset
        System.reset();
    }

    if (_mqttRebootRequested) {
        Log.warn("Reboot requested from Blynk");
        _mqtt->disconnect();
        delay(1000);
        System.reset();
    }
}
