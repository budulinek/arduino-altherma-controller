unsigned int listenPort = 10034;                                  // local listening port
unsigned int sendPort = 10000;                                    // local sending port
unsigned int remPort = 10034;                                     // remote port
IPAddress ip(192, 168, 1, 34);                                       // local IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress sendIpAddress(192, 168, 1, 255);                      // remote IP, we use use broadcast, because unicast is too slow and leads to bus timeout errors if IP not found
#define ETH_RESET_PIN 7                                       // Cheap Ethernet shield clones often have problems with initialization. 
                                                               // Solution: disconnect (bend) both reset pins between Uno and ethernet shield ("RESET" pin and another in ICSP connector,
                                                               // then connect RESET pin from ethernet shield to some digital pin (pin 7 in our case)
