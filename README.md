# Blynk-Particle-Example-MQTT

Provision a Particle device to Blynk using the Blynk app's BLE "Inject"
flow, then connect it directly to Blynk over MQTT. There's no Particle
webhook and no Blynk Data Converter.

This is the third in a series:

- [Blynk-Particle-Example](https://github.com/anthony-blynk/Blynk-Particle-Example):
  static tokens/QR codes, data via `Particle.publish()` + webhook.
- [Blynk-Particle-Example-BLE](https://github.com/anthony-blynk/Blynk-Particle-Example-BLE):
  BLE provisioning, data still via `Particle.publish()` + webhook.
- **This one**: BLE provisioning, data via a direct MQTT/TLS connection to
  Blynk.

Going direct means two-way data with no per-template webhook or converter to
maintain. Datastream changes from the Blynk app or dashboard reach the
device straight away. The device also shows as online/offline in Blynk
based on its own connection.

## How it works

```
Blynk app --BLE-->  Particle device --HTTPS--> Blynk (claim token, find regional server)
Particle device <--MQTT/TLS :8883--> Blynk
```

On first boot the device advertises over BLE. When you open it in the Blynk
app, the app hands over a fresh auth token. The device claims the token over
HTTPS, which also tells it which regional server it lives on (e.g.
`fra1.blynk.cloud`). It then connects to that server's MQTT broker with
username `device` and the token as the password. The app is only told that
provisioning succeeded once that MQTT connection is up. After that, the
token and server are saved, so later boots skip straight to MQTT.

The Particle Cloud connection stays up alongside MQTT, for OTA updates and
the `clearProvisioning` cloud function.

## Setup

1. **Blynk**: on your template, create datastreams named `Temperature`,
   `Humidity`, `Uptime` (device to Blynk) and `Switch` (Blynk to device,
   integer 0/1). Add widgets for them.
2. Fill in `BLYNK_TEMPLATE_ID` / `BLYNK_TEMPLATE_NAME` in `src/main.cpp` for
   your Blynk template.
3. Flash the firmware, then open the device in the Blynk app to provision
   it over BLE.
4. Values from `publishSensorData()` appear in Blynk every 30 seconds.
   Toggling the `Switch` widget logs `Switch turned on/off` over serial.

## Sending and receiving data

- **Device to Blynk**: `blynkMqttPublish("batch_ds", "{\"Temperature\":23.5}")`
  sends several datastreams at once, keyed by datastream name. For a single
  value, use `blynkMqttPublish("ds/Temperature", "23.5")`.
- **Blynk to device**: `blynkMqttWrite(datastream, value)` in `src/main.cpp`
  is called for every `downlink/ds/<Datastream>` message. It's the MQTT
  equivalent of `BLYNK_WRITE()`.

`src/BlynkMqtt.h` handles reconnecting and the `downlink/reboot`,
`downlink/reconfigure`, `downlink/ping` and `downlink/redirect` messages.
It also logs any `downlink/diag` server diagnostics as warnings. A packet (topic plus
payload) can be at most 512 bytes. That limit is `MQTT_MAX_PACKET_SIZE` in
`lib/MQTT-TLS/src/MQTT-TLS.h`.

## Libraries

- `lib/MQTT-TLS`: [hirotakaster/MQTT-TLS](https://github.com/hirotakaster/MQTT-TLS)
  0.2.27, with the packet size raised to 512.
- `lib/HttpClient-mbedtls`: HTTPS client for the one-off provisioning call.
- `lib/mbedtls`: one shared copy of mbedtls. Both libraries above normally
  bundle their own copy, which causes duplicate symbols when both are used
  together, so both include this one instead (`../../mbedtls/...`).

## Blynk Enterprise / your own branded app

If you're a Blynk Enterprise client with your own server and a
white-labeled app, uncomment and fill in `BLYNK_VENDOR_PREFIX` and
`BLYNK_DEFAULT_SERVER` near the top of `src/main.cpp`. The MQTT broker is
assumed to be on the same host, port 8883.

## Cellular-only, no WiFi

Same as Blynk-Particle-Example-BLE: WiFi is turned off (`WiFi.off()` in
`setup()`), and the vendored `NetMgr` has WiFi compiled out. On this
hardware WiFi shares a radio with BLE, and turning it on during
provisioning makes advertising flaky. Note that MQTT and the Particle Cloud
connection both use cellular data.

## Re-provisioning

In the Blynk app, open the device, tap the info icon, then the three dots,
then **Reconfigure**. Blynk sends `downlink/reconfigure` over MQTT, and the
device erases its saved token/server (and any stored WiFi credentials). It
then reboots straight back into BLE provisioning, ready to be set up again
from the app.

That needs the device to be online in Blynk. If it isn't, for example
because the device was deleted in Blynk and its saved token no longer works,
use the `clearProvisioning` cloud function instead. It does the same thing
via the Particle Cloud:
`particle call <device> clearProvisioning ""`.

Rebooting from Blynk (*Reboot* in the same menu, `downlink/reboot`) just
restarts the device. It keeps its provisioning.
