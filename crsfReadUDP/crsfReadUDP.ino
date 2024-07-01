//#define TELEMETRY_SOURCE  1  // BetaFlight/CF
#define TELEMETRY_SOURCE  2  // EdgeTX/OpenTX

//#define WIFI_MODE   1  //AP
#define WIFI_MODE   2  // STA

#define log               Serial          // USB / Serial 
#if (TELEMETRY_SOURCE  == 1)              // Telemetry from BetaFlight/CF
  #define in_baud          420000
#elif (TELEMETRY_SOURCE  == 2)            // EdgeTX/OpenTx
  #define in_baud          115200         // Telemetry from RadioMaster TX16S AUX2
#endif

#define inSerial          Serial1         // General telemetry input  
#define in_rxPin          27   // uart1
#define in_txPin          17 
bool inInvert = false;         // CRSF, false for telemetry, true for RC

const uint8_t max_buff = 64;
uint8_t buff[max_buff] = {};

#if (TELEMETRY_SOURCE  == 1)      // BetaFlight
  #define CRSF_TEL_SYNC_BYTE  0xC8 
#elif (TELEMETRY_SOURCE  == 2)    // EdgeTX/OpenTx
  #define CRSF_TEL_SYNC_BYTE  0xEA  
#endif

// Forward declarations
bool sendUDP(uint8_t);
bool readUDP(uint8_t);

//==================================
void printByte(byte b) 
{
  if (b<=0xf) log.print("0");
    log.print(b,HEX);
    log.print(" ");
}
//======================================= 
void printBuffer(uint8_t lth)
{
  log.printf("len %2d:", lth);
  for ( int i = 0; i < lth; i++ ) 
  {
    printByte(buff[i]);
  }
  log.println();
}
//===============================
void setup() {
  log.begin(115200);
  delay(2000);
  log.println("Starting ....."); 
  #if (WIFI_MODE == 1)  // AP
    startAccessPoint();
    log.println("Waiting for UDP clients to connect ...");
  #endif  
  #if (WIFI_MODE == 2)  // STA
    startStation();
  #endif   
}

void loop() {
  uint16_t len = readUDP();
  if (len) 
  {
    printBuffer(len);
  }
  serviceWiFiRoutines();
  delay(2);
}
