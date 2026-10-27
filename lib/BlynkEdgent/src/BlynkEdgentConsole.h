/*
 * Copyright (c) 2024 Blynk Technologies Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(ESP32)
extern "C" {
  #include "esp_partition.h"
  #include "esp_ota_ops.h"
}
#endif

#ifdef CONFIG_COMMAND_I2CDETECT
#include <Wire.h>
#endif

void Edgent::initConsole(Stream& stream) {
  _console.begin(stream);
  _console.print("\n>");
}

void Edgent::initConsoleCommands() {
  _console.addCommand("reboot", [this]() {
    if (_onUserInitiatedReboot) {
      _onUserInitiatedReboot();
    }
    _console.print("Rebooting...\n");
    systemReboot();
  });

  _console.addCommand("devinfo", [this]() {
    _console.printf(
        R"json({"name":"%s","board":"%s","tmpl_id":"%s","fw_type":"%s","fw_ver":"%s"})json" "\n",
        systemGetDeviceName().c_str(),
        BLYNK_TEMPLATE_NAME,
        BLYNK_TEMPLATE_ID,
        BLYNK_FIRMWARE_TYPE,
        BLYNK_FIRMWARE_VERSION
    );
  });

  _console.addCommand("connect", [this](const BlynkParam &param) {
    if (!param[0].isValid()) {
      _console.print("Usage: connect <auth> [host]\n");
      return;
    }
    String auth = param[0].asStr();
    if (auth.length() != 32) {
      _console.print("Error: invalid token size\n");
      return;
    }

    _store.loadDefault();
    _store.setBlynkAuth(auth);

    if (param[1].isValid()) {
      String host = param[1].asStr();
      _store.setBlynkHost(host);
    }

    _console.print("Trying to connect...\n");
    startInitialConnection();
  });

  _console.addCommand("config", [this](const BlynkParam &param) {
    const String cmd = param[0].asStr();
    if (!param[0].isValid() || cmd == "start") {
      startConfig();
    } else if (cmd == "stop") {
      stopConfig();
    } else if (cmd == "erase") {
      NetMgr.clearAllNetworks();
      setState(MODE_RESET_CONFIG);
    } else {
      _console.getStream().println(F("Available commands: start, stop, erase"));
    }
  });

  _console.addCommand("firmware", [this](const BlynkParam &param) {
    const String cmd = param[0].asStr();
    if (!param[0].isValid() || cmd == "info") {
      _console.printf(" Version:   %s (build %s)\n", BLYNK_FIRMWARE_VERSION, __DATE__ " " __TIME__);
      _console.printf(" Type:      %s\n", BLYNK_FIRMWARE_TYPE);
      _console.printf(" Platform:  %s\n", BLYNK_INFO_DEVICE);
#if defined(ESP32)
      _console.printf(" SDK:       %s\n", ESP.getSdkVersion());

      if (const esp_partition_t* running = esp_ota_get_running_partition()) {
        unsigned sketchSize = ESP.getSketchSize();
        _console.printf(" Partition: %s (%dK)\n", running->label, running->size / 1024);
        _console.printf(" App size:  %dK (%d%%)\n", sketchSize/1024, (sketchSize*100)/(running->size));
        _console.printf(" App MD5:   %s\n", ESP.getSketchMD5().c_str());
      }
#elif defined(PARTICLE)
      _console.printf(" Device OS: %s\n", System.version().c_str());
#endif
    } else {
      _console.getStream().println(F("Available commands: info"));
    }
  });

#if defined(CONFIG_COMMAND_SYS)
  _console.addCommand("sys", [this](const BlynkParam &param) {
    const String tool = param[0].asStr();
    if (tool == "info") {
      _console.printf(" Uptime:          %s\n",        timeSpanToStr(systemUptime() / 1000).c_str());
      _console.printf(" Reset reason:    %s\n",        systemGetResetReason().c_str());
      _console.printf(" Reboots total:   %lu\n",       systemStats.resetCount.total);
      _console.printf("      graceful:   %lu\n",       systemStats.resetCount.graceful);
      _console.printf(" Network drops:   %d\n",        systemStats.network_drops);
      _console.printf(" Cloud drops:     %d\n",        systemStats.cloud_drops);
      _console.printf(" Online total:    %s\n",        timeSpanToStr(systemStats.total_online_time).c_str());
      _console.printf("          max:    %s\n",        timeSpanToStr(systemStats.max_online_time).c_str());
      _console.printf(" Offline total:   %s\n",        timeSpanToStr(systemStats.total_offline_time).c_str());
      _console.printf("           max:   %s\n",        timeSpanToStr(systemStats.max_offline_time).c_str());
    } else if (tool == "drop_stats") {
      systemStats.clear();
    } else {
      _console.getStream().println(F("Available commands: info, drop_stats"));
    }
  });
#endif // CONFIG_COMMAND_SYS

#if defined(CONFIG_COMMAND_NETMGR) && defined(NetMgr_WiFi)
  _console.addCommand("wifi", [this](const BlynkParam &param) {
    const String cmd = param[0].asStr();
    if (!param[0].isValid() || cmd == "info") {
      _console.printf("mac:%s ip:%s status:%s\n",
        NetMgrWiFi.getMacAddress().c_str(),
        NetMgrWiFi.getLocalIP().c_str(),
        NetMgrWiFi.getStatus().c_str()
      );
      if (NetMgrWiFi.getStatus() == "ready") {
        _console.printf("ssid:%s bssid:%s rssi:%ddBm\n",
          NetMgrWiFi.getNetworkSSID().c_str(),
          NetMgrWiFi.getNetworkBSSID().c_str(),
          NetMgrWiFi.getRSSI()
        );
      }
      if (NetMgrWiFi.getErrorStr()) {
        _console.printf("error: %s\n", NetMgrWiFi.getErrorStr());
      }
    } else if (cmd == "scan") {
      int found = NetMgrWiFi.scanNetworks();
      if (found <= 0) {
        _console.printf("No networks\n");
      }
      for (int i = 0; i < found; i++) {
        String ssid, sec, bssid;
        int chan = -1, rssi = 0;
        NetMgrWiFi.scanGetResult(i, ssid, sec, rssi, bssid, chan);
        bool current = (bssid == NetMgrWiFi.getNetworkBSSID());
        _console.printf(
            "%s %-20s [%s] %s ch:%d rssi:%d\n",
            (current ? "*" : " "),
            ssid.c_str(), bssid.c_str(), sec.c_str(),
            chan, rssi);
      }
      NetMgrWiFi.scanDelete();
    } else if (cmd == "add") {
      if (param[2].isValid()) {
        NetMgrWiFi.addNetwork(param[1].asStr(), param[2].asStr());
      } else {
        NetMgrWiFi.addNetwork(param[1].asStr());
      }
    } else if (cmd == "clear") {
      NetMgrWiFi.clearNetworks();
    } else if (cmd == "on") {
      NetMgrWiFi.on();
    } else if (cmd == "off") {
      NetMgrWiFi.off();
    } else {
      _console.getStream().println(F("Available commands: info, scan, add ssid [pass], clear, on, off"));
    }
  });
#endif /* NetMgr_WiFi */

#if defined(CONFIG_COMMAND_NETMGR) && defined(NetMgr_Ethernet)
  _console.addCommand("eth", [this](const BlynkParam &param) {
    const String cmd = param[0].asStr();
    if (!param[0].isValid() || cmd == "info") {
      if (!NetMgrEthernet.isHardwareAvailable()) {
        _console.printf("Hardware N/A\n");
        return;
      }
      _console.printf("mac:%s ip:%s status:%s\n",
        NetMgrEthernet.getMacAddress().c_str(),
        NetMgrEthernet.getLocalIP().c_str(),
        NetMgrEthernet.getStatus().c_str()
      );
      if (NetMgrEthernet.getErrorStr()) {
        _console.printf("error: %s\n", NetMgrEthernet.getErrorStr());
      }
    } else if (cmd == "on") {
      NetMgrEthernet.on();
    } else if (cmd == "off") {
      NetMgrEthernet.off();
    } else {
      _console.getStream().println(F("Available commands: info, on, off"));
    }
  });
#endif /* NetMgr_Ethernet */

#if defined(CONFIG_COMMAND_NETMGR) && defined(NetMgr_Cellular)
  _console.addCommand("cell", [this](const BlynkParam &param) {
    const String cmd = param[0].asStr();
    if (!param[0].isValid() || cmd == "info") {
      if (!NetMgrCellular.isHardwareAvailable()) {
        _console.printf("Hardware N/A\n");
        return;
      }
      _console.printf("operator:%s ip:%s status:%s signal:%d%%\n",
          NetMgrCellular.getOperator().c_str(),
          NetMgrCellular.getLocalIP().c_str(),
          NetMgrCellular.getStatus().c_str(),
          NetMgrCellular.getSignalStrength()
      );
      if (NetMgrCellular.getErrorStr()) {
        _console.printf("error: %s\n", NetMgrCellular.getErrorStr());
      }
    } else if (cmd == "on") {
      NetMgrCellular.on();
    } else if (cmd == "off") {
      NetMgrCellular.off();
    } else if (cmd == "modem") {
      const String cmd2 = param[1].asStr();
      if (!param[1].isValid() || cmd2 == "info") {
        String name = NetMgrCellular.getModemName();
        String info = NetMgrCellular.getModemInfo();
        String imei = NetMgrCellular.getIMEI();
        String imsi = NetMgrCellular.getIMSI();
        String iccid = NetMgrCellular.getICCID();

        _console.printf("Modem: %s\n", name.c_str());
        _console.printf("       %s\n", info.c_str());
        _console.printf("IMEI:  %s\n", imei.c_str());
        _console.printf("IMSI:  %s\n", imsi.c_str());
        _console.printf("ICCID: %s\n", iccid.c_str());
      }
    } else {
      _console.getStream().println(F("Available commands: info, on, off, modem"));
    }
  });
#endif /* NetMgr_Cellular */

#if defined(CONFIG_COMMAND_PREFS)
  _console.addCommand("prefs", [this](int argc, const char** argv) {
    if (argc < 1) {
      // do nothing
    } else if (0 == strcmp(argv[0], "set") && argc == 4) {
      Preferences prefs;
      prefs.begin(argv[1]);
      if (prefs.putString(argv[2], argv[3])) {
        _console.print(R"json({"status":"ok"})json" "\n");
      } else {
        _console.print(R"json({"status":"error"})json" "\n");
      }
      return;
    } else if (0 == strcmp(argv[0], "get") && argc == 3) {
      if (String(argv[1]) == "blynk" && String(argv[2]) == "auth") {
        _console.print(R"json({"status":"error","msg":"not allowed"})json" "\n");
        return;
      }
      Preferences prefs;
      prefs.begin(argv[1], true); // readonly
      if (prefs.isKey(argv[2])) {
        String val = prefs.getString(argv[2], "");
        _console.printf(R"json({"value":"%s"})json" "\n", val.c_str());
      } else {
        _console.print(R"json({"status":"error","msg":"not found"})json" "\n");
      }
      return;
    } else if (0 == strcmp(argv[0], "erase") && argc >= 2) {
      Preferences prefs;
      prefs.begin(argv[1]);
      bool status = ((argc == 2) ? prefs.clear() : prefs.remove(argv[2]));
      if (status) {
        _console.print(R"json({"status":"ok"})json" "\n");
      } else {
        _console.print(R"json({"status":"error"})json" "\n");
      }
      return;
    }

    _console.print(R"json({"status":"error","msg":"invalid args"})json" "\n");
  });
#endif /* CONFIG_COMMAND_PREFS */

#if defined(CONFIG_COMMAND_I2CDETECT)
  _console.addCommand("i2cdetect", [this](const BlynkParam &param) {
    Stream& out = _console.getStream();
    uint8_t first = 0x03, last = 0x77;

    // table header
    out.print("  ");
    for (uint8_t i = 0; i < 16; i++) {
      out.printf(" x%X", i);
    }

    // table body
    for (uint8_t address = 0x00; address <= 0x77; address++) {
      if (address % 16 == 0) {
        out.printf("\n%Xx", address >> 4);
      }
      if (address >= first && address <= last) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
          // device found
          out.printf(" %02x", address);
        } else if (error == 4) {
          // other error
          out.print(" XX");
        } else {
          // error = 2: received NACK on transmit of address
          // error = 3: received NACK on transmit of data
          out.print(" --");
        }
      } else {
        // address not scanned
        out.print("   ");
      }
    }
    out.println("\ndone.");
  });
#endif /* CONFIG_COMMAND_I2CDETECT */

}
