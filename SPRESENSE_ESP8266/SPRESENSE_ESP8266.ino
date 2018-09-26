// This is a super simple demo program for ESP8266's that can be used by SPRESENSE
// Serial2 @ 115200 baud.

#define SSID "SSID"      //your wifi ssid here
#define PASS "PASSWD"   //your wifi key here

//  wifitest.adafruit.com/testwifi/index.html
#define HOST "wifitest.adafruit.com"
#define WEBPAGE "/testwifi/index.html"
#define PORT  "80"

#define LED_PIN LED0

#define REPLYBUFFSIZ 255
char replybuffer[REPLYBUFFSIZ];
uint8_t getReply(char *send, uint16_t timeout = 500, boolean echo = true);
uint8_t espreadline(uint16_t timeout = 500, boolean multiline = false);
boolean sendCheckReply(char *send, char *reply, uint16_t timeout = 500);

enum {WIFI_ERROR_NONE=0, WIFI_ERROR_AT, WIFI_ERROR_RST, WIFI_ERROR_SSIDPWD, WIFI_ERROR_SERVER, WIFI_ERROR_UNKNOWN};

void setup()
{
  pinMode(LED_PIN, OUTPUT);

  //blink LED0 to indicate power up
  for(int i = 0; i<3; i++)
  {
   digitalWrite(LED_PIN,HIGH);
   delay(50);
   digitalWrite(LED_PIN,LOW);
   delay(100);
  }

  // Serial debug console
  Serial.begin(115200);

  //Open software serial for chatting to ESP
  Serial2.begin(115200);

  Serial.println(F("SPRESENSE ESP8266 Demo"));

  //connect to the wifi
  byte err = setupWiFi();

  if (err) {
    // error, print error code
    Serial.print("setup error:");  Serial.println((int)err);

    debugLoop();
  }

  // success, print IP
  getIP();

  //set the single connection mode
  //sendCheckReply("AT+CIPSTO=0", "OK");
}

boolean ESP_GETpage(char *host, uint16_t port, char *page) {
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += host;
  cmd += "\",";
  cmd += port;
  cmd.toCharArray(replybuffer, REPLYBUFFSIZ);

  getReply(replybuffer);

  if (strcmp(replybuffer, "OK") != 0) {
     // this is OK! could be a version that says "Linked"
     if (strcmp(replybuffer, "Linked") != 0) {
       sendCheckReply("AT+CIPCLOSE", "OK");
       return false;
     }
  }

  delay(500);

  String request = "GET ";
  request += page;
  request += " HTTP/1.1\r\nHost: ";
  request += host;
  request += "\r\n\r\n";

  cmd = "AT+CIPSEND=";
  cmd += request.length();
  cmd.toCharArray(replybuffer, REPLYBUFFSIZ);
  sendCheckReply(replybuffer, ">");

  Serial.print("Sending: "); Serial.println(request.length());
  Serial.println(F("*********SENDING*********"));
  Serial.print(request);
  Serial.println(F("*************************"));

  request.toCharArray(replybuffer, REPLYBUFFSIZ);

  Serial2.println(request);

  while (true) {
    espreadline(3000);  // this is the 'echo' from the data
    Serial.print(">"); Serial.println(replybuffer); // probably the 'busy s...'

    // LOOK AT ALL THESE POSSIBLE ARBITRARY RESPONSES!!!
    if (strstr(replybuffer, "wrong syntax"))
      continue;
    else if (strstr(replybuffer, "ERROR"))
      continue;
    else if (strstr(replybuffer, "busy s..."))
      continue;
    else break;
  }

//  if (! strstr(replybuffer, "SEND OK") ) return false;

  if (! strstr(replybuffer, "OK") ) return false;

  espreadline(1000);
  Serial.print("3>"); Serial.println(replybuffer);
  char *s = strstr(replybuffer, "+IPD,");
  if (!s) return false;
  uint16_t len = atoi(s+5);
  //Serial.print(len); Serial.println(" bytes total");

  int16_t contentlen = 0;
  while (1) {
    espreadline(50);
    s = strstr(replybuffer, "Content-Length: ");
    if (s) {
      //Serial.println(replybuffer);
      contentlen = atoi(s+16);
      Serial.print(F("C-Len = ")); Serial.println(contentlen);
    }
    s = strstr(replybuffer, "Content-Type: ");
    if (s && contentlen) {
      int16_t i;
      char c;

      for (i=-2; i<contentlen; i++) {  // eat the first 2 chars (\n\r)
        while (!Serial2.available());
        c = Serial2.read(); //UDR0 = c;
        if (i >= 0) {
          replybuffer[i] = c;
        }
      }
      replybuffer[i] = 0;
      return true;
    }
  }
  //while (1) {
  //  if (esp.available()) UDR0 = esp.read();
  //}
}

void loop()
{
  ESP_GETpage(HOST, 80, WEBPAGE);

  Serial.println(F("**********REPLY***********"));
  Serial.println(replybuffer);
  Serial.println(F("**************************"));

  sendCheckReply("AT+CIPCLOSE", "OK");
  while (true) {
    espreadline(3000);  // this is the 'echo' from the data
    Serial.print(">"); Serial.println(replybuffer); // probably the 'busy s...'

    if (strstr(replybuffer, "busy s..."))
      continue;
    else break;
  }

  debugLoop();

  delay(1000);

  while (1);

}

boolean getVersion() {
  // Get version?
  getReply("AT+GMR", 100, true);
  while (true) {
    espreadline(50);  // this is the 'echo' from the data
    Serial.print("<--- "); Serial.println(replybuffer);
    if (strstr(replybuffer, "OK"))
    break;
  }
  return true;
}

boolean espReset() {
  getReply("AT+RST", 1000, true);
  if (! strstr(replybuffer, "OK")) return false;
  delay(500);

  // turn off echo
  getReply("ATE0", 250, true);

  return true;
}

boolean ESPconnectAP(char *s, char *p) {

  getReply("AT+CWMODE=1", 500, true);
  if (! (strstr(replybuffer, "OK") || strstr(replybuffer, "no change")) )
    return false;

  String connectStr = "AT+CWJAP=\"";
  connectStr += SSID;
  connectStr += "\",\"";
  connectStr += PASS;
  connectStr += "\"";
  connectStr.toCharArray(replybuffer, REPLYBUFFSIZ);
  getReply(replybuffer, 200, true);

  while (true) {
    espreadline(200);  // this is the 'echo' from the data
    if((String)replybuffer == ""){
      Serial.print(".");
    }else{
      Serial.println("");
      Serial.print("<--- "); Serial.println(replybuffer);
    }
    if (strstr(replybuffer, "OK"))
    break;
  }

  return true;
}


byte setupWiFi() {
  // reset WiFi module
  Serial.println(F("Soft resetting..."));
  if (!espReset())
    return WIFI_ERROR_RST;

  delay(500);

  Serial.println(F("Checking for ESP AT response"));
  if (!sendCheckReply("AT", "OK"))
    return WIFI_ERROR_AT;

  getVersion();

  Serial.print(F("Connecting to ")); Serial.println(SSID);
  if (!ESPconnectAP(SSID, PASS))
    return WIFI_ERROR_SSIDPWD;

  Serial.println(F("Single Client Mode"));

  if (!sendCheckReply("AT+CIPMUX=0", "OK"))
        return WIFI_ERROR_SERVER;

  return WIFI_ERROR_NONE;
}

boolean getIP() {
  getReply("AT+CIFSR", 100, true);
  while (true) {
    espreadline(50);  // this is the 'echo' from the data
    Serial.print("<--- "); Serial.println(replybuffer);
    if (strstr(replybuffer, "OK"))
    break;
  }

  delay(100);

  return true;
}




/************************/
uint8_t espreadline(uint16_t timeout, boolean multiline) {
  uint16_t replyidx = 0;

  while (timeout--) {
    if (replyidx > REPLYBUFFSIZ-1) break;

    while(Serial2.available()) {
      char c =  Serial2.read();
      if (c == '\r') continue;
      if (c == 0xA) {
        if (replyidx == 0)   // the first 0x0A is ignored
          continue;

        if (!multiline) {
          timeout = 0;         // the second 0x0A is the end of the line
          break;
        }
      }
      replybuffer[replyidx] = c;
      // Serial.print(c, HEX); Serial.print("#"); Serial.println(c);
      replyidx++;
    }

    if (timeout == 0) break;
    delay(1);
  }
  replybuffer[replyidx] = 0;  // null term
  return replyidx;
}

uint8_t getReply(char *send, uint16_t timeout, boolean echo) {
  // flush input
  while(Serial2.available()) {
     Serial2.read();
     delay(1);
  }

  if (echo) {
    Serial.print("---> "); Serial.println(send);
  }
  Serial2.println(send);

  // eat first reply sentence (echo)
  uint8_t readlen = espreadline(timeout);

  //Serial.print("echo? "); Serial.print(readlen); Serial.print(" vs "); Serial.println(strlen(send));

  if (strncmp(send, replybuffer, readlen) == 0) {
    // its an echo, read another line!
    readlen = espreadline();
  }

  if (echo) {
    Serial.print ("<--- "); Serial.println(replybuffer);
  }
  return readlen;
}

boolean sendCheckReply(char *send, char *reply, uint16_t timeout) {

  getReply(send, timeout, true);

 /*
  for (uint8_t i=0; i<strlen(replybuffer); i++) {
    Serial.print(replybuffer[i], HEX); Serial.print(" ");
  }
  Serial.println();
  for (uint8_t i=0; i<strlen(reply); i++) {
    Serial.print(reply[i], HEX); Serial.print(" ");
  }
  Serial.println();
*/
  return (strcmp(replybuffer, reply) == 0);
}

void debugLoop() {
  Serial.println("========================");
  //serial loop mode for diag
  while(1) {
    if (Serial.available()) {
      Serial2.write(Serial.read());
//      delay(1);
    }
    if (Serial2.available()) {
      Serial.write(Serial2.read());
//      delay(1);
    }
  }
}
