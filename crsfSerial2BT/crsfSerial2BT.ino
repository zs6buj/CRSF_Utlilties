#include "BluetoothSerial.h"
#include "esp_gap_bt_api.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

char  btSlaveName[20] = "btslavename";

#define log                   Serial     // USB / Serial 
//#define inBaud            400000         // crsf Betaflight
#define inBaud            115200         // crsf EdgeTx
#define inSerial          Serial1        // General telemetry input  

#define in_rxPin          27   // uart1
#define in_txPin          17 
bool inInvert = false;         // CRSF, false for telemetry, true for RC

BluetoothSerial SerialBT;

void setup() {
  log.begin(115200);
  delay(2000);
  delay(100);
  inSerial.begin(inBaud, SERIAL_8N1, in_rxPin, in_txPin, inInvert); 
  log.printf("inSerial baud:%u  rxPin:%u  txPin:%u  invert:%u\n", inBaud, in_rxPin, in_txPin, inInvert);
  delay(50);  
  SerialBT.begin(btSlaveName); // slave, passive, advertise this name
  log.printf("Slave BT device started, please pair with %s", btSlaveName);
  /*
   * Set default parameters for Legacy Pairing
   * Use fixed pin code
   */
  esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
  esp_bt_pin_code_t pin_code;
  pin_code[0] = '1';
  pin_code[1] = '1';
  pin_code[2] = '1';
  pin_code[3] = '1';
  esp_bt_gap_set_pin(pin_type, 4, pin_code);
}

void loop() {
  if (inSerial.available()) {
    SerialBT.write(inSerial.read());
  }
  if (SerialBT.available()) {
    inSerial.write(SerialBT.read());
  }
  delay(5);
}
