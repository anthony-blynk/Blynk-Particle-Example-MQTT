#include <Particle.h>
#include <queue>

#if !defined(PARTICLE)
  #error "ConfigSparkBLE.h should be used on Particle platform"
#endif

#if !Wiring_BLE
  #error "Blynk.Inject works using BLE, the target platform lacks BLE support"
#endif

constexpr static char SERVICE_UUID[]            = "95e30001-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RX[]  = "95e30002-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_TX[]  = "95e30003-5737-45a9-a092-a88e2e5dd659";

class ConfigBLE
{

public:
    ConfigBLE() {}

    void begin(const char* name) {
        _name = name;
        _active = true;

        BLE.on();

        if (!_queue_mutex) {
            _queue_mutex = new Mutex();

            _tx_char = new BleCharacteristic(nullptr,
                            BleCharacteristicProperty::NOTIFY,
                            CHARACTERISTIC_UUID_TX, SERVICE_UUID);
            _rx_char = new BleCharacteristic(nullptr,
                            BleCharacteristicProperty::WRITE_WO_RSP | BleCharacteristicProperty::WRITE,
                            CHARACTERISTIC_UUID_RX, SERVICE_UUID, ble_data_callback, this);

            BLE.addCharacteristic(*_tx_char);
            BLE.addCharacteristic(*_rx_char);

            // Preferred connection parameters: 7.5-15ms interval, no slave latency, 2s supervision timeout
            BLE.setPPCP(6, 12, 0, 200);

            BLE.onConnected([this](const BlePeerDevice& peer) {
                LOG_I("BLE connected");
            });
            BLE.onDisconnected([this](const BlePeerDevice& peer) {
                LOG_I("BLE disconnected");
                WITH_LOCK(*_queue_mutex) {
                    while (!_rx_queue.empty()) {
                        free(_rx_queue.front());
                        _rx_queue.pop();
                    }
                }
                if (_active) {
                    startAdvertising();
                }
            });
            BLE.onAttMtuExchanged([this](const BlePeerDevice& peer, size_t attMtu) {
                LOG_I("BLE MTU updated: %u", (unsigned)attMtu);
            });
        }

        startAdvertising();
    }

    void end() {
        _active = false;

        BLE.disconnectAll();
        BLE.stopAdvertising();
        BLE.off();

        if (_queue_mutex) {
            WITH_LOCK(*_queue_mutex) {
                while (!_rx_queue.empty()) {
                    free(_rx_queue.front());
                    _rx_queue.pop();
                }
            }
        }
    }

    size_t write(const void* buf, size_t len) {
        ssize_t sent = _tx_char->setValue((const uint8_t*)buf, len);
        if (sent < 0) {
            LOG_E("BLE notify failed: %d", (int)sent);
            return 0;
        }

        LOG_I("<< %.*s", (int)len, (const char*)buf);
        return len;
    }

    size_t write(const char* buf) {
        unsigned len = strlen(buf);
        return write(buf, len);
    }

    String read() {
      String result;
      WITH_LOCK(*_queue_mutex) {
        char* msg = _rx_queue.front();
        result = msg;
        free(msg);
        _rx_queue.pop();
      }
      return result;
    }

    bool available() {
        return !_rx_queue.empty();
    }

    bool isConnected() {
        return BLE.connected();
    }

private:

    void startAdvertising() {
        BleAdvertisingData advData, scanRspData;
        advData.appendServiceUUID(SERVICE_UUID);

        // Construct Scan Responce Data manually to bypass Particle limitation
        //unsigned name_len = strlen(name);
        //uint8_t buffer[32];
        //buffer[0] = name_len+1;
        //buffer[1] = BLE_SIG_AD_TYPE_COMPLETE_LOCAL_NAME;
        //memcpy(&buffer[2], name, name_len);
        //scanRspData.set(buffer, name_len+2);

        // A bit cleaner way:
        scanRspData.clear();
        scanRspData.append(BleAdvertisingDataType::COMPLETE_LOCAL_NAME,
                          (const uint8_t*)_name.c_str(), _name.length());

        // TODO: set device name in GENERIC_ACCESS_SVC->DEVICE_NAME_CHAR
        // Particle seemingly lacks API to do that

        BLE.advertise(&advData, &scanRspData);
    }

    static void ble_data_callback(const uint8_t* data, size_t len,
                                  const BlePeerDevice& peer, void* self)
    {
        ((ConfigBLE*)self)->onWrite(data, len);
    }

    void onWrite(const uint8_t* data, size_t len) {
      if (data && len > 0) {
        char* msg = (char*)malloc(len+1);
        memcpy(msg, data, len);
        msg[len] = 0;   // Null-terminate string
        LOG_I(">> %s", msg);
        WITH_LOCK(*_queue_mutex) {
          _rx_queue.push(msg);
        }
      }
    }

private:
    String                  _name;
    bool                    _active = false;
    std::queue<char*>       _rx_queue;
    Mutex*                  _queue_mutex = nullptr;
    BleCharacteristic*      _rx_char = nullptr;
    BleCharacteristic*      _tx_char = nullptr;
};
