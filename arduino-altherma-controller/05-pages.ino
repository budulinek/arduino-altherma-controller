const byte WEB_OUT_BUFFER_SIZE = 64;  // size of web server write buffer (used by StreamLib)

/**************************************************************************/
/*!
  @brief Sends the requested page (incl. 404 error and JSON document),
  displays main page, renders title and left menu using, calls content functions
  depending on the number (i.e. URL) of the requested web page.
  In order to save flash memory, some HTML closing tags are omitted,
  new lines in HTML code are also omitted.
  @param client Ethernet TCP client
  @param reqPage Requested page number
*/
/**************************************************************************/
void sendPage(EthernetClient &client, byte reqPage) {
  char webOutBuffer[WEB_OUT_BUFFER_SIZE];
  ChunkedPrint chunked(client, webOutBuffer, sizeof(webOutBuffer));  // the StreamLib object to replace client print
  if (reqPage == PAGE_ERROR) {
    chunked.print(F("HTTP/1.1 404 Not Found\r\n"
                    "\r\n"
                    "404 Not found"));
    chunked.end();
    return;
  } else if (reqPage == PAGE_DATA) {
    chunked.print(F("HTTP/1.1 200\r\n"  // An advantage of HTTP 1.1 is that you can keep the connection alive
                    "Content-Type: application/json\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"));
    chunked.begin();
    chunked.print(F("{"));
    for (byte i = 0; i < JSON_LAST; i++) {
      if (i) chunked.print(F(","));
      chunked.print(F("\""));
      chunked.print(i);
      chunked.print(F("\":\""));
      jsonVal(chunked, i);
      chunked.print(F("\""));
    }
    chunked.print(F("}"));
    chunked.end();
    return;
  }
  chunked.print(F("HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html\r\n"
                  "Transfer-Encoding: chunked\r\n"
                  "\r\n"));
  chunked.begin();
  chunked.print(F("<!DOCTYPE html>"
                  "<html>"
                  "<head>"
                  "<meta charset=utf-8>"
                  "<meta name=viewport content='width=device-width"));
  if (reqPage == PAGE_WAIT) {  // redirect to new IP and web port
    chunked.print(F("'>"
                    "<meta http-equiv=refresh content='5; url=http://"));
    chunked.print(IPAddress(data.config.ip));
    chunked.print(F(":"));
    chunked.print(data.config.webPort);
  }
  chunked.print(F("'>"
                  "<title>Altherma UDP Controller</title>"
                  "<style>"
                  /*
                  HTML Tags
                    h1 - main title of the page
                    h4 - text in navigation menu and header of page content
                    a - items in left navigation menu
                    label - first cell of a row in content
                  CSS Classes
                    w - wrapper (includes m + c)
                    m  - navigation menu (left)
                    c - wrapper for the content of a page (incl. smaller header and main)
                    q - row inside a content (default: top-aligned)
                    r - row inside a content (adds: center-aligned)
                    i - short input (byte or IP address octet)
                    n - input type=number
                    s - select input with numbers
                    p - inputs disabled by id=o checkbox
                  CSS Ids
                    o - checkbox which disables other checkboxes and inputs
                  */
                  "*{box-sizing:border-box}"
                  "body{padding:1px;margin:0;font-family:sans-serif;height:100vh}"
                  "body,.w,.c,.q{display:flex}"
                  "body,.c{flex-flow:column}"
                  ".w{flex-grow:1;min-height:0}"
                  ".m{flex:0 0 20vw}"
                  ".c{flex:1}"
                  ".m,main{overflow:auto;padding:15px}"
                  ".m,.q{padding:1px}"
                  ".r{align-items:center}"
                  "h1,h4{padding:10px}"
                  "h1,.m,h4{background:#0067AC;margin:1px}"
                  "a,h1,h4{color:white;text-decoration:none}"
                  ".c h4{padding-left:30%}"
                  "label{width:30%;text-align:right;margin-right:2px}"
                  ".s{text-align:right}"
                  ".s>option{direction:rtl}"
                  ".i{text-align:center;width:4ch}"
                  ".n{width:10ch}"
                  "</style>"
                  "</head>"
                  "<body onload=g(document.getElementById('o').checked)>"
                  "<script>function g(h) {var x = document.getElementsByClassName('p');for (var i = 0; i < x.length; i++) {x[i].disabled = h}}</script"));
  if (reqPage == PAGE_STATUS) {
    chunked.print(F("><script>"
                    "var a;"
                    "const b=()=>{"
                    "fetch('d.json')"  // Call the fetch function passing the url of the API as a parameter
                    ".then(e=>{return e.json();a=0})"
                    ".then(f=>{for(var i in f){if(document.getElementById(i))document.getElementById(i).innerHTML=f[i];}})"
                    ".catch(()=>{if(!a){alert('Connnection lost');a=1}})"
                    "};"
                    "setInterval(()=>b(),"));
    chunked.print(FETCH_INTERVAL);
    chunked.print(F(");"
                    "</script"));
  }
  chunked.print(F(">"
                  "<h1>Altherma UDP Controller</h1>"
                  "<div class=w>"
                  "<div class=m>"));

  // Left Menu
  for (byte i = 1; i < PAGE_WAIT; i++) {  // PAGE_WAIT is the last item in enum
    chunked.print(F("<h4"));
    if ((i) == reqPage) {
      chunked.print(F(" style=background-color:#FF6600"));
    }
    chunked.print(F("><a href="));
    chunked.print(i);
    chunked.print(F(".htm>"));
    stringPageName(chunked, i);
    chunked.print(F("</a></h4>"));
  }
  chunked.print(F("</div>"  // <div class=w>
                  "<div class=c>"
                  "<h4>"));
  stringPageName(chunked, reqPage);
  chunked.print(F("</h4>"
                  "<main>"
                  "<form method=post>"));

  //   PLACE FUNCTIONS PROVIDING CONTENT HERE
  switch (reqPage) {
    case PAGE_INFO:
      contentInfo(chunked);
      break;
    case PAGE_STATUS:
      contentStatus(chunked);
      break;
    case PAGE_IP:
      contentIp(chunked);
      break;
    case PAGE_TCP:
      contentTcp(chunked);
      break;
    case PAGE_P1P2:
      contentP1P2(chunked);
      break;
    case PAGE_FILTER:
      contentFilter(chunked);
      break;
    case PAGE_TOOLS:
      contentTools(chunked);
      break;
    case PAGE_WAIT:
      contentWait(chunked);
      break;
    default:
      break;
  }
  if (reqPage == PAGE_IP || reqPage == PAGE_TCP || reqPage == PAGE_P1P2 || reqPage == PAGE_FILTER) {
    chunked.print(F("<p><div class=r><label><input type=submit value='Save & Apply'></label><input type=reset value=Cancel></div>"));
  }
  chunked.print(F("</form>"
                  "</main>"));
  tagDivClose(chunked);  // close tags <div class=c> <div class=w>
  chunked.end();         // closing tags not required </body></html>
}


/**************************************************************************/
/*!
  @brief System Info

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentInfo(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("SW Version"));
  chunked.print(VERSION[0]);
  chunked.print(F("."));
  chunked.print(VERSION[1]);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Microcontroller"));
  chunked.print(BOARD);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("EEPROM Health"));
  chunked.print(data.eepromWrites);
  chunked.print(F(" Write Cycles"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Ethernet Chip"));
  switch (W5100.getChip()) {
    case 51:
      chunked.print(F("W5100"));
      break;
    case 52:
      chunked.print(F("W5200"));
      break;
    case 55:
      chunked.print(F("W5500"));
      break;
    default:  // TODO: add W6100 once it is included in Ethernet library
      chunked.print(F("Unknown"));
      break;
  }
  tagDivClose(chunked);

#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("Ethernet Sockets"));
  chunked.print(maxSockNum);
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */

  tagLabelDiv(chunked, F("MAC Address"));
  for (byte i = 0; i < 6; i++) {
    chunked.print(hex(data.mac[i]));
    if (i < 5) chunked.print(F(":"));
  }
  tagDivClose(chunked);

#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("DHCP Status"));
  if (!data.config.enableDhcp) {
    chunked.print(F("Disabled"));
  } else if (dhcpSuccess == true) {
    chunked.print(F("Success"));
  } else {
    chunked.print(F("Failed, using fallback static IP"));
  }
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */

  tagLabelDiv(chunked, F("IP Address"));
  chunked.print(IPAddress(Ethernet.localIP()));
  tagDivClose(chunked);
}

/**************************************************************************/
/*!
  @brief P1P2 Status

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentStatus(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Daikin Indoor Unit"));
  tagSpan(chunked, JSON_DAIKIN_INDOOR);
  tagDivClose(chunked);
#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("Daikin Outdoor Unit"));
  tagSpan(chunked, JSON_DAIKIN_OUTDOOR);
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */
  tagLabelDiv(chunked, F("External Controllers"), true);
  tagSpan(chunked, JSON_CONTROLLER);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Date"));
  tagSpan(chunked, JSON_DATE);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Daikin EEPROM Writes"));
  tagButton(chunked, F("Reset"), ACT_RESET_EEPROM, true);
  chunked.print(F(" Stats since "));
  tagSpan(chunked, JSON_DAIKIN_EEPROM_DATE);
  tagDivClose(chunked);
  tagLabelDiv(chunked, 0);
  tagSpan(chunked, JSON_DAIKIN_EEPROM);
  tagDivClose(chunked);
  chunked.print(F("</form><form method=post>"));
  tagLabelDiv(chunked, F("Write Command"));
  chunked.print(F("Packet Type "
                  "<select name="));
  chunked.print(POST_CMD_TYPE, HEX);
  chunked.print(F(">"));
  for (byte i = PACKET_TYPE_CONTROL[FIRST]; i <= PACKET_TYPE_CONTROL[LAST]; i++) {
    if (PACKET_PARAM_VAL_SIZE[i - PACKET_TYPE_CONTROL[FIRST]] == 0) continue;
    chunked.print(F("<option value="));
    chunked.print(hex(i));
    chunked.print(F(">"));
    chunked.print(hex(i));
    chunked.print(F("</option>"));
  }
  chunked.print(F("</select>"
                  " Param "));
  for (byte i = 0; i < 2; i++) {
    tagInputHex(chunked, POST_CMD_PARAM_1 + i, true, false, 0x00);
  }
  chunked.print(F(" Value "));
  for (byte i = 0; i <= POST_CMD_VAL_4 - POST_CMD_VAL_1; i++) {
    tagInputHex(chunked, POST_CMD_VAL_1 + i, false, false, 0x00);
  }
  tagSpan(chunked, JSON_WRITE_P1P2);
  tagDivClose(chunked);
  chunked.print(F("</form><form method=post>"));
#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("Run Time"));
  tagSpan(chunked, JSON_RUNTIME);
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */
  tagLabelDiv(chunked, F("P1P2 Packets"));
  tagButton(chunked, F("Reset"), ACT_RESET_STATS, true);
  chunked.print(F(" Stats since "));
  tagSpan(chunked, JSON_P1P2_STATS_DATE);
  tagDivClose(chunked);
  tagLabelDiv(chunked, 0);
  tagSpan(chunked, JSON_P1P2_STATS);
  tagDivClose(chunked);
#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("UDP Messages"));
  tagSpan(chunked, JSON_UDP_STATS);
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */
}

/**************************************************************************/
/*!
  @brief IP Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentIp(ChunkedPrint &chunked) {

  tagLabelDiv(chunked, F("MAC Address"));
  for (byte i = 0; i < 6; i++) {
    tagInputHex(chunked, POST_MAC + i, true, true, data.mac[i]);
    if (i < 5) chunked.print(F(":"));
  }
  tagButton(chunked, F("Randomize"), ACT_MAC, true);
  tagDivClose(chunked);

#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("Auto IP"));
  tagCheckbox(chunked, POST_SEND_ALL, data.config.enableDhcp, true, false);
  chunked.print(F(" DHCP"));
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */

  byte *tempIp;
  for (byte j = 0; j < 3; j++) {
    switch (j) {
      case 0:
        tagLabelDiv(chunked, F("Static IP"));
        tempIp = data.config.ip;
        break;
      case 1:
        tagLabelDiv(chunked, F("Submask"));
        tempIp = data.config.subnet;
        break;
      case 2:
        tagLabelDiv(chunked, F("Gateway"));
        tempIp = data.config.gateway;
        break;
      default:
        break;
    }
    tagInputIp(chunked, POST_IP + (j * 4), tempIp);
    tagDivClose(chunked);
  }
#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("DNS Server"));
  tagInputIp(chunked, POST_DNS, data.config.dns);
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */
}

/**************************************************************************/
/*!
  @brief TCP/UDP Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentTcp(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Remote IP"));
  tagInputIp(chunked, POST_REM_IP, data.config.remoteIp);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Send and Receive UDP"));
  static const __FlashStringHelper *optionsList[] = {
    F("Only to/from Remote IP"),
    F("To/From Any IP (Broadcast)")
  };
  tagSelect(chunked, POST_UDP_BROADCAST, optionsList, 2, data.config.udpBroadcast);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("UDP Port"));
  tagInputNumber(chunked, POST_UDP, 1, 65535, data.config.udpPort, F(""));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("WebUI Port"));
  tagInputNumber(chunked, POST_WEB, 1, 65535, data.config.webPort, F(""));
  tagDivClose(chunked);
}

/**************************************************************************/
/*!
  @brief P1P2 Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentP1P2(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Enable Write to P1P2"));
  static const __FlashStringHelper *optionsList[] = {
    F("Manually"),
    F("Automatically")
  };
  tagSelect(chunked, POST_CONTROL_MODE, optionsList, 2, data.config.controllerMode);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Connection Timeout"));
  tagInputNumber(chunked, POST_TIMEOUT, F0THRESHOLD, 60, data.config.connectTimeout, F("s"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Daikin EEPROM Write Quota"));
  tagInputNumber(chunked, POST_QUOTA, 0, 100, data.config.writeQuota, F("writes per day"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Target Temperature Hysteresis"));
  tagInputNumber(chunked, POST_HYSTERESIS, 0, 100, data.config.hysteresis, F("&#8530;\xB0"
                                                                             "C"));
  tagDivClose(chunked);
}

/**************************************************************************/
/*!
  @brief Packet Filter

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentFilter(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Send All Packet Types"));
  tagCheckbox(chunked, POST_SEND_ALL, data.config.sendAllPackets, true, false);
  chunked.print(F("Enable (use with caution!)"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Counters Packet"));
  chunked.print(F("Request Every "));
  tagInputNumber(chunked, POST_COUNTER_PERIOD, 1, 60, data.config.counterPeriod, F("mins"));
  tagDivClose(chunked);
  tagRowPacket(chunked, PACKET_TYPE_COUNTER);
  tagLabelDiv(chunked, F("Data Packets"));
  static const __FlashStringHelper *optionsList[] = {
    F("Always Send (~770ms cycle)"),
    F("If Payload Changed or When Counters Requested"),
    F("Only If Payload Changed")
  };
  tagSelect(chunked, POST_DATA_PACKETS, optionsList, 3, data.config.sendDataPackets);
  tagDivClose(chunked);
  for (byte i = PACKET_TYPE_DATA[FIRST]; i <= PACKET_TYPE_DATA[LAST]; i++) {
    tagRowPacket(chunked, i);
  }
  tagLabelDiv(chunked, F("Other Packets"));
  tagDivClose(chunked);
  for (byte i = 0;; i++) {
    if ((i < PACKET_TYPE_DATA[FIRST] || i > PACKET_TYPE_DATA[LAST]) && i != PACKET_TYPE_COUNTER) {
      tagRowPacket(chunked, i);
    }
    if (i == 0xFF) break;
  }
}

/**************************************************************************/
/*!
  @brief Tools

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentTools(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, 0);
  tagButton(chunked, F("Load Default Settings"), ACT_DEFAULT, true);
  chunked.print(F(" (static IP: "));
  chunked.print(IPAddress(DEFAULT_CONFIG.ip));
  chunked.print(F(")"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, 0);
  tagButton(chunked, F("Reboot"), ACT_REBOOT, true);
  tagDivClose(chunked);
}


/**************************************************************************/
/*!
  @brief Wait

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentWait(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, 0);
  chunked.print(F("Reloading. Please wait..."));
  tagDivClose(chunked);
}

/**************************************************************************/
/*!
  @brief Row for Packet Type setting

  @param chunked Chunked buffer
  @param packetType Packet type
*/
/**************************************************************************/
void tagRowPacket(ChunkedPrint &chunked, const byte packetType) {
  if (getPacketStatus(packetType, PACKET_SEEN) == true) {
    chunked.print(F("<div class='r q'>"
                    "<label>Type 0x"));
    chunked.print(hex(packetType));
    chunked.print(F(":</label>"
                    "<div>"));
    tagCheckbox(chunked, packetType, getPacketStatus(packetType, PACKET_SENT), false, true);
    tagDivClose(chunked);
  }
}

/**************************************************************************/
/*!
  @brief <select><option>

  @param chunked Chunked buffer
  @param name Name POST_
  @param options Strings for <option>
  @param numOptions Number of options
  @param value Value from data.config
*/
/**************************************************************************/
void tagSelect(ChunkedPrint &chunked, const byte name, const __FlashStringHelper *options[], const byte numOptions, byte value) {
  chunked.print(F("<select name="));
  chunked.print(name, HEX);
  chunked.print(F(">"));
  for (byte i = 0; i < numOptions; i++) {
    chunked.print(F("<option value="));
    chunked.print(i);
    if (value == i) chunked.print(F(" selected"));
    chunked.print(F(">"));
    chunked.print(options[i]);
    chunked.print(F("</option>"));
  }
  chunked.print(F("</select>"));
}

/**************************************************************************/
/*!
  @brief <input type=checkbox>
  Can be hidden by JS

  @param chunked Chunked buffer
  @param name Name
  @param setting Setting to be toggled
  @param parent Input is parent (setting visibility for other inputs)
  @param isPacket True if input is for Packet Type
*/
/**************************************************************************/
void tagCheckbox(ChunkedPrint &chunked, const byte name, const bool setting, const bool parent, const bool isPacket) {
  for (byte i = 0; i < 2; i++) {
    chunked.print(F("<input value="));
    if (i == 0) {
      chunked.print(F("0 type=hidden"));
    } else {
      chunked.print(F("1 type=checkbox"));
      if (setting) chunked.print(F(" checked"));
    }
    chunked.print(F(" name="));
    if (isPacket) chunked.print(F("p"));
    chunked.print(name, HEX);
    if (parent) {
      chunked.print(F(" id=o onclick=g(this.checked)"));
    } else {
      chunked.print(F(" class=p"));
    }
    chunked.print(F(">"));
  }
}

/**************************************************************************/
/*!
  @brief <input type=number>

  @param chunked Chunked buffer
  @param name Name POST_
  @param min Minimum value
  @param max Maximum value
  @param value Current value
  @param units Units (string)
*/
/**************************************************************************/
void tagInputNumber(ChunkedPrint &chunked, const byte name, uint16_t min, uint16_t max, uint16_t value, const __FlashStringHelper *units) {
  chunked.print(F("<input class='s n' required type=number name="));
  chunked.print(name, HEX);
  chunked.print(F(" min="));
  chunked.print(min);
  chunked.print(F(" max="));
  chunked.print(max);
  chunked.print(F(" value="));
  chunked.print(value);
  chunked.print(F("> ("));
  chunked.print(min);
  chunked.print(F("~"));
  chunked.print(max);
  chunked.print(F(") "));
  chunked.print(units);
}

/**************************************************************************/
/*!
  @brief <input>
  IP address (4 elements)

  @param chunked Chunked buffer
  @param name Name POST_
  @param ip IP address from data.config
*/
/**************************************************************************/
void tagInputIp(ChunkedPrint &chunked, const byte name, byte ip[]) {
  for (byte i = 0; i < 4; i++) {
    chunked.print(F("<input name="));
    chunked.print(name + i, HEX);
    chunked.print(F(" class='p i' required maxlength=3 pattern='^(&bsol;d{1,2}|1&bsol;d&bsol;d|2[0-4]&bsol;d|25[0-5])$' value="));
    chunked.print(ip[i]);
    chunked.print(F(">"));
    if (i < 3) chunked.print(F("."));
  }
}

/**************************************************************************/
/*!
  @brief <input>
  HEX string (2 chars)

  @param chunked Chunked buffer
  @param name Name POST_
  @param required True if input is required
  @param printVal True if value is shown
  @param value Value
*/
/**************************************************************************/
void tagInputHex(ChunkedPrint &chunked, const byte name, const bool required, const bool printVal, const byte value) {
  chunked.print(F("<input name="));
  chunked.print(name, HEX);
  if (required) {
    chunked.print(F(" required"));
  }
  chunked.print(F(" minlength=2 maxlength=2 class=i pattern='[a-fA-F&bsol;d]+' value='"));
  if (printVal) {
    chunked.print(hex(value));
  }
  chunked.print(F("'>"));
}

/**************************************************************************/
/*!
  @brief <label><div>

  @param chunked Chunked buffer
  @param label Label string
  @param top Align to top
*/
/**************************************************************************/
void tagLabelDiv(ChunkedPrint &chunked, const __FlashStringHelper *label) {
  tagLabelDiv(chunked, label, false);
}
void tagLabelDiv(ChunkedPrint &chunked, const __FlashStringHelper *label, bool top) {
  chunked.print(F("<div class='q"));
  if (!top) chunked.print(F(" r"));
  chunked.print(F("'><label> "));
  if (label) {
    chunked.print(label);
    chunked.print(F(":"));
  }
  chunked.print(F("</label><div>"));
}

/**************************************************************************/
/*!
  @brief <button>

  @param chunked Chunked buffer
  @param flashString Button string
  @param value Value to be sent via POST
  @param enabled Active if true, disabled if false
*/
/**************************************************************************/
void tagButton(ChunkedPrint &chunked, const __FlashStringHelper *flashString, byte value, bool enabled) {
  chunked.print(F(" <button name="));
  chunked.print(POST_ACTION, HEX);
  chunked.print(F(" value="));
  chunked.print(value);
  if (!enabled) chunked.print(F(" disabled"));
  chunked.print(F(">"));
  chunked.print(flashString);
  chunked.print(F("</button>"));
}

/**************************************************************************/
/*!
  @brief </div>

  @param chunked Chunked buffer
*/
/**************************************************************************/
void tagDivClose(ChunkedPrint &chunked) {
  chunked.print(F("</div>"
                  "</div>"));  // <div class=q>
}

/**************************************************************************/
/*!
  @brief <span>

  @param chunked Chunked buffer
  @param JSONKEY JSON_ id
*/
/**************************************************************************/
void tagSpan(ChunkedPrint &chunked, const byte JSONKEY) {
  chunked.print(F("<span id="));
  chunked.print(JSONKEY);
  chunked.print(F(">"));
  jsonVal(chunked, JSONKEY);
  chunked.print(F("</span>"));
}

/**************************************************************************/
/*!
  @brief Menu item strings

  @param chunked Chunked buffer
  @param item Page number
*/
/**************************************************************************/
void stringPageName(ChunkedPrint &chunked, byte item) {
  switch (item) {
    case PAGE_INFO:
      chunked.print(F("System Info"));
      break;
    case PAGE_STATUS:
      chunked.print(F("P1P2 Status"));
      break;
    case PAGE_IP:
      chunked.print(F("IP Settings"));
      break;
    case PAGE_TCP:
      chunked.print(F("TCP/UDP Settings"));
      break;
    case PAGE_P1P2:
      chunked.print(F("P1P2 Settings"));
      break;
    case PAGE_FILTER:
      chunked.print(F("Packet Filter"));
      break;
    case PAGE_TOOLS:
      chunked.print(F("Tools"));
      break;
    default:
      break;
  }
}

/**************************************************************************/
/*!
  @brief Prints date and time

  @param chunked Chunked buffer
  @param myDate Date in Daikin format (6 bytes)
*/
/**************************************************************************/
void stringDate(ChunkedPrint &chunked, byte myDate[]) {
  if (myDate[5] == 0) return;  // day can not be zero
  chunked.print(myDate[5]);
  chunked.print(F("."));
  chunked.print(myDate[4]);
  chunked.print(F(".20"));
  chunked.print(myDate[3]);
  chunked.print(F(" "));
  if (myDate[1] < 10) chunked.print(F("0"));
  chunked.print(myDate[1]);
  chunked.print(F(":"));
  if (myDate[2] < 10) chunked.print(F("0"));
  chunked.print(myDate[2]);
}


/**************************************************************************/
/*!
  @brief Provide JSON value to a corresponding JSON key. The value is printed
  in <span> and in JSON document fetched on the background.
  @param chunked Chunked buffer
  @param JSONKEY JSON key
*/
/**************************************************************************/
void jsonVal(ChunkedPrint &chunked, const byte JSONKEY) {
  switch (JSONKEY) {
#ifdef ENABLE_EXTENDED_WEBUI
    case JSON_RUNTIME:
      chunked.print(seconds / (3600UL * 24L));
      chunked.print(F(" days, "));
      chunked.print((seconds / 3600UL) % 24L);
      chunked.print(F(" hours, "));
      chunked.print((seconds / 60UL) % 60L);
      chunked.print(F(" mins, "));
      chunked.print((seconds) % 60L);
      chunked.print(F(" secs"));
      break;
    case JSON_UDP_STATS:
      {
        for (byte i = 0; i < UDP_LAST; i++) {
          chunked.print(data.udpCnt[i]);
          switch (i) {
            case UDP_SENT:
              chunked.print(F(" Sent to UDP"));
              break;
            case UDP_RECEIVED:
              chunked.print(F(" Received from UDP"));
              break;
            default:
              break;
          }
          chunked.print(F("<br>"));
        }
      }
      break;
#endif /* ENABLE_EXTENDED_WEBUI */
    case JSON_DAIKIN_INDOOR:
      {
        chunked.print(daikinIndoor);
      }
      break;
#ifdef ENABLE_EXTENDED_WEBUI
    case JSON_DAIKIN_OUTDOOR:
      {
        chunked.print(daikinOutdoor);
      }
      break;
#endif /* ENABLE_EXTENDED_WEBUI */
    case JSON_DATE:
      {
        stringDate(chunked, date);
      }
      break;
    case JSON_DAIKIN_EEPROM_DATE:
      {
        stringDate(chunked, data.eepromDaikin.date);
      }
      break;
    case JSON_DAIKIN_EEPROM:
      {
        chunked.print(data.eepromDaikin.total);
        chunked.print(F(" Total Commands<br>"));
        if (date[5] != 0) {  // day can not be zero
          chunked.print((uint16_t)(data.eepromDaikin.total / (days(date) - days(data.eepromDaikin.date) + 1)));
        } else {
          chunked.print(F("-"));
        }
        chunked.print(F(" Daily Average (should be bellow 19)<br>"));
        chunked.print(data.eepromDaikin.yesterday);
        chunked.print(F(" Yesterday<br>"));
        chunked.print(data.eepromDaikin.today);
        chunked.print(F(" / "));
        chunked.print(data.config.writeQuota);
        chunked.print(F(" Today (total / quota) "));
        tagButton(chunked, F("Clear Quota"), ACT_CLEAR_QUOTA, true);
      }
      break;
    case JSON_CONTROLLER:
      {
        if (p1p2Timer.isOver()) {
          chunked.print(F("No connection to the P1P2 bus"));
          return;
        }
        bool availableSlot = false;
        for (byte i = 0; i < 16; i++) {
          if ((0xF0 | i) == controllerAddr) continue;
          if (FxRequests[i] == F0THRESHOLD) {
            availableSlot = true;
          }
        }
        chunked.print(F("This device is connected "));
        if (controllerAddr > CONNECTING) {  // controller is connected
          chunked.print(F("(address 0x"));
          chunked.print(controllerAddr, HEX);
          chunked.print(F(") "));
          if (data.config.controllerMode == CONTROL_MANUAL) {
            tagButton(chunked, F("Disable Write"), ACT_DISCONNECT, true);
          }
        } else {
          chunked.print(F("(read only) "));
          if (data.config.controllerMode == CONTROL_MANUAL) {
            if (controllerAddr == CONNECTING) {
              tagButton(chunked, F("Connecting"), ACT_CONNECT, false);
            } else {
              tagButton(chunked, F("Enable Write"), ACT_CONNECT, availableSlot);
            }
          }
        }
        chunked.print(F("<br>"));
        for (byte i = 0; i < 16; i++) {
          if ((FxRequests[i] == 0)                          // Skip address Fx if no 00Fx30 request was made (address Fx not supported by the pump)
              || ((0xF0 | i) == controllerAddr)) continue;  // Skip address Fx if this device uses address Fx
          if (FxRequests[i] < 0) {
            chunked.print(F("Another device is connected"));
          } else if (FxRequests[i] == F0THRESHOLD) {
            chunked.print(F("Additional device can be connected"));
          }
          chunked.print(F(" (address 0xF"));
          chunked.print(i, HEX);
          chunked.print(F(")<br>"));
        }
        if (!availableSlot) {
          chunked.print(F("Additional device not supported by the pump"));
        }
      }
      break;
    case JSON_WRITE_P1P2:
      {
        chunked.print(F(" <input type=submit value=Send"));
        if (controllerAddr <= CONNECTING) {  // DISCONNECTED or CONNECTING
          chunked.print(F(" disabled"));
        }
        chunked.print(F(">"));
      }
      break;
    case JSON_P1P2_STATS_DATE:
      {
        stringDate(chunked, data.statsDate);
      }
      break;
    case JSON_P1P2_STATS:
      {
        for (byte i = 0; i < P1P2_LAST; i++) {
          chunked.print(data.p1p2Cnt[i]);
          switch (i) {
            case P1P2_READ_OK:
              chunked.print(F(" Bus Read OK"));
              break;
            case P1P2_WRITE_OK:
              chunked.print(F(" Bus Write OK"));
              break;
            case P1P2_WRITE_QUOTA:
              chunked.print(F(" EEPROM Write Quota Reached"));
              break;
            case P1P2_WRITE_QUEUE:
              chunked.print(F(" Write Command Queue Full"));
              break;
            case P1P2_WRITE_INVALID:
              chunked.print(F(" Write Command Invalid"));
              break;
            case P1P2_ERROR_PE:
              chunked.print(F(" Parity Read"));
              break;
            case P1P2_LARGE:
              chunked.print(F(" Too Long Read"));
              break;
            case P1P2_ERROR_SB:
              chunked.print(F(" Start Bit Write"));
              break;
            case P1P2_ERROR_BE_BC:
              chunked.print(F(" Bus Collission Write"));
              break;
            case P1P2_ERROR_OR:
              chunked.print(F(" Buffer Overrun"));
              break;
            case P1P2_ERROR_CRC:
              chunked.print(F(" CRC"));
              break;
            default:
              break;
          }
          if (i >= P1P2_ERROR_PE) chunked.print(F(" Error"));
          chunked.print(F("<br>"));
        }
      }
      break;
    default:
      break;
  }
}
