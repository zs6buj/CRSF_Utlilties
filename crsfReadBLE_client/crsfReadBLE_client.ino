/*
  zs6buj
  CRSF Utilities
  Read BLE example

*/

#include "BLEDevice.h"
#include <terseCRSF.h>  
#include <CobsTranscoder.hpp>

#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59"
#define CHARACTERISTIC_MSG_UUID "cba1d466-344c-4be3-ab3f-189f80dd7518"
#define bleServerName "AntTrack"

static BLEUUID bmeServiceUUID(SERVICE_UUID);
static BLEUUID msgCharacteristicUUID(CHARACTERISTIC_MSG_UUID);
static boolean doConnect = false;
static boolean connected = false;

//address of the peripheral device. Address will be found during scanning...
static BLEAddress *pServerAddress;
//Characteristics that we want to read
static BLERemoteCharacteristic* msgCharacteristic;
//Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

boolean msgNew = false;
uint16_t msgLen = 0;

#define log               Serial         // USB / Serial 

CRSF crsf;            // instantiate CRSF object

// ==========================  Cobs class data
//ByteArray  rawData;   
ByteArray  encodedData;   
ByteArray  decodedData;   

CobsTranscoder cobs;
//============================

//Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAddress pAddress) 
{
   BLEClient* pClient = BLEDevice::createClient();
 
  // Connect to the remote BLE Server.
  pClient->connect(pAddress);
  log.println("connected to remote BLE server");
 
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) 
  {
    log.print("Failed to find our service UUID: ");
    log.println(bmeServiceUUID.toString().c_str());
    return (false);
  }
   log.println("our crsf service found");

  // Obtain a reference to the characteristics in the service of the remote BLE server.
  msgCharacteristic = pRemoteService->getCharacteristic(msgCharacteristicUUID);

  if (msgCharacteristic == nullptr)  
  {
    log.print("failed to find our msg characteristic UUID");
    return false;
  }
  log.println("our msg characteristics found");
  //assign callback functions for the Characteristic
  msgCharacteristic->registerForNotify(recordNotifyCallback);
  return true;
}

//Callback function called when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks 
{
  void onResult(BLEAdvertisedDevice advertisedDevice) 
  {
    //log.printf("Device:%s\n", advertisedDevice.toString().c_str());
    if (advertisedDevice.getName() == bleServerName) { //Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop(); //Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); //Address of advertiser is the one we need
      doConnect = true; //Set indicator, stating that we are ready to connect
      log.printf("our device found, connecting to %S...\n", bleServerName);
    }
  }
};
 
//When the BLE Server sends a new  msg with the notify property
static void recordNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                        uint8_t* pData, size_t length, bool isNotify) 
{
  //log.printf("length:%u ", length);
  //log.print("pData:");
  //crsf.printBytes(pData, Length);
  //store new message
  memcpy(crsf.crsf_buf, pData, length);
  msgNew = true;
  msgLen = length;
}

void setup() {
  log.begin(115200);
  delay(2000);
  String pgm_path = __FILE__; 
  String pgm_name = pgm_path.substring(pgm_path.lastIndexOf("\\") + 1);
  pgm_name = pgm_name.substring(0, pgm_name.lastIndexOf('.')); // remove the extension
  log.print("\nStarting ");
  log.println(pgm_name);

  //Init BLE device
  BLEDevice::init("");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  log.printf("scanning for device \"%s\"\n", bleServerName);
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      log.println("fully connected to BLE server");
      //Activate the Notify property of each Characteristic
      msgCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 20, true);
      connected = true;
    } else {
      log.println("failed to connect to the server. restart to scan for server again.");
    }
    doConnect = false;
  }
  //if new message is available, printit
  if (msgNew)
  {
    msgNew = false;
    uint8_t len = msgLen;
    //log.print("buf before:");
    //crsf.printBytes(&*crsf.crsf_buf, len); 

    for (auto i = 0; i < len; i++)
    {
      encodedData.push_back(crsf.crsf_buf[i]);
    }
    cobs.decode(encodedData, decodedData);
    encodedData.clear();
    len = decodedData.size();
    //log.printf("len:%2u after:", len);
    for (auto i = 0; i < len; i++) 
    {
      //crsf.printByte(decodedData[i], ' ');
      crsf.crsf_buf[i] = decodedData[i];
    };
    //log.println();
    decodedData.clear();
    //log.print("buf after:");
    //crsf.printBytes(&*crsf.crsf_buf, len); 

    uint8_t crsf_id = crsf.decodeTelemetry(&*crsf.crsf_buf, len);

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
  delay(1); 
}