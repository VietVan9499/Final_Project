/**************************** Library *********************************/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>         //https://github.com/knolleary/pubsubclient
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <Wire.h>
#include <string.h>
/******************* HARDWARE PIN DEFINITIONS (Set to match your hardware)***********************/

/************* Flags and Counts***********************/
int checkOpen = 0;
int checkNguoiLa = 0;
int checkKhongCoNguoi = 0;
int checkNongLanhOnOff = 0;
int checkNongLanh = 0;
int count = 0;     //count the number of button presses to reset
int flag = 0;      //check MQTT connection status (When the broker fails, the application will constantly reconnect, resulting in hardware failure.)
int countR = 0;   //count reconnect
int flagMQTT = 0; //It reconnects repeatedly after 3 minutes of boot with no WiFi or MQTT connection.
int buff = 0;          //Biến dùng để lưu dữ liệu từ Raspberry Pi
/************* SENSOR***********************/

static uint32_t  currentmls = millis(), intervalPM = currentmls, interval_MQTT = intervalPM, intervalGas = interval_MQTT , timeConnectMqtt = 180000;

/************* MQTT TOPICS (change these topics as you wish)  **************************/
///COMMAND
#define command_topic_open   "DoAn/LivingRoom/Servo"
#define command_topic_NongLanh  "DoAn/Three/MayNongLanh"

const char* open_door = "OPEN";
const char* close_door = "CLOSE";
const char* turn_on = "ON";
const char* turn_off = "OFF";

/********************************** START SETUP*****************************************/
WiFiClient espClient2;
PubSubClient client(espClient2);
long lastMsg = 0;
char msg[50];
int value = 0;
//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "vietnguyen.duckdns.org";
char mqtt_port[6] = "8889";
char username[34] = "vietnguyen";
char password[34] = "vanviet";
//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/********************************** START MQTT CALLBACK*****************************************/

void callback(char* topic, byte* payload, unsigned int length) {
  char* payload_str;
  payload_str = (char*) malloc(length + 1);
  memcpy(payload_str, payload, length);
  payload_str[length] = '\0';
  Serial.println(String(payload_str));
  if ( String(topic) == command_topic_open) {
    if (String(payload_str) == close_door)
    {
      checkOpen = 1;
    }
    else if ( String(payload_str) == open_door ) {
      checkOpen = 0;
    }
  }
  if ( String(topic) == command_topic_NongLanh ) {
    if (String(payload_str) == turn_on)
    {
      checkNongLanhOnOff = 0;
      checkNongLanh = 0;
    }
    else if ( String(payload_str) == turn_off ) {
      checkNongLanh = 1;
    }
  }
  else {
    Serial.print("I do not know what to do with ");
    Serial.print(String(payload_str));
    Serial.print(" on topic ");
    Serial.println( String(topic));
  }
}
/********************************** START SETUP*****************************************/
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  //SPIFFS.format();
  //read configuration from FS json
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(username, json["username"]);
          strcpy(password, json["password"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_username("username", "username", username, 32);
  WiFiManagerParameter custom_password("password", "password", password, 32);
  WiFiManager wifiManager;

  //clean FS, for testing

  //  wifiManager.resetSettings();


  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_username);
  wifiManager.addParameter(&custom_password);

  wifiManager.setTimeout(120);
  if (!wifiManager.autoConnect("NHAN DANG")) {
    Serial.println("failed to connect and hit timeout");
    flag = 1;
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(username, custom_username.getValue());
  strcpy(password, custom_password.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["username"] = username;
    json["password"] = password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  // mqtt
  client.setServer(mqtt_server, atoi(mqtt_port)); // parseInt to the port
  client.setCallback(callback);

}

/********************************** START RECONNECT*****************************************/
void reconnect() {

  //attempt to connect to the wifi if connection is lost
  if (WiFi.status() != WL_CONNECTED) {
    //loop while we wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }

  }
  if (WiFi.status() == WL_CONNECTED) {
    // Loop until we're reconnected to the MQTT server
    while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect("NhanDang", username, password)) { // username as client ID
        Serial.println("connected");
        client.subscribe(command_topic_open );
        client.subscribe(command_topic_NongLanh );
      }
      else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 2 seconds");
        // Wait 5 seconds before retrying
        countR++;
        delay(2000);
        if (countR == 10) {
          flag = 1;
          flagMQTT = 0;
          Serial.println("Flag=");
          Serial.print(flag);
          Serial.println("CountR=");
          Serial.print(countR++);
          break;
        }
      }
    }
  }
}
void Nguoi() {
  if (Serial.available()) //Nếu có tín hiệu từ Pi
  {
    //buff = Serial.readStringUntil('\r'); //Đọc vào đến khi gặp \r (xuống dòng)
    buff = Serial.read();
    if (buff == '1' && checkOpen == 0)           //Nếu dữ liệu = "Led On"
    {
      client.publish("DoAn/LivingRoom/Servo", "CLOSE" );
      Serial.println("mo cua");  //Trả ngược về "Turned On"
      checkOpen = 1;
    }
    if (buff == '0' && checkKhongCoNguoi == 0) //Nếu dữ liệu = "Led Off"
    {
      Serial.println("Khong Co nguoi"); //Trả ngược về "Turned Off"
      client.publish("DoAn/NhanDang/Door", "KhongCoNguoi");
      checkKhongCoNguoi = 1;
      checkNguoiLa = 0;

    }
    if (buff == '2' && checkNguoiLa == 0) //Nếu dữ liệu = "Led Off"
    {
      Serial.println("nguoi la"); //Trả ngược về "Turned Off"
      client.publish("DoAn/NhanDang/Door", "NguoiLa");
      checkNguoiLa = 1;
      checkKhongCoNguoi = 0;
      delay(1000);
      client.publish("DoAn/NhanDang/Door", "KhongCoNguoi");
    }
  }
}

/********************************** START MAIN LOOP***************************************/
void loop() {
  Nguoi();
  if (flag == 0)
  {
    if ( !client.connected()) { //NẾU HÀM BOOLE NGHĨA LÀ NẾU NGƯỜI LẠI CỦA CONECT THÌ RECONECT
      reconnect();
    }
    client.loop();
    countR = 0;
  }

  if ( ( millis() - interval_MQTT > timeConnectMqtt ) && ( flagMQTT == 0)) {
    interval_MQTT = millis();
    if (interval_MQTT > timeConnectMqtt)
    {
      timeConnectMqtt = 120000;
    }
    if (flag == 1) {
      flagMQTT = 1;
      countR = 0;
      Serial.println("MQTT RECONECT:");
      Serial.println(flagMQTT);
      Serial.println("CountR:");
      Serial.println(countR);
      flag = 0;
    } else {
      flagMQTT = 1;
    }
  }

}
