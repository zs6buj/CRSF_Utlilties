/*
  CLIENT or PERIPHERAL
*/

#include "BLEDevice.h"

#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59"
#define CHARACTERISTIC_01_UUID "cba1d466-344c-4be3-ab3f-189f80dd7518"
#define bleServerName "AntTrack"

// BLE Service
static BLEUUID bmeServiceUUID(SERVICE_UUID);

// BLE Characteristics
  static BLEUUID recordCharacteristicUUID(CHARACTERISTIC_01_UUID);

//Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean connected = false;

//Address of the peripheral device. Address will be found during scanning...
static BLEAddress *pServerAddress;
 
//Characteristics that we want to read
static BLERemoteCharacteristic* recordCharacteristic;

//Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

//Variables to store record
char* recordChar;

//Flags to check whether new temperature and humidity readings are available
boolean newRecord = false;
boolean newHumidity = false;

//Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAddress pAddress) {
   BLEClient* pClient = BLEDevice::createClient();
 
  // Connect to the remote BLE Server.
  pClient->connect(pAddress);
  Serial.println("connected to remote BLE server");
 
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) 
  {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    return (false);
  }
   Serial.println("our service found");

  // Obtain a reference to the characteristics in the service of the remote BLE server.
  recordCharacteristic = pRemoteService->getCharacteristic(recordCharacteristicUUID);

  if (recordCharacteristic == nullptr)  
  {
    Serial.print("failed to find our characteristic UUID");
    return false;
  }
  Serial.println("our characteristics found");
  //Assign callback functions for the Characteristics
  recordCharacteristic->registerForNotify(recordNotifyCallback);
  return true;
}

//Callback function called when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks 
{
  void onResult(BLEAdvertisedDevice advertisedDevice) 
  {
    Serial.printf("Device:%s\n", advertisedDevice.toString().c_str());
    if (advertisedDevice.getName() == bleServerName) { //Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop(); //Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); //Address of advertiser is the one we need
      doConnect = true; //Set indicator, stating that we are ready to connect
      Serial.printf("our device found, connecting to %S...\n", bleServerName);
    }
  }
};
 
//When the BLE Server sends a new  reading with the notify property
static void recordNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                        uint8_t* pData, size_t length, bool isNotify) {
  //store record value
  Serial.printf("length:%u  ", length);
  recordChar = (char*)pData;
  newRecord = true;
}

//function that prints the latest record
void printReadings(){
  Serial.print("Record:");
  Serial.println(recordChar);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting BLE Client ...");

  //Init BLE device
  BLEDevice::init("");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  Serial.printf("Scanning for device \"%s\"\n", bleServerName);
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.printf("fully connected to BLE server %S\n", bleServerName);
      //Activate the Notify property of each Characteristic
      recordCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      ///humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
    } else {
      Serial.println("failed to connect to the server. restart to scan for server again.");
    }
    doConnect = false;
  }
  //if new temperature readings are available, print in the OLED
  if (newRecord)
  {
    newRecord = false;
    printReadings();
  }
  delay(1000); // Delay a second between loops.
}