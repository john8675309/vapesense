#include <sps30.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoUniqueID.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <IPAddress.h>
#include <avr/wdt.h>
// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
char server[] = "domain.com";
int port=80;
EthernetClient client;
HttpClient httpClient = HttpClient(client, server, port);
int alerting = 0;
int readings = 0;
float avg = 0;
float sum = 0;
int watch = 0;
int rounds = 0;
long rearm = 90000;
int checkRounds = 5;
String uid;
String ipmode;
int changeip=0;
IPAddress ip;
IPAddress dns;
IPAddress gateway;
IPAddress subnet;
DynamicJsonDocument doc(100);
int ip1;
int ip2;
int ip3;
int ip4;
int s1;
int s2;
int s3;
int s4; 
int g1;
int g2;
int g3;
int g4;
int d1;
int d2;
int d3;
int d4;
int alertLevel = 9999;
float low = 99999;
float high = 0;
String extra;
int start;
void setup() {
  Serial.begin(9600);
  Serial.println("Starting UP");
  Serial.println("Welcome To ANP Vape Sense Copyright Advanced Network Professionals By John Hass john@getanp.com World Headquarters Spencer Iowa 712-584-2024");
  int eepromAddress = 0;
  eepromAddress=0;
  int addy = 0;
  for (uint16_t cnt = 0; cnt < 4; cnt++) {
    // retrieve
    uint32_t ipRaw = EEPROM.get(eepromAddress, ipRaw);
    IPAddress ipRetrieved(ipRaw);
    if (addy == 0) {
      ip = ipRetrieved;
    }
    if (addy == 1) {
      dns = ipRetrieved;
    }
    if (addy == 2) {
      gateway = ipRetrieved;
    }
    if (addy == 3) {
      subnet = ipRetrieved;
    }
    eepromAddress+=sizeof(uint32_t);
    addy++;
  }
  if (ip[0] == 0) {
    ipmode="dhcp";
  } else {
    ipmode="static";
  }
 
  for (size_t i = 0; i < UniqueIDsize; i++) {
		if (UniqueID[i] < 0x10)
			uid.concat("0");
		uid.concat(UniqueID[i]);
	}
  byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0xF8, 0x4A };
  int16_t ret;
  uint8_t auto_clean_days = 4;
  uint32_t auto_clean;
  Ethernet.init(10);
  delay(1000);
  if (ipmode == "dhcp") {
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Error DHCP!!!");
    } else {
      //Serial.print("DHCP assigned IP ");
      //Serial.println(Ethernet.localIP());
    }
  }
  if (ipmode == "static") {
    Ethernet.begin(mac,ip,dns,gateway,subnet);
  }
 
  delay(5000);
  alerting = 0;
  String postData = "c=vapeAlert&a=0&m=";
  postData.concat(uid);
  String contentType = "application/x-www-form-urlencoded";

  httpClient.post("/api.php", contentType, postData);
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();
  //Serial.println(response);
  sensirion_i2c_init();
  while (sps30_probe() != 0) {
    delay(500);
  }
  ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
  ret = sps30_start_measurement();
  delay(1000);
  Serial.print("Ready ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  struct sps30_measurement m;
  char serial[SPS30_MAX_SERIAL_LEN];
  uint16_t data_ready;
  int16_t ret;
  do {
    ret = sps30_read_data_ready(&data_ready);
    if (ret < 0) {
      //Serial.print("error reading data-ready flag: ");
      //Serial.println(ret);
    } else if (!data_ready) {
      //Serial.print("data not ready, no new measurement available\n");
    } else {
      break;
    }
    delay(100); /* retry in 100ms */
  } while (1);

  ret = sps30_read_measurement(&m);
  if (ret < 0) {
    //Serial.print("error reading measurement\n");
  } else {
    if (watch == 1) {
        sum = sum + m.mc_2p5;
      if (m.mc_2p5 < low) {
        low = m.mc_2p5;
      }
      if (m.mc_2p5 > high) {
        high = m.mc_2p5;
      }
      readings = readings + 1;
      avg = sum/readings;
    }
    if (watch != 1) { 
      if (m.mc_2p5 > alertLevel) {
        if (alerting == 0) {
          if (start == 0) {
            start = millis();
            setAlarm(1);
          }
          if (millis() < start+rearm) {                    
          } else {
            start = millis();
            setAlarm(1);
          }
        }
      } else {      
        if (alerting == 1) {
          setAlarm(0);   
        }
      }
    }
  }
  rounds = rounds + 1;
  if (rounds == checkRounds) {
    rounds = 0;
    String command;
    if (changeip == 1) {
      if (ip1 ==0) {
        command="ip";
      } else if (s1 == 0) {
        command="subnet";
      } else if (g1 == 0) {
        command="gateway";
      } else if (d1 == 0) {
        command="dns";
      } else {
        for (int i = 0 ; i < EEPROM.length() ; i++) {
          EEPROM.write(i, 0);
        }
        int eepromAddress = 0;
        IPAddress hosts[] = { IPAddress(ip1, ip2, ip3, ip4), IPAddress(d1,d2,d3,d4), IPAddress(g1,g2,g3,g4), IPAddress(s1, s2, s3, s4)};
        const int hostsCount = sizeof(hosts) / sizeof(hosts[0]);
        for (uint16_t cnt = 0; cnt < hostsCount; cnt++) {
          EEPROM.put(eepromAddress, uint32_t(hosts[cnt]));
          eepromAddress += sizeof(uint32_t);
        }
        reboot();
      }
    } else {
      command="update";
    }
    String postData="c=";
    postData.concat(command);
    postData.concat("&m=");
    postData.concat(uid);
    if (command == "update" && extra != "") {
      postData.concat(extra);
    }         
    String contentType = "application/x-www-form-urlencoded";
    httpClient.post("/api.php", contentType, postData);
    // read the status code and body of the response
    int statusCode = httpClient.responseStatusCode();
    String response = httpClient.responseBody();
    if (extra != "") {
      extra = "";
    }
    if (statusCode == 200) {
      if (response != "") {
        DeserializationError error = deserializeJson(doc, response);
        if (error) {
          Serial.println("JE");
        } else {
          char* ripMode = doc["ipmode"];
          int w = doc["w"];
          float al = doc["al"];
          long re = doc["re"];
          if (al > 0) {
            alertLevel = al;
          } 
          if (re > 0) {
            rearm = re;
          }         
          if (w == 0) {
            if (watch == 1) {
              extra.concat("&v=");
              extra.concat(avg);
              extra.concat("&h=");
              extra.concat(high); 
              extra.concat("&l=");
              extra.concat(low);               
            }
            watch = 0;
          } else if (w == 1) {
            watch = 1;
            high = 0;
            low=999;
            sum = 0;
            readings = 0;
          }
          if (String(ripMode) != ipmode) {
            changeip=1;
            if (String(ripMode) == "dhcp") {
              for (int i = 0 ; i < EEPROM.length() ; i++) {
                EEPROM.write(i, 0);
              }
              reboot();              
            }            
            if (ip1 == 0) {
              ip1 = doc["ip1"];
              ip2 = doc["ip2"];
              ip3 = doc["ip3"];
              ip4 = doc["ip4"];
            } else if (s1 == 0 ) {
              s1 = doc["s1"];
              s2 = doc["s2"];
              s3 = doc["s3"];
              s4 = doc["s4"];
            } else if (g1 == 0) {
              g1 = doc["g1"];
              g2 = doc["g2"];
              g3 = doc["g3"];
              g4 = doc["g4"];
            } else if (d1 == 0) {
              d1 = doc["d1"];
              d2 = doc["d2"];
              d3 = doc["d3"];
              d4 = doc["d4"];
            }
          }
        }
      }                      
    }
  }
  delay(1000);
}

void setAlarm(int set) {
  alerting = set;
  String postData = "c=vapeAlert&m=";
  postData.concat(uid);
  postData.concat("&a=");
  postData.concat(set);
  String contentType = "application/x-www-form-urlencoded";

  httpClient.post("/api.php", contentType, postData);
  // read the status code and body of the response
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();
}


int writeStringToEEPROM(int addrOffset, const String &strToWrite) {
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  return addrOffset + 1 + len;
}
int readStringFromEEPROM(int addrOffset, String *strToRead) {
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++){
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  *strToRead = String(data);
  return addrOffset + 1 + newStrLen;
}

void reboot() {
 wdt_disable();
 wdt_enable(WDTO_15MS);
 while (1) {}
}
