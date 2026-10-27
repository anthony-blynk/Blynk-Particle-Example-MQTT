/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2023 Volodymyr Shymanskyy
 */

#ifndef NetMgrParticleEthernet_h
#define NetMgrParticleEthernet_h

class NetMgrParticleEthernet
{

public:
    NetMgrParticleEthernet() {}

    ~NetMgrParticleEthernet() {
        this->off();
    }

    void begin() {
        System.enableFeature(FEATURE_ETHERNET_DETECTION);
    }

    void startConfig() {
        on();
        Ethernet.listen(false);
    }

    bool isConfigured() {
        return isHardwareAvailable();
    }

    void on() {
        // Retry connection. Sometimes fixes cable detection issues
        if (Ethernet.connecting()) {
            Ethernet.disconnect();
        }
        Ethernet.on();
        Ethernet.connect();
    }

    void off() {
        Ethernet.disconnect();
        Ethernet.off();
    }

    void setHostname(const String& hostname) {
        // TODO: no API for this
    }

    bool isHardwareAvailable() {
        static bool cached = detectEthernet();
        return cached;
    }

    bool supportsStaticIP() { return false; }

    bool isConnected() {
        return Ethernet.ready();
    }

    const char* getErrorStr() {
        return NULL;
    }

    const char* getStateStr() {
        return "UNKNOWN";
    }

    String getMacAddress() {
        byte mac[6];
        memset(mac, 0, sizeof(mac));
        Ethernet.macAddress(mac);
        return macToString(mac);
    }

    String getLocalIP() {
        return Ethernet.localIP().toString();
    }

    String getStatus() {
        if (Ethernet.connecting()) {
          return Ethernet.localIP() ? "down" : "up";
        }
        if (Ethernet.ready()) {
          return "ready";
        }
        return "off";
    }

    void clearNetworks() {
    }

    void run() {
        // do nothing
    }

private:
    static
    bool detectEthernet() {
      if (!System.featureEnabled(FEATURE_ETHERNET_DETECTION)) {
        return false;
      }
      byte mac[6] = { 0, };
      return (Ethernet.macAddress(mac) != 0);
    }

};

#endif /* NetMgrParticleEthernet_h */
