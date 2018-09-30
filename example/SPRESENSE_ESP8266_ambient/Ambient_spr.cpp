/*
 * ambient.cpp - Library for sending data to Ambient
 * Created by Takehiko Shimojima, April 21, 2016
 */
#include "Ambient_spr.h"

#define AMBIENT_DEBUG 1

#if AMBIENT_DEBUG
#define DBG(...) { Serial.print(__VA_ARGS__); }
#define ERR(...) { Serial.print(__VA_ARGS__); }
#else
#define DBG(...)
#define ERR(...)
#endif /* AMBIENT_DBG */

const char* AMBIENT_HOST = "54.65.206.59";
int AMBIENT_PORT = 80;
const char* AMBIENT_HOST_DEV = "192.168.0.11";
int AMBIENT_PORT_DEV = 4567;

const char * ambient_keys[] = {"\"d1\":\"", "\"d2\":\"", "\"d3\":\"", "\"d4\":\"", "\"d5\":\"", "\"d6\":\"", "\"d7\":\"", "\"d8\":\"", "\"lat\":\"", "\"lng\":\"", "\"created\":\""};

Ambient::Ambient() {
}

bool
// Ambient::begin(unsigned int channelId, const char * writeKey, WiFiClient * c, int dev) {
Ambient::begin(unsigned int channelId, const char * writeKey, int dev) {
  if(AMBIENT_DEBUG){
    Serial.println("Initialize Ambient");
  }

   this->channelId = channelId;

    if (sizeof(writeKey) > AMBIENT_WRITEKEY_SIZE) {
        ERR("writeKey length > AMBIENT_WRITEKEY_SIZE");
        return false;
    }
    strcpy(this->writeKey, writeKey);

//    if(NULL == c) {
//        ERR("Socket Pointer is NULL, open a socket.");
//        return false;
//    }
//    this->client = c;
    this->dev = dev;
    if (dev) {
        strcpy(this->host, AMBIENT_HOST_DEV);
        this->port = AMBIENT_PORT_DEV;
    } else {
        strcpy(this->host, AMBIENT_HOST);
        this->port = AMBIENT_PORT;
    }
    for (int i = 0; i < AMBIENT_NUM_PARAMS; i++) {
        this->data[i].set = false;
    }
    if(AMBIENT_DEBUG){
      Serial.println("Host: " + (String)this->host + ", Port: " + this->port);
      Serial.println("CannelID: " + (String)this->channelId + ", Write Key: " + (String)this->writeKey);
    }

    return true;
}

bool
Ambient::set(int field,const char * data) {
    --field;
    if (field < 0 || field >= AMBIENT_NUM_PARAMS) {
        return false;
    }
    if (strlen(data) > AMBIENT_DATA_SIZE) {
        return false;
    }
    this->data[field].set = true;
    strcpy(this->data[field].item, data);

    Serial.println(String(field) + ", " + String(this->data[field].item));

    return true;
}

bool Ambient::set(int field, double data)
{
	return set(field,String(data).c_str());
}

bool Ambient::set(int field, int data)
{
	return set(field, String(data).c_str());
}

bool
Ambient::clear(int field) {
    --field;
    if (field < 0 || field >= AMBIENT_NUM_PARAMS) {
        return false;
    }
    this->data[field].set = false;

    return true;
}

bool
Ambient::send() {

	uint16_t READBUFFSIZ = 0xFF;
	char readbuffer[READBUFFSIZ];
	String cmd;
	int retry;

	for (retry = 0; retry < AMBIENT_MAX_RETRY; retry++) {
		int ret;
//        ret = this->client->connect(this->host, this->port);
		ret = this->connect(this->host, this->port);
		if (ret) {
			break ;
		}
	}

  if(retry == AMBIENT_MAX_RETRY) {
      ERR("Could not connect socket to host\r\n");
      return false;
  }

  char str[180];
  char body[192];
  char inChar;

  memset(body, 0, sizeof(body));
  strcat(body, "{\"writeKey\":\"");
  strcat(body, this->writeKey);
  strcat(body, "\",");

  for (int i = 0; i < AMBIENT_NUM_PARAMS; i++) {
    if (this->data[i].set) {
      strcat(body, ambient_keys[i]);
      strcat(body, this->data[i].item);
      strcat(body, "\",");
    }
  }
  body[strlen(body) - 1] = '\0';
  strcat(body, "}\r\n");

  memset(str, 0, sizeof(str));
  sprintf(str, "POST /api/v2/channels/%d/data HTTP/1.1\r\n", this->channelId);
  if (this->port == 80) {
    sprintf(&str[strlen(str)], "Host: %s\r\n", this->host);
  } else {
    sprintf(&str[strlen(str)], "Host: %s:%d\r\n", this->host, this->port);
  }
  sprintf(&str[strlen(str)], "Content-Length: %d\r\n", strlen(body));
  sprintf(&str[strlen(str)], "Content-Type: application/json\r\n\r\n");

  DBG("sending: ");DBG(strlen(str));DBG(" bytes\r\n");DBG(str);

  int ret;
//    ret = this->client->print(str);
	ret = this->print(str, "SEND OK");

  delay(30);
  DBG(ret);DBG(" bytes sent\n\n");
  if (ret == 0) {
    ERR("send failed\n");
    return false;
  }
//    ret = this->client->print(body);
  ret = this->print(body, "SEND OK");

  delay(30);
  DBG(ret);DBG(" bytes sent\n\n");
  if (ret == 0) {
    ERR("send failed\n");
    return false;
  }

//    while (this->client->available()) {
  while (Serial2.available()) {
//        inChar = this->client->read();
    inChar = Serial2.read();
    if(AMBIENT_DEBUG){
      Serial.write(inChar);
    }
  }

//    this->client->stop();
	this->stop();

  for (int i = 0; i < AMBIENT_NUM_PARAMS; i++) {
    this->data[i].set = false;
  }
  return true;
}

int
Ambient::bulk_send(char *buf) {

    int retry;
    for (retry = 0; retry < AMBIENT_MAX_RETRY; retry++) {
        int ret;
//        ret = this->client->connect(this->host, this->port);
		ret = this->connect(this->host, this->port);
        if (ret) {
            break ;
        }
    }
    if(retry == AMBIENT_MAX_RETRY) {
        ERR("Could not connect socket to host\r\n");
        return -1;
    }

    char str[180];
    char inChar;

    memset(str, 0, sizeof(str));
    sprintf(str, "POST /api/v2/channels/%d/dataarray HTTP/1.1\r\n", this->channelId);
    if (this->port == 80) {
        sprintf(&str[strlen(str)], "Host: %s\r\n", this->host);
    } else {
        sprintf(&str[strlen(str)], "Host: %s:%d\r\n", this->host, this->port);
    }
    sprintf(&str[strlen(str)], "Content-Length: %d\r\n", strlen(buf));
    sprintf(&str[strlen(str)], "Content-Type: application/json\r\n\r\n");

    DBG("sending: ");DBG(strlen(str));DBG("bytes\r\n");DBG(str);

    int ret;
//    ret = this->client->print(str); // send header
	ret = this->print(str, "SEND OK");
    delay(30);
    DBG(ret);DBG(" bytes sent\n\n");
    if (ret == 0) {
        ERR("send failed\n");
        return -1;
    }

    int sent = 0;
    unsigned long starttime = millis();
    while ((millis() - starttime) < AMBIENT_TIMEOUT) {
//        ret = this->client->print(&buf[sent]);
		ret = this->print(&buf[sent], "SEND OK");
        delay(30);
        DBG(ret);DBG(" bytes sent\n\n");
        if (ret == 0) {
            ERR("send failed\n");
            return -1;
        }
        sent += ret;
        if (sent >= strlen(buf)) {
            break;
        }
    }
    delay(500);

//    while (this->client->available()) {
	while (Serial2.available()) {
//        inChar = this->client->read();
		inChar = Serial2.read();
		if(AMBIENT_DEBUG){
			Serial.write(inChar);
		}
	}

//    this->client->stop();
	this->stop();

    for (int i = 0; i < AMBIENT_NUM_PARAMS; i++) {
        this->data[i].set = false;
    }

    return (sent == 0) ? -1 : sent;
}

bool
Ambient::delete_data(const char * userKey) {
    int retry;
    for (retry = 0; retry < AMBIENT_MAX_RETRY; retry++) {
        int ret;
//        ret = this->client->connect(this->host, this->port);
		ret = this->connect(this->host, this->port);
        if (ret) {
            break ;
        }
    }
    if(retry == AMBIENT_MAX_RETRY) {
        ERR("Could not connect socket to host\r\n");
        return false;
    }

    char str[180];
    char inChar;

    memset(str, 0, sizeof(str));
    sprintf(str, "DELETE /api/v2/channels/%d/data?userKey=%s HTTP/1.1\r\n", this->channelId, userKey);
    if (this->port == 80) {
        sprintf(&str[strlen(str)], "Host: %s\r\n", this->host);
    } else {
        sprintf(&str[strlen(str)], "Host: %s:%d\r\n", this->host, this->port);
    }
    sprintf(&str[strlen(str)], "Content-Length: 0\r\n");
    sprintf(&str[strlen(str)], "Content-Type: application/json\r\n\r\n");
    DBG(str);

    int ret;
//    ret = this->client->print(str);
	ret = this->print(str, "SEND OK");
    delay(30);
    DBG(ret);DBG(" bytes sent\r\n");
    if (ret == 0) {
        ERR("send failed\r\n");
        return false;
    }

//    while (this->client->available()) {
	while (Serial2.available()) {
//        inChar = this->client->read();
		inChar = Serial2.read();
		if(AMBIENT_DEBUG){
			Serial.write(inChar);
		}
	}

//    this->client->stop();
	this->stop();

    for (int i = 0; i < AMBIENT_NUM_PARAMS; i++) {
        this->data[i].set = false;
    }

    return true;
}

bool
Ambient::connect(char* host, int port) {

	uint16_t READBUFFSIZ = 0xFF;
	char readbuffer[READBUFFSIZ];

	String cmd = "AT+CIPSTART=\"TCP\",\"";
	cmd += host;
	cmd += "\",";
	cmd += port;

	// flush read buffer of ESP8266
	while(Serial2.available()) {
		Serial2.read();
		delay(1);
	}

	Serial2.println(cmd);
	if(AMBIENT_DEBUG){
		Serial.println(cmd);
	}

	uint16_t replyidx = 0;
	// clear readbuffer
	memset(readbuffer, 0, sizeof(readbuffer));

	while (true) {
		while(Serial2.available()) {
			char c =  Serial2.read();
			if(AMBIENT_DEBUG){
				Serial.write(c);
			}
			if (c == '\r'){
				continue;
			}
			if (c == 0xA) {
				if (replyidx == 0){
					continue;  // the first 0x0A is ignored
				}
			}
			readbuffer[replyidx] = c;
			// Serial.print(c, HEX); Serial.print("#"); Serial.println(c);
			replyidx++;
		}
		if (strstr(readbuffer, "OK")){
			if(AMBIENT_DEBUG){
				Serial.println("*** Connect OK ***");
			}
			return true;
		}
		delay(500);
	}
	return false;
}

bool
Ambient::stop() {
	uint16_t READBUFFSIZ = 0xFF;
	char readbuffer[READBUFFSIZ];

	String cmd = "AT+CIPCLOSE";

	Serial2.println(cmd);
	if(AMBIENT_DEBUG){
		Serial.println(cmd);
	}

	while(!Serial2.available()){
		delay(1);
	}

	uint16_t replyidx = 0;

	// clear readbuffer
	memset(readbuffer, 0, sizeof(readbuffer));

	while (true) {
		while(Serial2.available()) {
			char c =  Serial2.read();
			if(AMBIENT_DEBUG){
				Serial.write(c);
			}
			if (c == '\r'){
				continue;
			}
			if (c == 0xA) {
				if (replyidx == 0){
					// the first 0x0A is ignored
					continue;
				}
			}
			readbuffer[replyidx] = c;
			// Serial.print(c, HEX); Serial.print("#"); Serial.println(c);
			replyidx++;
		}
//      Serial.print("<--- "); Serial.println(readbuffer);
		if (strstr(readbuffer, "OK")){
			if(AMBIENT_DEBUG){
				Serial.println("\n*** CLOSED ***");
			}
			return true;
		}
	}
	return false;
}

int
Ambient::print(char *send, char *reply){
	uint16_t READBUFFSIZ = 0xFF;
	char readbuffer[READBUFFSIZ];

	int ret;

 // flush input
  while(Serial2.available()) {
    Serial2.read();
	  delay(1);
	}

	String cmd = "AT+CIPSEND=";
	cmd += strlen(send);

	Serial2.println(cmd);
	if(AMBIENT_DEBUG){
		Serial.println(cmd);
	}

	uint16_t replyidx = 0;

	// clear readbuffer
	memset(readbuffer, 0, sizeof(readbuffer));

	while (true) {
		while(Serial2.available()) {
			char c =  Serial2.read();
			if(AMBIENT_DEBUG){
				Serial.write(c);
			}
			if (c == '\r'){
				continue;
			}
			if (c == 0xA) {
				if (replyidx == 0){
					// the first 0x0A is ignored
					continue;
				}
			}
			readbuffer[replyidx] = c;
			// Serial.print(c, HEX); Serial.print("#"); Serial.println(c);
			replyidx++;
		}
//		  Serial.print("<--- "); Serial.println(readbuffer);
		if (strstr(readbuffer, ">")){
			ret = strlen(send);
			if(AMBIENT_DEBUG){
				Serial.println("\n*** Ready to send " + String(ret) + " bytes ***");
			}
			break;
		}
	}

	Serial2.println(send);
	if(AMBIENT_DEBUG){
		Serial.println("*** send data ***");
		Serial.print(send);
		Serial.println("*****************");
	}

	while(!Serial2.available()){
		delay(1);
	}

	while (true) {
		while(Serial2.available()) {
			char c =  Serial2.read();
			if(AMBIENT_DEBUG){
				Serial.write(c);
			}
			if (c == '\r'){
				continue;
			}
			if (c == 0xA) {
				if (replyidx == 0){
					// the first 0x0A is ignored
					continue;
				}
			}
			readbuffer[replyidx] = c;
			// Serial.print(c, HEX); Serial.print("#"); Serial.println(c);
			replyidx++;
		}
//      Serial.print("<--- "); Serial.println(readbuffer);
		if (strstr(readbuffer, reply)){
			ret = strlen(send);
			if(AMBIENT_DEBUG){
				Serial.println("\n***" + String(ret) + " bytes sent ***");
			}
			break;
		}
	}

	return ret;
}
