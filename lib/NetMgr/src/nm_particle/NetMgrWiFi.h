/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2023 Volodymyr Shymanskyy
 */

#ifndef NetMgrParticleWiFi_h
#define NetMgrParticleWiFi_h

class NetMgrParticleWiFi
{

public:
    NetMgrParticleWiFi() {}

    ~NetMgrParticleWiFi() {
        this->off();
    }

    void begin() {
    }

    void startConfig() {
        WiFi.on();
        WiFi.listen(false);
    }

    bool isConfigured() {
        return WiFi.hasCredentials();
    }

    void on() {
        // Re-connect
        if (WiFi.ready() || WiFi.connecting()) {
          WiFi.disconnect();
        }

        WiFi.connect(WIFI_CONNECT_SKIP_LISTEN);
    }

    void off() {
        WiFi.off();
    }

    void setHostname(const String& hostname) {
        // not avail on Argon
    }

    bool isHardwareAvailable() {
        return true;
    }

    bool supportsScan() { return true; }
    bool supports5GHz() {
#if PLATFORM_ID == PLATFORM_P2
        return true;
#else
        return false;
#endif
    }
    bool supportsStaticIP() { return false; }  // not avail on Argon

    bool isConnected() {
        return WiFi.ready();
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
        WiFi.macAddress(mac);
        return macToString(mac);
    }

    String getLocalIP() {
        return WiFi.localIP().toString();
    }

    String getStatus() {
        if (WiFi.connecting()) {
          return WiFi.localIP() ? "down" : "up";
        }
        if (WiFi.ready()) {
          return "ready";
        }
        return "off";
    }

    String getNetworkSSID() {
        return WiFi.SSID();
    }

    String getNetworkBSSID() {
        byte bssid[6] = { 0, };
        WiFi.BSSID(bssid);
        return macToString(bssid);
    }

    int getRSSI() {
        return WiFi.RSSI();
    }

    int scanNetworks() {
        if (!_scanResults) {
            _scanResults = new WiFiAccessPoint[15];
        }
        _scanResultsQty = WiFi.scan(_scanResults, 15);
        return _scanResultsQty;
    }

    void scanDelete() {
        if (_scanResults) {
            delete[] _scanResults;
            _scanResults = nullptr;
            _scanResultsQty = 0;
        }
    }

    bool scanGetResult(int i, String& ssid, String& sec,
                       int& rssi, String& bssid, int& chan)
    {
        if (!_scanResults || i < 0 || i >= _scanResultsQty) {
            return false;
        }

        WiFiAccessPoint& ap = _scanResults[i];
        ssid  = ap.ssid;
        bssid = macToString(ap.bssid);
        rssi  = ap.rssi;
        sec   = wifiSecToStr(ap.security);
        chan  = ap.channel;
        return true;
    }

    bool addNetwork(const String& ssid) {
        return addNetwork(ssid, "");
    }

    bool addNetwork(const String& ssid, const String& psk) {
        if (!ssid.length() || ssid.length() > 31) {
            LOG_E("No ssid or ssid too long");
            return false;
        }

        if (psk.length()) {
            if (psk.length() > 64) {
                LOG_E("Passphrase too long");
                return false;
            } else if (psk.length() < 8) {
                LOG_E("Passphrase too short");
                return false;
            }
        }

        WiFi.on();
        if (psk.length()) {
            WiFi.setCredentials(ssid, psk);
        } else {
            WiFi.setCredentials(ssid);
        }

        return true;
    }

    void clearNetworks() {
        WiFi.clearCredentials();
    }

    void run() {
        // do nothing
    }

public:

    static inline
    const char* wifiSecToStr(WLanSecurityType t) {
      switch (t) {
        case UNSEC:                 return "OPEN";
        case WEP:                   return "WEP";
        case WPA:                   return "WPA";
        case WPA2:                  return "WPA2";
        case WPA_ENTERPRISE:        return "WPA-EAP";
        case WPA2_ENTERPRISE:       return "WPA2-EAP";
        default:                    return "unknown";
      }
    }

    static inline
    const char* wifiCipherToStr(WLanSecurityCipher t) {
      switch (t) {
        case WLAN_CIPHER_NOT_SET:   return "NONE";
        case WLAN_CIPHER_AES:       return "AES";
        case WLAN_CIPHER_TKIP:      return "TKIP";
        case WLAN_CIPHER_AES_TKIP:  return "AES+TKIP";
        default:    return "unknown";
      }
    }

private:
    WiFiAccessPoint* _scanResults = nullptr;
    int              _scanResultsQty = 0;
};

#endif /* NetMgrParticleWiFi_h */
