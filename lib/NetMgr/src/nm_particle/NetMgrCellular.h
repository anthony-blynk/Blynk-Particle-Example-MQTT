/**
 * @author     Volodymyr Shymanskyy
 * @copyright  Copyright (c) 2023 Volodymyr Shymanskyy
 */

#ifndef NetMgrParticleCellular_h
#define NetMgrParticleCellular_h

#if defined(NETMGR_USE_LIB_CELLULAR_HELPER)
#include "CellularHelper.h"
#endif

class NetMgrParticleCellular
{

public:
    NetMgrParticleCellular() {
    }

    ~NetMgrParticleCellular() {
        this->off();
    }

    void begin() {
    }

    void startConfig() {
        // This turns on modem, but also waits until it boots-up
        cellular_on(NULL);
        Cellular.listen(false);
    }

    bool isConfigured() {
        // Assume cellular connection is always possible
        return true;
    }

    void on() {
        Cellular.on();
        Cellular.connect();
    }

    void off() {
        Cellular.disconnect();
        Cellular.off();
    }

    bool isHardwareAvailable() {
        return true;
    }

    bool supportsScan()   { return false; }
    bool supportsSimPin() { return false; }
    bool supportsAPN()    { return false; }

    bool isConnected() {
        return Cellular.ready();
    }

    String getModemName() {
        return "Built-in";
    }

    String getModemInfo() {
        return "";
    }

    const char* getErrorStr() {
        return NULL;
    }

    const char* getStateStr() {
        return "UNKNOWN";
    }

    String getStatus() {
        if (Cellular.connecting()) {
          return "up";
        }
        if (Cellular.ready()) {
          return "ready";
        }
        return "off";
    }

    String getLocalIP() {
        return Cellular.localIP().toString();
    }

    int getSignalStrength() {
        CellularSignal sig = Cellular.RSSI();
        return sig.getStrength();
    }


#if defined(NETMGR_USE_LIB_CELLULAR_HELPER)

    String getICCID() {
      return CellularHelper.getICCID();
    }

    String getIMSI() {
      return CellularHelper.getIMSI();
    }

    String getIMEI() {
      return CellularHelper.getIMEI();
    }

    String getOperator() {
        return CellularHelper.getOperatorName();
    }

#else

    static
    int modemCommandCbkPlusUDOPN(int type, const char* buf, int len, char* res)
    {
      if (!res) return WAIT;
      if (type == TYPE_PLUS) {
        char buf2[48];
        int n = 0;
        sscanf(buf, "\r\n+UDOPN: %[^\r]\r\n", buf2);
        sscanf(buf2, "%d,\"%[^\"]", &n, res);
      }
      return WAIT;
    }

    static
    int modemCommandCbkPlusCCID(int type, const char* buf, int len, char* res)
    {
      if (!res) return WAIT;
      if (type == TYPE_PLUS) {
        sscanf(buf, "\r\n+CCID: %[^\r]\r\n", res);
      }
      return WAIT;
    }

    static
    int modemCommandCbkUnknown(int type, const char* buf, int len, char* res)
    {
      if (!res) return WAIT;
      if (type == TYPE_UNKNOWN) {
        int i = 0, j = 0;
        while (i < len) {
          char c = buf[i];
          if (!(c == '\r' || c == '\n')) {
            res[j++] = c;
          }
          i++;
        }
        res[j] = '\0';
      }
      return WAIT;
    }

    String getICCID() {
      char iccid[32] = "";
      if (RESP_OK == Cellular.command(modemCommandCbkPlusCCID, iccid, 1000, "AT+CCID\r\n")) {
        unsigned len = strnlen(iccid, sizeof(iccid));
        if (len > 0 && len < sizeof(iccid)) {
          return iccid;
        }
      }
      return "";
    }

    String getIMEI() {
      char imei[32] = "";
      if (RESP_OK == Cellular.command(modemCommandCbkUnknown, imei, 1000, "AT+CGSN\r\n")) {
        unsigned len = strnlen(imei, sizeof(imei));
        if (len > 0 && len < sizeof(imei)) {
          return imei;
        }
      }
      return "";
    }

    String getIMSI() {
      char imei[32] = "";
      if (RESP_OK == Cellular.command(modemCommandCbkUnknown, imei, 1000, "AT+CIMI\r\n")) {
        unsigned len = strnlen(imei, sizeof(imei));
        if (len > 0 && len < sizeof(imei)) {
          return imei;
        }
      }
      return "";
    }

    String getOperator() {
      const int OPERATOR_NAME_LONG_EONS = 9;
      char name[48] = "";
      if (RESP_OK == Cellular.command(modemCommandCbkPlusUDOPN, name, 1000, "AT+UDOPN=%d\r\n", OPERATOR_NAME_LONG_EONS)) {
        unsigned len = strnlen(name, sizeof(name));
        if (len > 0 && len < sizeof(name)) {
          return name;
        }
      }
      return "";
    }

#endif

    void clearNetworks() {
    }

    void run() {
        // do nothing
    }

};

#endif /* NetMgrParticleCellular_h */
