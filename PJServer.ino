#define RH_ENABLE_EXPLICIT_RETRY_DEDUP 1

#include <string>
#include <unordered_map>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define JSON_MAX_LEN_FIELD 32
#define JSON_MAX_NESTEDOBJECT 16
//code for client / server
#define COMMAND 0
#define BYTE_COUNT 1
#define START_PAYLOAD 2

#define NOP 0
#define REGISTRATION 1
#define DATA 2
#define CLEAR_INFO 3

#define REPLY_TRUE 1
#define REPLY_FALSE 0

#define INTEGER 0x00		        //00000000
#define CUSTOM_FLOAT 0x40	      //01000000
#define STRING 0x80		          //10000000
#define SPECIFIC 0xC0 		      //11000000
#define _8BIT 0x00   		        //00000000
#define _16BIT 0x10		          //00010000
#define _32BIT 0x20		          //00100000
#define IP_ADDR 0x01		        //00000001
#define MAC_ADDR 0x02		        //00000010
#define DATE_YYYYMMDD 0x03      //00000011
#define TIME_HHMMSS 0x04	      //00000100
#define JSON_OBJECT 0x05	      //00000101
#define JSON_ARRAY 0x06         //00000110
#define JSON_LEVEL_UP 0xFF	    //11111111
//
#define GENERAL_TYPE_MASK 0xC0	//11000000
#define SPECIFIC_TYPE_MASK 0x3F	//00111111
#define NUMBER_SIZE_MASK 0x30	  //00110000
#define DECIMAL_POINT_MASK 0x0F	//00001111

#define SERVER_ADDRESS 200

#define WIFITRUES 15
#define WIFICOOLDOWN 500

//collected old data
#define DATACOL_COOLDOWN 120e3 //in second   600e3
#define DATACOL_TIMESTORE 180e3 //in second 86400e3
//gate working ping
#define GATEWORKPING_INTERVAL 300e3 //in second   60e3
//
#define WIFICONN_INTERVAL 60e3
//Json generate param
#define PARAM_SerialDevice "0"
#define PARAM_Akey "NeKKxx2"
#define PARAM_VersionDevice "TestSecond"

unsigned long int prevWiFiConnect = 0;
unsigned long int prevWatchTime = 0;
unsigned long int prevTimeGateWorking = 0;
unsigned long int packetcntr = 0;

typedef struct deviceInfo {
  std::unordered_map<uint8_t, std::string> maskKeys;
  unsigned long int lastSendTime = 0;
  uint8_t *device_buf = NULL;
  size_t device_buf_size = 0;
} deviceInfo;
std::unordered_map<uint8_t, deviceInfo> devicesInfo;

uint8_t curDeviceIdHandling = 0x00;

RH_RF95 driver(15,4); 
RHReliableDatagram manager(driver, SERVER_ADDRESS);

const char* ssid     = "rt451";
const char* password = "apsI0gqvFn";

IPAddress staticIP(10,200,1,202);
IPAddress gateway(10,200,1,200);
IPAddress mask(255,255,255,0);

//const char* ssid     = "Beeline_WIFI";
//const char* password = "a192%QUE";

uint8_t send_buf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t recv_buf[RH_RF95_MAX_MESSAGE_LEN];

String curDeviceJson = ""; 

void setup()  {
  Serial.begin(9600);
  while (!Serial) ;
  Serial.println();
  if (!manager.init())
    Serial.println("init failed");
  else
    Serial.println("init");
  //
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  //lora
  driver.setTxPower(20, false);
  driver.setFrequency(869.2);
  driver.setCodingRate4(5);
  driver.setSignalBandwidth(125000);
  driver.setSpreadingFactor(9); 

  driver.setModeRx();

  connect_wifi();
}

void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //for static ip
  //if (!WiFi.config(staticIP, gateway, mask))
  //  Serial.println("Wifi Failed to configure");
  
  Serial.print(F("\nConnecting to WiFi "));
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFICOOLDOWN);
    Serial.print(F("."));
    if(i > WIFITRUES)
      break;
    i++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print(F("Connected. IP address: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("SSID: "));
    Serial.println(WiFi.SSID());
  }
  else Serial.println("WIFI not connected");
}

void loop() {
  if (manager.available()) {
    digitalWrite(LED_BUILTIN, LOW);
    //
    uint8_t len = sizeof(recv_buf); 
    uint8_t from;
    if (manager.recvfromAck(recv_buf, &len, &from)) {
      curDeviceIdHandling = from;
      Serial.print("Получен запрос от 0x" + String(from, HEX) + " [" + String(driver.lastRssi()) + "]: ");
      printBuf(recv_buf);
      process_package();
      if (!manager.sendtoWait(send_buf, send_buf[BYTE_COUNT], from))
        Serial.println("sendtoWait failed"); 
      if(curDeviceJson != "") {
        //send_to_server(curDeviceJson, "http://robo.itkm.ru/core/jsonapp.php");
        send_to_server(curDeviceJson, "http://robo.itkm.ru/core/jsonapp.php");
        send_to_server(curDeviceJson, "http://46.254.216.214/core/jsonapp.php");
        send_to_server(curDeviceJson, "http://dbrobo1.mf.bmstu.ru/core/jsonapp.php");
      }
      packetcntr++;
    }
    //clear info
    curDeviceJson = "";
    curDeviceIdHandling = 0x00;
    memset(send_buf, 0, sizeof(send_buf));
    //
    digitalWrite(LED_BUILTIN, HIGH);
  }
  //garbage collected every * sec
  unsigned long elapsedTime = millis() - prevWatchTime;
  if(elapsedTime > DATACOL_COOLDOWN) {
    deviceGarbageCollect();
    prevWatchTime = millis();
    printInfoDevices();
  }
  //gate working ping
  unsigned long elapsedTimeGateWorking = millis() - prevTimeGateWorking;
  if(prevTimeGateWorking == 0 || elapsedTimeGateWorking > GATEWORKPING_INTERVAL) {
    gateWorkingPing();
    prevTimeGateWorking = millis();
  }
  //wifi reconnect
  if(WiFi.status() != WL_CONNECTED) {
    unsigned long elapsedTimeWiFiConnect = millis() - prevWiFiConnect;
    if(elapsedTimeWiFiConnect > WIFICONN_INTERVAL) {
      Serial.println(WiFi.status());
      prevWiFiConnect = millis();
      connect_wifi();
    }
  }
}

void gateWorkingPing() {
  Serial.println("\n---Sending info about gate---");
  String json;
  json = "{\"system\":{ ";
  json += "\"Akey\":\"" + String(PARAM_Akey) + "\",";
  json += "\"Serial\":\"" + String(PARAM_SerialDevice) + "\", ";
  //json += "\"Version\":\"" + String(PARAM_VersionDevice) + "\"}";
  json += "\"packetTotal\":\"" + String(packetcntr) + "\", ";
  json += "\"Version\":\"" + String(PARAM_VersionDevice) + "\"}";
  json += "}";
  Serial.println(json);
  send_to_server(json, "http://robo.itkm.ru/core/jsonapp.php");
  send_to_server(json, "http://46.254.216.214/core/jsonapp.php");
  send_to_server(json, "http://dbrobo1.mf.bmstu.ru/core/jsonapp.php");
  Serial.println("\n---Sending finished---\n");
}

void process_package() {
  if(curDeviceIdHandling == 0x00)
    return;
  send_buf[COMMAND] = recv_buf[COMMAND];
  send_buf[BYTE_COUNT] = START_PAYLOAD;

  //if(devicesInfo.find(curDeviceIdHandling) == devicesInfo.end())
  //  devicesInfo[curDeviceIdHandling] = NULL;
    
  deviceInfo &curDeviceInfo = devicesInfo[curDeviceIdHandling];

	switch (recv_buf[COMMAND]) {
		case REGISTRATION:
			registationKeys();
      if(curDeviceInfo.device_buf != NULL) {
        memcpy(&recv_buf[START_PAYLOAD - 1], curDeviceInfo.device_buf, sizeof(recv_buf) - 1);
        curDeviceJson = decodeJsonFromBytes();
        Serial.println(curDeviceJson);

        free(curDeviceInfo.device_buf);
        curDeviceInfo.device_buf = NULL;
        curDeviceInfo.device_buf_size = 0;
      }
			break;
		case DATA:
      curDeviceInfo.device_buf_size = 0;

      if(devicesInfo.find(curDeviceIdHandling) != devicesInfo.end() &&
          curDeviceInfo.maskKeys.size() > 0) {
			  curDeviceJson = decodeJsonFromBytes();
        send_buf[send_buf[BYTE_COUNT]++] = REPLY_TRUE;
        Serial.println(curDeviceJson);
      }
      else {
        if(curDeviceInfo.device_buf != NULL) {
          free(curDeviceInfo.device_buf);
          curDeviceInfo.device_buf = NULL;
        }
        curDeviceInfo.device_buf_size = recv_buf[BYTE_COUNT] - 1;
        curDeviceInfo.device_buf = (uint8_t*)calloc(curDeviceInfo.device_buf_size, sizeof(uint8_t));
        if(curDeviceInfo.device_buf != NULL) {
          memcpy(curDeviceInfo.device_buf, &recv_buf[COMMAND + 1], curDeviceInfo.device_buf_size);
          Serial.println("Данные сохранены в ожидании ключей");
        }
        send_buf[send_buf[BYTE_COUNT]++] = REPLY_FALSE;
      }
			break;
		case CLEAR_INFO:
      deleteInfo();
      send_buf[send_buf[BYTE_COUNT]++] = REPLY_TRUE;
			break;
	}
  if(devicesInfo.find(curDeviceIdHandling) == devicesInfo.end())
    return;
  updateTimeDevice();
}

void printInfoDevices() {
  Serial.println("\n***********DEBUG INFO***********");
  Serial.println("Информация о девайсах");
  for (auto it : devicesInfo) {
    Serial.println("ID device: " + String(it.first));
    unsigned long elapsedTime = millis() - it.second.lastSendTime;
    Serial.println("Время последнего сообщения: " + String(it.second.lastSendTime) + "ms");
    Serial.println("Время с последнего сообщения: " + String(elapsedTime) + "ms");
    Serial.println("Ключи: ");
    for (auto x : it.second.maskKeys)
      Serial.println(String(x.first) + "\t" + String(x.second.c_str()));
    Serial.println();
  }
  Serial.println("***********END DEBUG INFO***********\n");
}

void updateTimeDevice() {
  if(devicesInfo.find(curDeviceIdHandling) == devicesInfo.end())
    return;
  devicesInfo[curDeviceIdHandling].lastSendTime = millis();
}

void send_to_server(String data, String server_addr) {
  WiFiClient client;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, server_addr);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(data);
    Serial.print(F("HTTP response code: "));
    Serial.println(httpResponseCode);
    http.end();
  }
  else {
    Serial.println(F("WiFi disconnected."));
  }
}

void printBuf(uint8_t *buf) {
  for(int i = 0; i < buf[BYTE_COUNT]; i++) {
    Serial.print(buf[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void registationKeys() {
  if(devicesInfo.find(curDeviceIdHandling) != devicesInfo.end())
    devicesInfo[curDeviceIdHandling].maskKeys.clear();
  char *pch = strtok((char *)recv_buf, " ") + START_PAYLOAD;
  for (uint8_t i = 0; pch != NULL; i++) {
    devicesInfo[curDeviceIdHandling].maskKeys.insert(std::make_pair(i, pch));
    pch = strtok(NULL, " ");
  }
  for (auto x : devicesInfo[curDeviceIdHandling].maskKeys)
    Serial.println(String(x.first) + "\t" + String(x.second.c_str()));
}

void deleteInfo() {
  if(devicesInfo.find(curDeviceIdHandling) == devicesInfo.end())
    return;
  devicesInfo.erase(devicesInfo.find(curDeviceIdHandling));
}

void deviceGarbageCollect() {
  auto it = devicesInfo.begin();
  while (it != devicesInfo.end()) {
    uint8_t nowId = it->first;
    unsigned long elapsedTimeDataCol =  millis() - it->second.lastSendTime;
    it++;
    if(elapsedTimeDataCol > DATACOL_TIMESTORE) {
      free(devicesInfo[nowId].device_buf);
      devicesInfo[nowId].device_buf = NULL;
      devicesInfo.erase(nowId);
      Serial.println("info about 0x" + String(nowId) + " deleted");
    }
  }
  Serial.println("Data collected");
}


String getKeyFromId(uint8_t idKey) {
  auto res = devicesInfo[curDeviceIdHandling].maskKeys.find(idKey);
  if (res != devicesInfo[curDeviceIdHandling].maskKeys.end())
    return String(res->second.c_str());
  return "NotRegistredKey";
}

String decodeJsonFromBytes() {
  uint8_t nestedObject[JSON_MAX_NESTEDOBJECT], nesObjCount = 0;
  String result = "{";
  for (int i = START_PAYLOAD; i < recv_buf[BYTE_COUNT];) {
    uint8_t key_id = recv_buf[i++];
    if (key_id == JSON_LEVEL_UP) {
      if(nesObjCount == 0) {
        Serial.print("Error with nested object!");
        break;
      }
      nesObjCount--;
      if(nestedObject[nesObjCount] == JSON_OBJECT)
        result += '}';
      else if(nestedObject[nesObjCount] == JSON_ARRAY)
        result += ']';
      if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
        result += ", ";
      continue;
    }
    uint8_t tag;
    if(nesObjCount > 0 && nestedObject[nesObjCount - 1] == JSON_ARRAY)
      tag = key_id;
    else {
      result += '\"' + getKeyFromId(key_id) + '\"' + ": ";
      tag = recv_buf[i++];
    }
    switch (tag & GENERAL_TYPE_MASK) {
      case SPECIFIC: {
        switch (tag & SPECIFIC_TYPE_MASK) {
          case JSON_OBJECT: {
            result += '{';
            if(nesObjCount == JSON_MAX_NESTEDOBJECT)
              Serial.println("Error. Max nested object reach");
            nestedObject[nesObjCount] = JSON_OBJECT;
            nesObjCount++;
            continue;
          }
          case JSON_ARRAY: {
            result += '[';
            if(nesObjCount == JSON_MAX_NESTEDOBJECT)
              Serial.println("Error. Max nested object reach");
            nestedObject[nesObjCount] = JSON_ARRAY;
            nesObjCount++;
            continue;
          }
          case IP_ADDR: {
            uint8_t ip[4];
            char ip_str[16];
            for(int j = 0; j < 4; j++)
              ip[j] = recv_buf[i++];
            sprintf(ip_str, "%hhu.%hhu.%hhu.%hhu", ip[0], ip[1], ip[2], ip[3]);
            result += '\"' + String(ip_str) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case MAC_ADDR: {
            uint8_t mac[6];
            char mac_str[18];
            for(int j = 0; j < 6; j++)
              mac[j] = recv_buf[i++];
            sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            result += '\"' + String(mac_str) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case DATE_YYYYMMDD: {
            uint16_t year = 0;
            year |= recv_buf[i++];
            year |= recv_buf[i++] << 8;
            uint8_t mon = recv_buf[i++];
            uint8_t day = recv_buf[i++];
            char date[11];
            sprintf(date, "%d-%02hhu-%02hhu", year, mon, day);
            result += '\"' + String(date) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case TIME_HHMMSS: {
            uint8_t hour = recv_buf[i++];
            uint8_t min = recv_buf[i++];
            uint8_t sec = recv_buf[i++];
            char time[11];
            sprintf(time, "%02hhu:%02hhu:%02hhu", hour, min, sec);

            result += '\"' + String(time) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
        }
      }
      case STRING: {
        char unpack_buf_cstr[JSON_MAX_LEN_FIELD];
        strcpy(unpack_buf_cstr, (char*)&recv_buf[i]);
        i += strlen((char*)&recv_buf[i]) + 1;
        result += '\"' + String(unpack_buf_cstr) + '\"';
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case INTEGER: {
        switch (tag & NUMBER_SIZE_MASK) {
          case _8BIT: {
            int8_t num = (int8_t)recv_buf[i++];
            result += String(num);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _16BIT: {
            int16_t num = 0;
            num |= recv_buf[i++];
            num |= recv_buf[i++] << 8;
            result += String(num);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _32BIT: {
            int32_t num = 0;
            for(int j = 0; j < 32; j += 8)
              num |= recv_buf[i++] << j;
            result += String(num);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
        }
      }
      case CUSTOM_FLOAT: {
        switch (tag & NUMBER_SIZE_MASK) {
          case _8BIT: {
            int8_t num = (int8_t)recv_buf[i++];
            uint8_t dec_places = tag & DECIMAL_POINT_MASK;
            int8_t integer = num / quick_pow10(dec_places);
            uint8_t fraction = abs(num) % quick_pow10(dec_places);
            char num_str[18];
            sprintf(num_str, "%d.%0*d", integer, dec_places, fraction);
            if (integer == 0 && num < 0)
              result += '-';
            result += String(num_str);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _16BIT: {
            int16_t num = 0;
            num |= recv_buf[i++];
            num |= recv_buf[i++] << 8;
            uint8_t dec_places = tag & DECIMAL_POINT_MASK;
            int16_t integer = num / quick_pow10(dec_places);
            uint16_t fraction = abs(num) % quick_pow10(dec_places);
            char num_str[18];
            sprintf(num_str, "%d.%0*d", integer, dec_places, fraction);
            if (integer == 0 && num < 0)
              result += '-';
            result += String(num_str);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _32BIT: {
            int32_t num = 0;
            for(int j = 0; j < 32; j += 8)
              num |= recv_buf[i++] << j;
            uint8_t dec_places = tag & DECIMAL_POINT_MASK;
            int32_t integer = num / quick_pow10(dec_places);
            uint32_t fraction = abs(num) % quick_pow10(dec_places);
            char num_str[18];
            sprintf(num_str, "%ld.%0*ld", integer, dec_places, fraction);
            if (integer == 0 && num < 0)
              result += '-';
            result += String(num_str);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
        }
      }
    }
  }
  result += '}';
  return result;
}

int64_t quick_pow10(int n) {
	static int64_t pow10[16] = {
		1, 10, 100, 1000,
		10000, 100000, 1000000, 10000000,
		100000000, 1000000000, 10000000000, 100000000000,
		1000000000000, 10000000000000, 100000000000000, 1000000000000000
	};
	return pow10[n];
}
