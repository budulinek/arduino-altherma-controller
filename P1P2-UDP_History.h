/* P1P2_Daikin_History.h code for saving history of incoming packets

   Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)

*/

#ifndef P1P2_Daikin_History
#define P1P2_Daikin_History

const int savedPacketsNum = 2 * (savedPacketsRange[1] - savedPacketsRange[0] + 1);

static byte rbhistory[SAVED_HISTORY_BYTES];              // history storage
static uint16_t savehistoryend = 0;         // #bytes saved so far
static uint16_t savehistoryp[savedPacketsNum] = {};   // history pointers
static  uint8_t savehistorylen[savedPacketsNum] = {}; // length of history for a certain packet


int8_t savehistoryindex(byte *rb) {
  // returns whether rb should be saved,
  //    and if so, returns index of rbhistoryp to store history data
  // returns -1 if this packet is not to be saved
  // returns a value in the range of 0..(savedPacketsNum-1) if message is to be saved as a packet
  int8_t rv;
  if ((rb[2] >= savedPacketsRange[0]) && (rb[2] <= savedPacketsRange[1]) && ((rb[0] & 0xBF) == 0x00)) {
    rv = ((rb[2] & 0x0F) + ((rb[0] & 0x40) ? (savedPacketsRange[1] - savedPacketsRange[0] + 1) : 0));
    return rv;
  }
  return -1;
}


void savehistory(byte *rb, int n) {
  // also modifies savehistoryp, savehistorylen, savehistoryend
  if (n > 3) {
    int8_t shi = savehistoryindex(rb);
    if (shi >= 0) {
      // the first 3 bytes of a packet can be ignored
      uint8_t shign = 3;
      if (!savehistorylen[shi]) {
        if (savehistoryend + (n - shign) <= SAVED_HISTORY_BYTES) {
          savehistoryp[shi] = savehistoryend;
          savehistorylen[shi] = n - shign;
          savehistoryend += (n - shign);
        } else {
          // Memory error: Not enough memory to store saved packets. Increase SAVED_HISTORY_BYTES or adjust savedPacketsRange[].
          error(0x40);
        }
      }
      if (savehistorylen[shi]) {
        for (byte i = shign; ((i < n) && (i < shign + savehistorylen[shi])); i++) {
          // copying history
          rbhistory[savehistoryp[shi] + (i - shign)] = rb[i];
        }
      }
    }
  }
}

bool changedPacket(byte *rb, int n) {
  // returns whether packet has new values (also returns true if packet has not been saved)
  int8_t shi = savehistoryindex(rb);
  if (shi >= 0) {
    // usually the first 3 bytes can be ignored
    byte shign = 3;
    if (n == (savehistorylen[shi] + shign)) {
      // enough history available, so compare
      for (byte i = 0; i < n - shign; i++) {
        if ((rbhistory[i + savehistoryp[shi]]) != rb[i + shign]) {
          return 1;
        }
      }
      // no diff found
      return 0;
    } else {
      // no or not enough history in the partly saved packet, so assume value is new
      return 1;
    }
  } else {
    // no history for this packet, assume value is new
    return 1;
  }
}

void deletehistory() {
  memset(rbhistory, 0, sizeof(rbhistory));
  savehistoryend = 0;
  memset(savehistoryp, 0, sizeof(savehistoryp));
  memset(savehistorylen, 0, sizeof(savehistorylen));
}


#endif /* P1P2_Daikin_History */
