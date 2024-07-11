#include <Arduino.h>
#include <iostream>
#include <CobsTranscoder.hpp>
#include <terseCRSF.h>

#define crsf_invert     false
#define crsf_rxPin      27      //16 YELLOW rx from GREEN FC tx
#define crsf_txPin      17      // GREEN tx to YELLOW FC rx    

#define crsf_uart            1              // Serial1
#if (TELEMETRY_SOURCE  == 1)                // Telemetry from BetaFlight/CF
  #define crsf_baud          420000
#elif (TELEMETRY_SOURCE  == 2)              // EdgeTX/OpenTx
  #define crsf_baud          115200         // Telemetry from RadioMaster TX16S AUX2
#endif

#define log   Serial
HardwareSerial crsfSerial(crsf_uart);       // instantiate Serial object

ByteArray  rawData;   
ByteArray  encodedData;   
ByteArray  decodedData;   
//ByteArray::iterator iter;

CobsTranscoder cobs;
CRSF crsf;  

void setup() 
{
  log.begin(115200);
  delay(2000);
  std::cout << "Starting Cobs encoder example" << std::endl;
  crsfSerial.begin(crsf_baud, SERIAL_8N1, crsf_rxPin, crsf_txPin, crsf_invert);
  log.printf("CRFS uart:%u  baud:%u  rxPin:%u  txPin:%u  invert:%u\n", crsf_uart, crsf_baud, crsf_rxPin, crsf_txPin, crsf_invert);
  crsf.initialise(crsfSerial);

}

void loop() 
{
  if (crsf.readCrsfFrame(crsf.frame_lth))  // exposes discovered frame_lth if needed
  {
    log.printf("len:%2u Before:", crsf.frame_lth);
    crsf.printBytes(&*crsf.crsf_buf, crsf.frame_lth);
    for (auto i = 0; i < crsf.frame_lth; i++)
    {
      rawData.push_back(crsf.crsf_buf[i]);
    }

    cobs.encode(rawData, encodedData);
    rawData.clear();
    uint8_t len = encodedData.size();
    log.printf("len:%2u during:", len);
    for (uint8_t i = 0; i < len; i++) 
    {
      crsf.printByte(encodedData[i], ' ');
    };
    log.println();

    cobs.decode(encodedData, decodedData);
    encodedData.clear();
    len = decodedData.size();
    log.printf("len:%2u after:", len);
    for (uint8_t i = 0; i < len; i++) 
    {
      crsf.printByte(decodedData[i], ' ');
      crsf.crsf_buf[i] = decodedData[i];
    };
    log.println();
    decodedData.clear();

    log.printf("len:%2u Finished:", len);
    crsf.printBytes(&*crsf.crsf_buf, len);
    log.println();
  }
}
