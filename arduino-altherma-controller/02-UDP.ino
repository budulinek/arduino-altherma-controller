/* *******************************************************************
   UDP functions

   recvUdp()
   - receives P1P2 command via UDP
   - calls checkCommand

   checkCommand()
   - checks P1P2 command
   - checks availability of queue
   - stores commands into queue or returns an error

   deleteCmd()
   - deletes command from queue

   clearQueue()
   - clears queue and corresponding counters

   getPacketStatus(), setPacketStatus()
   - read from and write to bool array

   ***************************************************************** */

byte masks[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

void recvUdp() {
  uint16_t udpLen = Udp.parsePacket();
  if (udpLen) {
    byte command[1 + 2 + MAX_PARAM_SIZE];  // 1 byte packet type + 2 bytes param number + MAX_PARAM_SIZE bytes param value
    if (udpLen > sizeof(command) || (!localConfig.udpBroadcast && Udp.remoteIP() != IPAddress(localConfig.remoteIp))) {
      while (Udp.available()) Udp.read();
      // TODO error: UDP too long or wrong remote IP
      return;
    }
    Udp.read(command, sizeof(command));
    checkCommand(command, byte(udpLen));
#ifdef ENABLE_EXTRA_DIAG
    udpCount[UDP_RECEIVED]++;
#endif /* ENABLE_EXTRA_DIAG */
  }
}

void checkCommand(byte command[], byte cmdLen) {
  if (cmdQueue.available() > cmdLen) {                                                               // check available space in queue
    if (PACKET_PARAM_VAL_SIZE[command[0] - PACKET_TYPE_CONTROL[FIRST]] != 0                          // check if param size is not zero (0 = write command not supported (yet))
        && cmdLen - 3 == (PACKET_PARAM_VAL_SIZE[command[0] - PACKET_TYPE_CONTROL[FIRST]])            // check parameter value size
        && (command[0] >= PACKET_TYPE_CONTROL[FIRST] && command[0] <= PACKET_TYPE_CONTROL[LAST])) {  // check packet type
      if (changed36Param(command) == true) {
        // push to queue (incl. cmdLen)
        cmdQueue.push(cmdLen);  // first byte in queue is cmdLen
        for (byte i = 0; i < cmdLen; i++) {
          cmdQueue.push(command[i]);
        }
      }
    } else {
      p1p2Count[P1P2_WRITE_INVALID]++;  // Write Command Invalid
    }
  } else {
    p1p2Count[P1P2_WRITE_QUEUE]++;  // TODO error: Write Queue Full
  }
}

void deleteCmd()  // delete command from queue
{
  byte cmdLen = cmdQueue.first();
  for (byte i = 0; i <= cmdLen; i++) {
    cmdQueue.shift();
  }
}

bool getPacketStatus(const byte packetType, const byte status) {
  return (localConfig.packetStatus[status][packetType / 8] & masks[packetType & 7]) > 0;
}

bool setPacketStatus(const byte packetType, byte status, const bool value) {
  if (getPacketStatus(packetType, status) == value) return false;
  if (value == 0) {
    localConfig.packetStatus[status][packetType / 8] &= ~masks[packetType & 7];
  } else {
    localConfig.packetStatus[status][packetType / 8] |= masks[packetType & 7];
  }
  return true;  // return true if value changed
}
