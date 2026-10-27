/*
 * Blynk-Particle-Example-MQTT
 *
 * Uses Blynk's BLE "Inject" flow (in the Blynk app) to provision this
 * device once: the app hands over a Blynk auth token over BLE, the device
 * claims it via the Blynk HTTP API, then connects straight to Blynk's MQTT
 * broker with it - no Particle webhook or Blynk Data Converter involved.
 *
 * Follow-on to Blynk-Particle-Example-BLE (same provisioning flow, but data
 * went Particle.publish() -> webhook -> Blynk there).
 *
 * Fill in your Blynk Template here. Read more: https://bit.ly/BlynkInject
 */
// #define BLYNK_TEMPLATE_ID     "TMPxxxxxx"
// #define BLYNK_TEMPLATE_NAME   "Device"
#define BLYNK_TEMPLATE_ID "TMPL4fKqC2IdZ"
#define BLYNK_TEMPLATE_NAME "blynk-particle-example"

#define BLYNK_FIRMWARE_VERSION "0.1.0"

/* White labeling (use this ONLY if you have a branded Blynk App/server) */
//#define BLYNK_VENDOR_PREFIX         "Blynk"
//#define BLYNK_DEFAULT_SERVER        "my-dashboard.com"

#include <Particle.h>
#include "BlynkInjectHelper.h"

SYSTEM_MODE(AUTOMATIC);

SerialLogHandler logHandler(LOG_LEVEL_INFO, {
    { "app", LOG_LEVEL_TRACE },
    { "blynk.inject", LOG_LEVEL_TRACE }
});

const unsigned long PUBLISH_INTERVAL_MS = 30000;

unsigned long lastPublish = 0;

void publishSensorData();

void setup() {
    Serial.begin(115200);
    waitFor(Serial.isConnected, 5000);
    Serial.println();

    // This device is cellular-only - explicitly turn WiFi off rather than
    // relying only on having no stored credentials. Safe alongside BLE:
    // the WiFi+BLE combo radio's contention risk (see lib/NetMgr/src/NetMgr.h)
    // is specifically about code turning WiFi *on*, not off.
    WiFi.off();

    // Register cloud functions *before* connecting - Particle.connect()
    // (called inside doBlynkProvisioning()) triggers Device OS's one-time
    // "Describe" message early in the handshake, well before
    // doBlynkProvisioning() itself returns. Anything registered after that
    // point wouldn't be included and would never show up in the console.
    Particle.function("clearProvisioning", clearProvisioning);

    // One-time BLE provisioning: no-op if already provisioned (see
    // isAlreadyProvisioned() in BlynkInjectHelper.h).
    if (!doBlynkProvisioning()) {
        Log.error("Provisioning failed, rebooting to retry");
        delay(1000); // let the log line flush over serial before reset
        System.reset();
    }
}

void loop() {
    blynkMqttRun();

    if (blynkMqttConnected() && millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
        lastPublish = millis();
        publishSensorData();
    }

    if (resetRequested) {
        delay(1000); // let the clearProvisioning() call's return value flush over the cloud connection first
        System.reset();
    }
}

// Swap this for real sensor readings. Keys are Blynk datastream names (as
// set up on your template), all sent in one message via batch_ds. A single
// value can be sent with blynkMqttPublish("ds/Temperature", "23.5").
void publishSensorData() {
    float temperature = 20.0 + random(0, 100) / 10.0; // 20.0 - 30.0 C
    float humidity = 40.0 + random(0, 300) / 10.0;    // 40.0 - 70.0 %

    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"Temperature\":%.1f,\"Humidity\":%.1f,\"Uptime\":%lu}",
             temperature, humidity, millis() / 1000);

    blynkMqttPublish("batch_ds", payload);
}

// Called whenever a datastream value is changed from Blynk (app or web
// dashboard widget, automation, HTTP API...) - the MQTT equivalent of
// BLYNK_WRITE(). value is always a string; convert it as needed.
void blynkMqttWrite(const char* datastream, const char* value) {
    if (strcmp(datastream, "Switch") == 0) {
        bool on = atoi(value) != 0;
        Log.info("Switch turned %s", on ? "on" : "off");
        // e.g. digitalWrite(RELAY_PIN, on);
    } else {
        Log.info("Unhandled datastream %s = %s", datastream, value);
    }
}
