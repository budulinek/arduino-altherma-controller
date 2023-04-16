/* *******************************************************************
   Pages for Webserver

   sendPage()
   - sends the requested page (incl. 404 error and JSON document)
   - displays main page, renders title and left menu using <div> 
   - calls content functions depending on the number (i.e. URL) of the requested web page
   - also displays buttons for some of the pages
   - in order to save flash memory, some HTML closing tags are omitted, new lines in HTML code are also omitted

   contentInfo(), contentStatus(), contentIp(), contentTcp(), contentP1P2()
   - render the content of the requested page

   contentWait()
   - renders the "please wait" message instead of the content, will be forwarded to home page after 5 seconds

   tagInputNumber(), tagLabelDiv(), tagButton(), tagDivClose(), tagSpan()
   - render snippets of repetitive HTML code for <input>, <label>, <div>, <button> and <span> tags

   stringPageName(), stringStats()
   - renders repetitive strings for menus, error counters

   jsonVal()
   - provide JSON value to a corresponding JSON key

   ***************************************************************** */

const byte WEB_OUT_BUFFER_SIZE = 64;  // size of web server write buffer (used by StreamLib)

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
    chunked.print(F("HTTP/1.1 200\r\n"
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
                  // "Connection: close\r\n"
                  "Content-Type: text/html\r\n"
                  "Transfer-Encoding: chunked\r\n"
                  "\r\n"));
  chunked.begin();
  chunked.print(F("<!DOCTYPE html>"
                  "<html>"
                  "<head>"
                  "<meta"));
  if (reqPage == PAGE_WAIT) {  // redirect to new IP and web port
    chunked.print(F(" http-equiv=refresh content=5;url=http://"));
    chunked.print(IPAddress(localConfig.ip));
    chunked.print(F(":"));
    chunked.print(localConfig.webPort);
  }
  chunked.print(F(">"
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
                    c - content of a page
                    r - row inside a content
                    i - short input (byte or IP address octet)
                    n - input type=number
                    s - select input with numbers
                    p - inputs disabled by id=o checkbox
                  CSS Ids
                    o - parent checkbox which disables other checkboxes and inputs
                  */
                  "body,.m{padding:1px;margin:0;font-family:sans-serif}"
                  "h1,h4{padding:10px}"
                  "h1,.m,h4{background:#0067AC;margin:1px}"
                  ".m,.c{height:calc(100vh - 71px)}"
                  ".m{min-width:20%}"
                  ".c{flex-grow:1;overflow-y:auto}"
                  ".w,.r{display:flex}"
                  "a,h1,h4{color:white;text-decoration:none}"
                  ".c h4{padding-left:30%;margin-bottom:20px}"
                  ".r{margin:4px}"
                  "label{width:30%;text-align:right;margin-right:2px}"
                  "input,button,select{margin-top:-2px}"  // improve vertical allignment of input, button and select
                  ".s{text-align:right}"
                  ".s>option{direction:rtl}"
                  ".i{text-align:center;width:3ch}"
                  ".n{width:8ch}"
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
  for (byte i = 1; i < PAGE_WAIT; i++) {  // RTU Settings are the last item in the left menu
    chunked.print(F("<h4 "));
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
    case PAGE_WAIT:
      contentWait(chunked);
      break;
    default:
      break;
  }

  if (reqPage == PAGE_IP || reqPage == PAGE_TCP || reqPage == PAGE_P1P2 || reqPage == PAGE_FILTER) {
    chunked.print(F("<p><div class=r><label><input type=submit value='Save & Apply'></label><input type=reset value=Cancel></div>"));
  }
  chunked.print(F("</form>"));
  tagDivClose(chunked);  // close tags <div class=c> <div class=w>
  chunked.end();         // closing tags not required </body></html>
}


//        System Info
void contentInfo(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("SW Version"));
  chunked.print(VERSION[0]);
  chunked.print(F("."));
  chunked.print(VERSION[1]);
  tagButton(chunked, F("Load Default Settings"), ACT_FACTORY);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Microcontroller"));
  chunked.print(BOARD);
  tagButton(chunked, F("Reboot"), ACT_REBOOT);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("EEPROM Health"));
  chunked.print(eepromCount.eepromWrites);
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
  tagLabelDiv(chunked, F("MAC Address"));
  byte macBuffer[6];
  W5100.getMACAddress(macBuffer);
  for (byte i = 0; i < 6; i++) {
    chunked.print(hex(macBuffer[i]));
    if (i < 5) chunked.print(F(":"));
  }
  tagButton(chunked, F("Generate New MAC"), ACT_MAC);
  tagDivClose(chunked);

#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("Auto IP"));
  if (!localConfig.enableDhcp) {
    chunked.print(F("DHCP disabled"));
  } else if (dhcpSuccess == true) {
    chunked.print(F("DHCP successful"));
  } else {
    chunked.print(F("DHCP failed, using fallback static IP"));
  }
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */

  tagLabelDiv(chunked, F("IP Address"));
  chunked.print(IPAddress(Ethernet.localIP()));
  tagDivClose(chunked);
}

//        P1P2 Status
void contentStatus(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Controller"));
  tagSpan(chunked, JSON_CONTROLLER);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Daikin Indoor Unit"));
  tagSpan(chunked, JSON_DAIKIN_INDOOR);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Daikin Outdoor Unit"));
  tagSpan(chunked, JSON_DAIKIN_OUTDOOR);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Date"));
  tagSpan(chunked, JSON_DATE);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Daikin EEPROM Writes"));
  tagButton(chunked, F("Reset"), ACT_RESET_EEPROM);
  tagSpan(chunked, JSON_DAIKIN_EEPROM);
  tagDivClose(chunked);
  chunked.print(F("</form><form method=post>"));
  tagLabelDiv(chunked, F("Write Packet"));
  chunked.print(F("Type "
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
    tagInputHex(chunked, POST_CMD_PARAM_1 + i, true);
  }
  chunked.print(F(" Value "));
  for (byte i = 0; i <= POST_CMD_VAL_4 - POST_CMD_VAL_1; i++) {
    tagInputHex(chunked, POST_CMD_VAL_1 + i, false);
  }
  tagSpan(chunked, JSON_WRITE_P1P2);
  tagDivClose(chunked);
  chunked.print(F("</form><form method=post>"));
#ifdef ENABLE_EXTRA_DIAG
  tagLabelDiv(chunked, F("Run Time"));
  tagSpan(chunked, JSON_RUNTIME);
  tagDivClose(chunked);
#endif /* ENABLE_EXTRA_DIAG */
  tagLabelDiv(chunked, F("P1P2 Packets"));
  tagButton(chunked, F("Reset Stats"), ACT_RESET_STATS);
  tagSpan(chunked, JSON_P1P2_STATS);
  tagDivClose(chunked);
#ifdef ENABLE_EXTRA_DIAG
  tagLabelDiv(chunked, F("UDP Messages"));
  tagSpan(chunked, JSON_UDP_STATS);
  tagDivClose(chunked);
#endif /* ENABLE_EXTRA_DIAG */
}

//            IP Settings
void contentIp(ChunkedPrint &chunked) {

#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("Auto IP"));
  tagCheckbox(chunked, POST_SEND_ALL, localConfig.enableDhcp, true, false);
  chunked.print(F(" DHCP"));
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */

  byte *tempIp;
  for (byte j = 0; j < 3; j++) {
    switch (j) {
      case 0:
        tagLabelDiv(chunked, F("Static IP"));
        tempIp = localConfig.ip;
        break;
      case 1:
        tagLabelDiv(chunked, F("Submask"));
        tempIp = localConfig.subnet;
        break;
      case 2:
        tagLabelDiv(chunked, F("Gateway"));
        tempIp = localConfig.gateway;
        break;
      default:
        break;
    }
    tagInputIp(chunked, POST_IP + (j * 4), tempIp);
    tagDivClose(chunked);
  }
#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("DNS Server"));
  tagInputIp(chunked, POST_DNS, localConfig.dns);
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */
}

//            TCP/UDP Settings
void contentTcp(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Remote IP"));
  tagInputIp(chunked, POST_REM_IP, localConfig.remoteIp);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Send and Receive UDP"));
  static const __FlashStringHelper *optionsList[] = {
    F("Only to/from Remote IP"),
    F("To/From Any IP (Broadcast)")
  };
  tagSelect(chunked, POST_UDP_BROADCAST, optionsList, 2, localConfig.udpBroadcast);
  tagDivClose(chunked);
  unsigned int value;
  for (byte i = 0; i < 2; i++) {
    switch (i) {
      case 0:
        tagLabelDiv(chunked, F("UDP Port"));
        value = localConfig.udpPort;
        break;
      case 1:
        tagLabelDiv(chunked, F("WebUI Port"));
        value = localConfig.webPort;
        break;
      default:
        break;
    }
    tagInputNumber(chunked, POST_UDP + i, 1, 65535, value, F(""));
    tagDivClose(chunked);
  }
}


//            P1P2 Settings
void contentP1P2(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Controller (Write to P1P2)"));
  static const __FlashStringHelper *optionsList[] = {
    F("Disabled"),
    F("Manual Connect"),
    F("Auto Connect")
  };
  tagSelect(chunked, POST_CONTROL_MODE, optionsList, 3, localConfig.controllerMode);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Connection Timeout"));
  tagInputNumber(chunked, POST_TIMEOUT, F0THRESHOLD, 60, localConfig.connectTimeout, F("secs"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Target Temp. Hysteresis"));
  tagInputNumber(chunked, POST_HYSTERESIS, 1, 5, localConfig.hysteresis, F("\xB0"
                                                                           "C"));
  tagDivClose(chunked);
}


//            Packet Filter
void contentFilter(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Send All Packet Types"));
  tagCheckbox(chunked, POST_SEND_ALL, localConfig.sendAllPackets, true, false);
  chunked.print(F("Enable (use with caution!)"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Counters Packet"));
  chunked.print(F("Request Every "));
  tagInputNumber(chunked, POST_COUNTER_PERIOD, 1, 60, localConfig.counterPeriod, F("mins"));
  tagDivClose(chunked);
  tagRowPacket(chunked, PACKET_TYPE_COUNTER);
  tagLabelDiv(chunked, F("Data Packets"));
  static const __FlashStringHelper *optionsList[] = {
    F("Always Send (~770ms cycle)"),
    F("Send If Payload Changed")
  };
  tagSelect(chunked, POST_SAVE_DATA_PACKETS, optionsList, 2, localConfig.saveDataPackets);
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

void contentWait(ChunkedPrint &chunked) {
  chunked.print(F("Reloading. Please wait..."));
}

// Functions providing snippets of repetitive HTML code

void tagRowPacket(ChunkedPrint &chunked, const byte packetType) {
  if (getPacketStatus(packetType, PACKET_SEEN) == true) {
    chunked.print(F("<div class=r>"
                    "<label>Type 0x"));
    chunked.print(hex(packetType));
    chunked.print(F(":</label>"
                    "<div>"));
    tagCheckbox(chunked, packetType, getPacketStatus(packetType, PACKET_SENT), false, true);
    tagDivClose(chunked);
  }
}

void tagInputNumber(ChunkedPrint &chunked, const byte name, unsigned int min, unsigned int max, unsigned int value, const __FlashStringHelper *units) {
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

void tagInputHex(ChunkedPrint &chunked, const byte name, bool required) {
  chunked.print(F("<input name="));
  chunked.print(name, HEX);
  if (required) {
    chunked.print(F(" required"));  // first 3 bytes are required (1 byte packet type and 2 bytes param number)
  }
  chunked.print(F(" minlength=2 maxlength=2 class=i pattern='[a-fA-F&bsol;d]+'>"));
}

void tagLabelDiv(ChunkedPrint &chunked, const __FlashStringHelper *label) {
  chunked.print(F("<div class=r>"
                  "<label>"));
  chunked.print(label);
  chunked.print(F(":</label>"
                  "<div>"));
}

void tagButton(ChunkedPrint &chunked, const __FlashStringHelper *flashString, byte value) {
  chunked.print(F(" <button name="));
  chunked.print(POST_ACTION, HEX);
  chunked.print(F(" value="));
  chunked.print(value);
  chunked.print(F(">"));
  chunked.print(flashString);
  chunked.print(F("</button><br>"));
}

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

void tagDivClose(ChunkedPrint &chunked) {
  chunked.print(F("</div>"
                  "</div>"));  // <div class=r>
}

void tagSpan(ChunkedPrint &chunked, const byte JSONKEY) {
  chunked.print(F("<span id="));
  chunked.print(JSONKEY);
  chunked.print(F(">"));
  jsonVal(chunked, JSONKEY);
  chunked.print(F("</span>"));
}


// Menu item strings
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
    default:
      break;
  }
}

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


void jsonVal(ChunkedPrint &chunked, const byte JSONKEY) {
  switch (JSONKEY) {
#ifdef ENABLE_EXTRA_DIAG
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
          chunked.print(udpCount[i]);
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
#endif /* ENABLE_EXTRA_DIAG */
    case JSON_DAIKIN_INDOOR:
      {
        chunked.print(daikinIndoor);
      }
      break;
    case JSON_DAIKIN_OUTDOOR:
      {
        chunked.print(daikinOutdoor);
      }
      break;
    case JSON_DATE:
      {
        stringDate(chunked, date);
      }
      break;
    case JSON_DAIKIN_EEPROM:
      {
        unsigned int days = (date[3] * 365) + ((date[4] - 1) * 30) + date[5];
        unsigned int eepromDays = (eepromCount.eepromDate[3] * 365) + ((eepromCount.eepromDate[4] - 1) * 30) + eepromCount.eepromDate[5];
        chunked.print(eepromCount.daikinWrites);
        chunked.print(F(" Total<br>"));
        // if (eepromCount.eepromDate[5] == 0 || date[5] == 0) return;
        chunked.print((unsigned int)(eepromCount.daikinWrites / (days - eepromDays + 1)));
        chunked.print(F(" Average per Day (should be bellow 19)<br>"));
        chunked.print(eepromCount.yesterdayWrites);
        chunked.print(F(" Yesterday"));
      }
      break;
    case JSON_CONTROLLER:
      {
        switch (controllerState) {
          case DISABLED:
            chunked.print(F("Disabled "));
            break;
          case DISCONNECTED:
            chunked.print(F("Disconnected "));
            break;
          case CONNECTING:
            chunked.print(F("Connecting... "));
            break;
          case CONNECTED:
            chunked.print(F("Connected "));
            break;
          case NOT_SUPPORTED:
            chunked.print(F("Not Supported by the Pump "));
            break;
          default:
            break;
        }
        switch (controllerState) {
          case DISCONNECTED:
          case NOT_SUPPORTED:
            tagButton(chunked, F("Connect"), ACT_CONNECT);
            break;
          case CONNECTING:
          case CONNECTED:
            if (localConfig.controllerMode == CONTROL_MANUAL) {  // Disconnect button available only in manual connect mode (not in auto connect mode)
              tagButton(chunked, F("Disconnect"), ACT_DISCONNECT);
            }
            break;
          default:
            break;
        }
      }
      break;
    case JSON_WRITE_P1P2:
      {
        chunked.print(F(" <input type=submit value=Send"));
        if (controllerState != CONNECTED) {
          chunked.print(F(" disabled"));
        }
        chunked.print(F(">"));
      }
      break;
    case JSON_P1P2_STATS:
      {
        for (byte i = 0; i < P1P2_LAST; i++) {
          chunked.print(p1p2Count[i]);
          switch (i) {
            case P1P2_READ:
              chunked.print(F(" Read OK"));
              break;
            case P1P2_WRITTEN:
              chunked.print(F(" Write OK"));
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
