#include <iostream>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <terseCRSF.h>  
#include <CobsTranscoder.hpp>

//#define TELEMETRY_SOURCE  1  // BetaFlight/CF
#define TELEMETRY_SOURCE  2  // EdgeTX/OpenTX

#define crsf_invert     false
#define crsf_rxPin      27      
#define crsf_txPin      17   

#define log               Serial         // USB / Serial 

#define crsf_uart            1              // Serial1
#if (TELEMETRY_SOURCE  == 1)                // Telemetry from BetaFlight/CF
  #define crsf_baud          420000
#elif (TELEMETRY_SOURCE  == 2)              // EdgeTX/OpenTx
  #define crsf_baud          115200         // Telemetry from RadioMaster TX16S AUX2
#endif
HardwareSerial crsfSerial(crsf_uart);       // instantiate Serial object

bool deviceConnected = false;
CRSF crsf;            // instantiate CRSF object

//#define Frsky_Tandem_SERVICE_UUID "0000fff0-0000-1000-8000-00805f9b34fb"
#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59"
#define CHARACTERISTIC_MSG_UUID "cba1d466-344c-4be3-ab3f-189f80dd7518"
#define bleServerName "AntTrack"
//#define bleServerName "Hello"  // Frsky_Tandem

// msg Characteristic and Descriptor
BLECharacteristic msgCharacteristic(CHARACTERISTIC_MSG_UUID, BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor msgDescriptor(BLEUUID((uint16_t)0x2902));

// ==========================  Cobs class data
ByteArray  rawData;   
ByteArray  encodedData;   
//ByteArray  decodedData;   

CobsTranscoder cobs;
//============================

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

void setup() {
  log.begin(115200);
  delay(2000);
  String pgm_path = __FILE__; 
  String pgm_name = pgm_path.substring(pgm_path.lastIndexOf("\\") + 1);
  pgm_name = pgm_name.substring(0, pgm_name.lastIndexOf('.')); // remove the extension
  log.print("\nStarting ");
  log.println(pgm_name);
  crsfSerial.begin(crsf_baud, SERIAL_8N1, crsf_rxPin, crsf_txPin, crsf_invert);
  log.printf("CRFS uart:%u  baud:%u  rxPin:%u  txPin:%u  invert:%u\n", crsf_uart, crsf_baud, crsf_rxPin, crsf_txPin, crsf_invert);
  crsf.initialise(crsfSerial);  
  delay(10);

  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.printf("BLE Server %s started\n", bleServerName);
  // Create the BLE Service
  BLEService *anttrackService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics and Create a BLE Descriptor
    anttrackService->addCharacteristic(&msgCharacteristic);
    msgDescriptor.setValue("tracker_msg");
    msgCharacteristic.addDescriptor(&msgDescriptor);

  // Start our service
  anttrackService->start();
  Serial.printf("BLE Service started\n");
  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a BLE client to notify connect...");
}

void loop() 
{
  static bool ft = true;
  if (deviceConnected) 
  { 
    if (ft)
    {
      ft = false;
      log.println("Client connected");
    }
    if (crsf.readCrsfFrame(crsf.frame_lth))  
    {
      uint8_t len = crsf.frame_lth;
      uint8_t crsf_id = crsf.decodeTelemetry(&*crsf.crsf_buf, len);
      if (crsf_id != 0x14)  // don't need link statistics 
      {
        for (auto i = 0; i < crsf.frame_lth; i++)
        {
          rawData.push_back(crsf.crsf_buf[i]);
        }
        cobs.encode(rawData, encodedData);
        rawData.clear();

        uint8_t len = encodedData.size();
        //log.printf("len:%2u encoded:", len);
        for (auto i = 0; i < len; i++) 
        {
          crsf.crsf_buf[i] = encodedData[i];
          //crsf.printByte(encodedData[i], ' ');
        };
        encodedData.clear();
        //Set  Characteristic value and notify connected client
        msgCharacteristic.setValue((char*)crsf.crsf_buf);
        msgCharacteristic.notify();
        //log.printf("len:%2u crsf_buf:", len);
        //crsf.printBytes(&*crsf.crsf_buf, len);
      }

      if (crsf_id == GPS_ID) 
      {
      #if defined DEMO_CRSF_GPS    
        log.print("GPS id:");
        crsf.printByte(crsf_id, ' ');
        log.printf("lat:%2.7f  lon:%2.7f", crsf.gpsF_lat, crsf.gpsF_lon);
        log.printf("  ground_spd:%.1fkm/hr", crsf.gpsF_groundspeed);
        log.printf("  hdg:%.2fdeg", crsf.gpsF_heading);
        log.printf("  alt:%dm", crsf.gps_altitude);
        log.printf("  sats:%d\n", crsf.gps_sats); 
        #endif     
      }

      if (crsf_id == BATTERY_ID) // 0x08
      { 
      #if defined DEMO_CRSF_BATTERY         
        log.print("BATTERY id:");
        crsf.printByte(crsf_id, ' ');
        log.printf("volts:%2.1f", crsf.batF_voltage);
        log.printf("  amps:%3.1f", crsf.batF_current);
        log.printf("  Ah_drawn:%3.1f", crsf.batF_fuel_drawn);
        log.printf("  remaining:%3u%%\n", crsf.bat_remaining);
      #endif 
      }
    
      if (crsf_id == LINK_ID) // 0x14 Link statistics
      {
      #if defined DEMO_CRSF_LINK 
        log.print("LINK id:");     
        crsf.printByte(crsf_id, ' ');
        log.printf("  up_rssi_ant_1:%ddB", crsf.link_up_rssi_ant_1 * -1);  
        log.printf("  up_rssi_ant_2:%ddB", crsf.link_up_rssi_ant_2 * -1);  
        log.printf("  up_quality:%d%%", crsf.link_up_quality);  
        log.printf("  up_snr:%ddB", crsf.link_up_snr);
        log.printf("  diversity_active_ant:%d", crsf.link_diversity_active_ant);  
        log.printf("  rf_mode:%d", crsf.link_rf_mode);  
        log.printf("  up_tx_power:%d", crsf.link_up_tx_power);  
        log.printf("  dn_rssi:%ddB", crsf.link_dn_rssi * -1);  
        log.printf("  dn_quality:%d%%", crsf.link_dn_quality);  
        log.printf("  up_rssi_ant_1:%d", crsf.link_up_rssi_ant_1);  
        log.printf("  link_dn_snr:%ddB\n", crsf.link_dn_snr);        
      #endif      
      }
      if (crsf_id == ATTITUDE_ID) // 0x1E
      {
      #if defined DEMO_CRSF_ATTITUDE 
        log.print("ATTITUDE id:");
        crsf.printByte(crsf_id, ' '); 
        log.printf("pitch:%3.1fdeg", crsf.attiF_pitch);
        log.printf("  roll:%3.1fdeg", crsf.attiF_roll);
        log.printf("  yaw:%3.1fdeg\n", crsf.attiF_yaw);  
      #endif          
      }    

      if (crsf_id == FLIGHT_MODE_ID)
      {
      #if defined DEMO_CRSF_FLIGHT_MODE 
        log.print("FLIGHT_MODE id:");
        crsf.printByte(crsf_id, ' ');
        log.printf("lth:%u %s\n", crsf.flight_mode_lth, &crsf.flightMode);
      #endif
      }          
    }
  } else
  {  // if device not connected
    if (!ft) 
    {
    log.println("Client disconnected");
    ft = true;
    }
  }
  delay(1);
}
