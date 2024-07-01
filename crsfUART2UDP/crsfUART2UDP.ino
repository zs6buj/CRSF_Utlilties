#include <terseCRSF.h>  

//#define TELEMETRY_SOURCE  1  // BetaFlight/CF
#define TELEMETRY_SOURCE  2  // EdgeTX/OpenTX

//#define WIFI_MODE   1  //AP
//#define WIFI_MODE   2  // STA

#define log               Serial          // USB / Serial 

#if defined RC_BUILD
    #define crsf_rxPin      13      // Signal tx pin, transmitter, in back bay
    #define crsf_txPin      14      // 
    #define crsf_invert     true
#else
    #define crsf_invert     false
    #define crsf_rxPin      27      
    #define crsf_txPin      17       
#endif

#define crsf_uart            1              // Serial1
#if (TELEMETRY_SOURCE  == 1)                // Telemetry from BetaFlight/CF
  #define crsf_baud          420000
#elif (TELEMETRY_SOURCE  == 2)              // EdgeTX/OpenTx
  #define crsf_baud          115200         // Telemetry from RadioMaster TX16S AUX2
#endif
HardwareSerial crsfSerial(crsf_uart);       // instantiate Serial object

CRSF crsf;            // instantiate CRSF object

//===============================
void setup() {
  log.begin(115200);
  delay(2000);
  log.println("Starting .....");
  crsfSerial.begin(crsf_baud, SERIAL_8N1, crsf_rxPin, crsf_txPin, crsf_invert);
  log.printf("CRFS uart:%u  baud:%u  rxPin:%u  txPin:%u  invert:%u\n", crsf_uart, crsf_baud, crsf_rxPin, crsf_txPin, crsf_invert);
  crsf.initialise(crsfSerial);  

  #if (WIFI_MODE == 1)  // AP
    startAccessPoint();
    log.println("Waiting for UDP clients to connect ...");  
  #endif  
  #if (WIFI_MODE == 2)  // STA
    startStation();
  #endif    
}

void loop() 
{
  if (crsf.readCrsfFrame(crsf.frame_lth))  
  {
      sendUDP(&*crsf.crsf_buf, crsf.frame_lth);
      //crsf.printBytes(&*crsf.crsf_buf, crsf.frame_lth);       
  }

  serviceWiFiRoutines();
  delay(2);
}
