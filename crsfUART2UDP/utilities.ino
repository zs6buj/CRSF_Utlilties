    #include <WiFi.h>  // includes UDP class
    #include <WebServer.h>
    #include <ESPmDNS.h>
    //WebServer server(80);
    #include <WiFiAP.h> 
    
    #define HostName    "crsfUDP"  
    #define APssid      "crsfUDP"
    #define APpw        "password"         // Change me! Must be >= 8 chars  
    #define STAssid     "crsfUDP"
    #define STApw       "password"         // Change me! Must be >= 8 chars  

//#define WIFI_PROTOCOL 1    // TCP/IP
    #define WIFI_PROTOCOL 2    // UDP 

    IPAddress AP_default_IP(192, 168, 4, 1);
    IPAddress AP_gateway(192, 168, 4, 1);
    IPAddress AP_mask(255, 255, 255, 0);

    IPAddress localIP;                            // tcp and udp
    IPAddress TCP_REMOTEIP(192, 168, 4, 1);       // when we connect to a server in tcp client mode, put the server IP here

    uint16_t  UDP_LOCALPORT = 14555;    // readPort - (default 14555) remote host (like MP and QGC) expects to send to this port
    uint16_t  UDP_REMOTEPORT = 14550;   // sendPort - (default 14550) remote host reads on this port
    uint16_t  TCP_LOCALPORT = 5760;
    uint16_t  TCP_REMOTEPORT = 5760;

    uint16_t  udp_read_port = 0;
    uint16_t  udp_send_port = 0;

    bool wifiSuGood = false;
    bool wifiApConnected = false;
    bool wifiStaConnected = false;
    bool inbound_clientGood = false;
    bool wifiDisconnected = false;
    bool outbound_clientGood = false;
    bool showRemoteIP = true;
    bool wifi_recv_good = false;
    uint32_t wifi_retry_millis = 0;
    uint32_t wifi_recv_millis = 0;

    //====================       I n s t a n t i a t e   W i F i   O b j e c t s
    
    #define max_clients    6
    uint8_t active_udp_obj_idx = 0;         // for UDP
    uint8_t active_udpremoteip_idx = 0;     // active remote ip 
    uint8_t active_client_obj_idx = 0;

    WiFiClient *tcp_client[max_clients] = {NULL}; // pointers to TCP client objects
    WiFiServer TCPserver(TCP_LOCALPORT);          // dummy TCP local port(changes on TCPserver.begin() ).
    IPAddress UDP_remoteIP(192, 168, 1, 255);     // default to broadcast unless (not defined UDP_Broadcast)
    IPAddress udpremoteip[max_clients];           // table of remote UDP client IPs
    WiFiUDP *udp_object[2] = {NULL};              // pointers to UDP objects for STA and AP modes
                           
  //=================================================================================================
  void printRemoteIP()
  {
    if (showRemoteIP)
    {
      showRemoteIP = false;
      log.print("UDP client identified, remote IP: ");
      log.print(UDP_remoteIP);
      log.print(", remote port: ");
      log.println(UDP_REMOTEPORT);
    }
  }
  //=================================================================================================
  bool newOutboundTCPClient()
  {
    static uint8_t retry = 3;
    WiFiClient newClient;
    while (!newClient.connect(TCP_REMOTEIP, TCP_REMOTEPORT))
    {
      log.printf("Local outbound tcp client connect failed, retrying %d\n", retry);
      retry--;
      if (retry == 0)
      {
        log.println("Tcp client connect aborted!");
        return false;
      }
      delay(4000);
    }
    active_client_obj_idx = 0; // reserve the first tcp client object for our single outbound session
    tcp_client[0] = new WiFiClient(newClient);
    log.print("Local tcp client connected to remote server IP:");
    log.print(TCP_REMOTEIP);
    log.print(" remote Port:");
    log.println(TCP_REMOTEPORT);

    return true;
  }
  //==================================================
  void startWiFiTimer()
  {
    wifi_retry_millis = millis();
  }
  //===============================     H a n d l e   I n B o u n d   T C P   C l i e n t s ( G C S   s i d e )
  void serviceInboundTCPClients()
  {
    #if (WIFI_PROTOCOL == 1)  // TCP
      if (wifiSuGood)
      {
        WiFiClient newClient = TCPserver.available(); // check if a new client wants to connect
        if (newClient)
        {
          for (int i = 0; i < max_clients; ++i)
          { // if connected add it into our connected tcp_client table
            if (NULL == tcp_client[i])
            {                                            // find first empty slot in table
              tcp_client[i] = new WiFiClient(newClient); // create new client object to use
              inbound_clientGood = true;
              active_client_obj_idx = i;
              log.printf("Remote tcp client %d connected\n", i + 1);
              break;
            }
          }
        }
        for (int i = 0; i < max_clients; ++i)
        { // check to see if any clients have disconnected
          if (NULL != tcp_client[i])
          {
            if (!tcp_client[i])
            {
              delay(100);
              tcp_client[i]->stop();
              tcp_client[i] = NULL;
              delay(100);
              inbound_clientGood = false;
              log.println("TCP client disconnected");
            }
            break;
          }
        }
      }
    #endif  
  }
  //===================================================================
  String wifiStatusText(uint16_t wifi_status)
  {
    switch (wifi_status)
    {
    case 0:
      return "WL_IDLE_STATUS";
    case 1:
      return "WL_NO_SSID_AVAIL";
    case 2:
      return "WL_SCAN_COMPLETED";
    case 3:
      return "WL_CONNECTED";
    case 4:
      return "WL_CONNECT_FAILED";
    case 5:
      return "WL_CONNECTION_LOST";
    case 6:
      return "WL_DISCONNECTED";
    case 255:
      return "WL_NO_SHIELD";
    default:
      return "UNKNOWN";
    }
  }
  //===================     H a n d l e   W i F i   D i s c o n n e c t  /  R e c o n n e c t
  void checkWiFiConnected()
  {
    static uint16_t prev_wifi_status;
    uint16_t wifi_status = WiFi.status();
    if (wifi_status != prev_wifi_status)
    {
      prev_wifi_status = wifi_status;
      String wifi_text = wifiStatusText(wifi_status);
      log.printf("Wifi status:%u %s\n", wifi_status, wifi_text.c_str());
    }
    if (!WiFi.isConnected())
    { // only applies to STA
      if (!wifiDisconnected)
        startWiFiTimer(); // start wifi retry interrupt timer
      wifiDisconnected = true;
      #if (WIFI_MODE == 2)
        wifiStaConnected = false;
      #endif
      #if (WIFI_MODE == 1)
        wifiApConnected = false;
      #endif
      // if ( (wifi_retry_millis > 0) && ( (millis() - wifi_retry_millis) > 10000) ) {
      if ((millis() - wifi_retry_millis) > 10000)
      {
        log.println("WiFi link lost - retrying");
        startStation();
        // restartWiFi();
        wifi_retry_millis = millis(); // restart timer
      }
    }
    else
    {
      if (wifiDisconnected)
      {
        log.println("Wifi link restored");
        wifiDisconnected = false;
        wifi_retry_millis = 0; // stop timer
      }
    }
  }
  //===============================       H a n d l e   W i F i   E v e n t s
  void serviceWiFiRoutines()
  {
    #if (WIFI_MODE == 2)  // STA                  // in ap_sta or sta modes only, check for disconnect from AP
    //  checkWiFiConnected(); // Handle disconnect/reconnect
    #endif
    serviceInboundTCPClients(); // from GCS side
    // Report stations connected to/from our AP
    uint8_t AP_sta_count = WiFi.softAPgetStationNum();
    static uint8_t AP_prev_sta_count = 0;
    wifiApConnected = (AP_sta_count > 0);
    if (AP_sta_count > AP_prev_sta_count)
    {
      AP_prev_sta_count = AP_sta_count;
      log.printf("Remote STA %d connected to our AP\n", AP_sta_count);
      #if (WIFI_PROTOCOL == 1)  // TCP
        if (!outbound_clientGood) // and we don't have an active tcp session, start a new session
          outbound_clientGood = newOutboundTCPClient();
      #endif
    }
    else if (AP_sta_count < AP_prev_sta_count)
    { // a device has disconnected from the AP
      AP_prev_sta_count = AP_sta_count;
      log.println("A STA disconnected from our AP"); // back in listening mode
    }
  }
  //==================================================
  void restartWiFi()
  {
    WiFi.disconnect(false);
    #if (WIFI_MODE == 2)
      WiFi.mode(WIFI_STA);
      WiFi.begin(STAssid, STApw);
    #endif
    delay(250);
  }
  //=================================================================================================
  void startAccessPoint()
  {
    log.printf("WiFi mode set to WIFI_AP %s\n", WiFi.mode(WIFI_AP) ? "" : "Failed!");
    // WiFi.softAP(const char* ssid, const char* password, int channel, int ssid_hidden, int max_connection)
    WiFi.softAP(APssid, APpw);
    delay(100);
    log.print("AP_default_IP:");
    log.print(AP_default_IP); // these print statement give the module time to complete the above setting
    log.print("  AP_gateway:");
    log.print(AP_gateway);
    log.print("  AP_mask:");
    log.println(AP_mask);

    WiFi.softAPConfig(AP_default_IP, AP_gateway, AP_mask);

    localIP = WiFi.softAPIP(); // tcp and udp

    log.print("AP IP address: ");
    log.print(localIP);
    log.printf(" SSID:%s", APssid);

    /*use mdns for host name resolution*/
    if (!MDNS.begin(HostName))
    { // http://<HostName>.local
      log.println("Error setting up MDNS responder!");
      while (1)
      {
        delay(1000);
      }
    }
    log.println("mDNS responder started");

    #if (WIFI_PROTOCOL == 1)  // TCP
      TCPserver.begin(TCP_LOCALPORT); //  Server for TCP/IP traffic
      log.printf("TCP/IP started, local IP:port %s:%d\n", localIP.toString().c_str(), TCP_LOCALPORT);
    #endif

    #if (WIFI_PROTOCOL == 2)  // UDP
        // regular AP
      udp_read_port = UDP_LOCALPORT;
      udp_send_port = UDP_REMOTEPORT;
    #endif

    // Start UDP Object
    WiFiUDP UDP_Object;
    udp_object[1] = new WiFiUDP(UDP_Object);
    log.printf("Begin UDP using AP UDP object  read port:%d  send port:%d\n", udp_read_port, udp_send_port);

    udp_object[1]->begin(udp_read_port); // there are 2 possible udp objects, STA [0]    and    AP [1]

    UDP_remoteIP = WiFi.softAPIP();
    UDP_remoteIP[3] = 255; // broadcast until we know which ip to target

    // Now initialise the first entry of the udp targeted ip table

    udpremoteip[0] = UDP_remoteIP;
    udpremoteip[1] = UDP_remoteIP;

    log.printf("UDP for AP started, local %s   remote %s\n", WiFi.softAPIP().toString().c_str(),
                UDP_remoteIP.toString().c_str());

    wifiSuGood = true;
  }
  //=================================================================================================
  bool startStation()
  {
    uint8_t retry = 0;
    WiFi.disconnect(true); // To circumvent "wifi: Set status to INIT" error bug
    delay(500);
    if (WiFi.mode(WIFI_STA))
    {
      log.println("WiFi mode set to STA sucessfully");
    } else
    {
      log.println("WiFi mode set to STA failed!");
    }
    log.print("Trying to connect to ");
    log.print(STAssid);
    delay(500);

    WiFi.begin(STAssid, STApw);
    while (WiFi.status() != WL_CONNECTED)
    {
      retry++;
      static uint32_t max_retry = 30;
      if (retry >= max_retry)
      {
        log.println();
        log.println("Failed to connect in STA mode");
        return false;
      }
      delay(1000);
      log.print(".");
    }   

    if (WiFi.status() == WL_CONNECTED)
    {
      log.println();
      log.println("WiFi connected!");
      wifiStaConnected = true;
      /*use mdns for host name resolution*/
      if (!MDNS.begin(HostName))
      { // http://<HostName>.local
        log.println("Error setting up MDNS responder!");
        while (1)
        {
          delay(1000);
        }
      }
      log.println("mDNS responder started");

      localIP = WiFi.localIP(); // TCP and UDP

      UDP_remoteIP = localIP; // Initially broadcast on the subnet we are attached to. patch by Stefan Arbes.
      UDP_remoteIP[3] = 255;  // patch by Stefan Arbes

      log.print("Local IP address: ");
      log.print(localIP);
      #if (WIFI_PROTOCOL == 1)      // TCP 
        log.print("  port: ");
        log.println(TCP_LOCALPORT); 
      #else
        log.println();
      #endif

      int16_t wifi_rssi = WiFi.RSSI();
      log.print("WiFi RSSI:");
      log.print(wifi_rssi);
      log.println(" dBm");

      #if (WIFI_PROTOCOL == 1)  // TCP
        TCPserver.begin(TCP_LOCALPORT); //  tcp server socket started
        log.println("TCP server started");
      #endif

      #if (WIFI_PROTOCOL == 2)  // UDP
        udp_read_port = UDP_REMOTEPORT;
        udp_send_port = UDP_LOCALPORT; // so we swap read and send ports, local (read) becomes 14550
        WiFiUDP UDP_STA_Object;
        udp_object[0] = new WiFiUDP(UDP_STA_Object);
        log.printf("Begin UDP using STA UDP object  read port:%d  send port:%d\n", udp_read_port, udp_send_port);
        udp_object[0]->begin(udp_read_port); // there are 2 possible udp objects, STA [0]    and    AP [1]
        UDP_remoteIP = localIP;
        UDP_remoteIP[3] = 255; // broadcast until we know which ip to target
        udpremoteip[1] = UDP_remoteIP; // [1] IPs reserved for GCS side
        log.printf("UDP for STA started, local %s   remote %s\n", localIP.toString().c_str(),
                  UDP_remoteIP.toString().c_str());
      #endif
      wifiSuGood = true;
    }  
    return true;
  }
  //=================================================================================================
  bool sendUDP(uint8_t *buf, uint8_t lth)
  {
    if (!wifiSuGood)
      return false;

   // 2 possible udp objects, STA [0]    and    AP [1]
    #if (WIFI_MODE == 2)  // STA
      active_udp_obj_idx = 0;             // Use STA UDP object for FC read
      udp_read_port = UDP_REMOTEPORT; // used by printRemoteIP() only. read port set by ->begin(udp_read_port)).
      udp_send_port = UDP_LOCALPORT;
      // log.printf("readFC() read port:%u    send port:%u\n", udp_read_port, udp_send_port);

    #endif
    #if (WIFI_MODE == 1)  // AP
        active_udp_obj_idx = 1; // Use AP UDP object for FC read
        udp_read_port = UDP_LOCALPORT;
        udp_send_port = UDP_REMOTEPORT;
    #endif

    if ((active_udp_obj_idx == 0) && (!(wifiStaConnected)))
      return false;
    if ((active_udp_obj_idx == 1) && (!(wifiApConnected)))
      return false;

    bool msgSent = false;

    udp_object[active_udp_obj_idx]->beginPacket(UDP_remoteIP, udp_send_port); //  udp ports gets flipped for sta_ap mode

    size_t sent = udp_object[active_udp_obj_idx]->write(buf, lth);

    if (sent == lth)
    {
      msgSent = true;
    }
    udp_object[active_udp_obj_idx]->flush();
    bool endOK = udp_object[active_udp_obj_idx]->endPacket();

    //   if (!endOK) log.printf("msgSent=%d   endOK=%d\n", msgSent, endOK);
    return (msgSent & endOK); // send good if msgSent and endOK both true
  }
  //=================================================================================================
  bool readUDP(uint8_t *buf)
  {
    if (!wifiSuGood)
      return false;
    bool msgRcvd = false;

    if ((active_udp_obj_idx == 0) && (!(wifiStaConnected)))
      return 0;
    if ((active_udp_obj_idx == 1) && (!(wifiApConnected)))
      return 0; 
      
   // 2 possible udp objects, STA [0]    and    AP [1]
     #if (WIFI_MODE == 2)  // STA
        active_udp_obj_idx = 0;             // Use STA UDP object for FC read
        udp_read_port = UDP_REMOTEPORT; // used by printRemoteIP() only. read port set by ->begin(udp_read_port)).
        udp_send_port = UDP_LOCALPORT;
        // log.printf("readFC() read port:%u    send port:%u\n", udp_read_port, udp_send_port);

      #endif
      #if (WIFI_MODE == 1)  // AP
          active_udp_obj_idx = 1; // Use AP UDP object for FC read
          udp_read_port = UDP_LOCALPORT;
          udp_send_port = UDP_REMOTEPORT;
      #endif

    int16_t packetSize = udp_object[active_udp_obj_idx]->parsePacket();
    // esp sometimes reboots here: WiFiUDP.cpp line 213 char * buf = new char[1460];

    // log.printf("Read UDP object=%d port:%d  len:%d\n", active_udp_obj_idx, udp_read_port, len);
    
    if (packetSize)
      {
        int16_t len = udp_object[active_udp_obj_idx]->read(buf, 64);
        if (len >= 0)
        {
          msgRcvd = true;
          wifi_recv_millis = millis();
          wifi_recv_good = true;
          //printBuffer(buf);
          #if (not defined UDP_Broadcast)
            UDP_remoteIP = udp_object[active_udp_obj_idx]->remoteIP();
            bool in_table = false;
              for (int i = 1; i < max_clients; i++)
              {
                if (udpremoteip[i] == UDP_remoteIP)
                { // IP already in the table
                  //    log.printf("%s already in table\n", UDP_remoteIP.toString().c_str() );
                  in_table = true;
                  break;
                }
              }
              if (!in_table)
              { // if not in table, add it into empty slot, but not [0] reserved for otgoing (FC side)
                for (int i = 1; i < max_clients; i++)
                {
                  if ((udpremoteip[i][0] == 0) || (udpremoteip[i][3] == 255))
                  {                                // overwrite empty or broadcast ips
                    udpremoteip[i] = UDP_remoteIP; // remember unique IP of remote udp client so we can target it
                    log.printf("%s client inserted in UDP client table\n", UDP_remoteIP.toString().c_str());
                    showRemoteIP = true;
                    break;
                  }
                }
              }
          #endif
          printRemoteIP();
        }
      }   
    return msgRcvd;
  }
