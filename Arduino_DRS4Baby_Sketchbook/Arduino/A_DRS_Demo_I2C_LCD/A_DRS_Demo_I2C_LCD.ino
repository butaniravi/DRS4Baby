// ********************************************************************************
// DRS4Baby ESP12E Firmware
// BY Ravi Butani
// Released Under Creative Commons BY-SA License
// Project Docs hosted at https://www.hackster.io/ravi-butani/drs-for-sanitization-needs-of-baby-25b391?ref=challenge&ref_id=78&offset=11
// Contact E-mail ravi.butani@marwadieducation.edu.in 
//*********************************************************************************

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for wifi manager library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

//needed for Json Response parse
#include <ArduinoJson.h>

//needed for eeprom access
#include <EEPROM.h>
#include <Arduino.h>
#include "EEPROMAnything.h"

//needed for Grove 16x2 I2C LCD
#include <Wire.h>
#include "rgb_lcd.h"

// Objects Init... 
rgb_lcd lcd;
WiFiClientSecure client;

//GPIO Definations
#define ST_LED    D2 //Normally off LED ON-->WiFi Connected Blink at every 5 Sec-->WiFi Not Connected
#define NDEF_PIN  D8 //Active LOW--3 Reserved for future use
#define AP_PIN    D0 //Active LOW--2 ESP8266 as Access Point
#define RST_PIN   D1 //Active LOW--1 Reset counts in any Slot
#define RE1_PIN   D5 //Active LOW Reed switch for Count Diapers
#define RE2_PIN   D6 //Active LOW Reed switch for Count Wipes
#define RE3_PIN   D7 //Active LOW Reed switch for Count Soap

#define MAX_RE1   20 // Maximum quantity of Diapers
#define MAX_RE2   20 // Maximum quantity of Wipes
#define MAX_RE3   20 // Maximum quantity of Soap

#define TH_RE1   5 // Threshold after which replenishment event of Diaper start
#define TH_RE2   5 // Threshold after which replenishment event of Wipes start
#define TH_RE3   5 // Threshold after which replenishment event of oap start

volatile int re1_cnt = MAX_RE1;
volatile int re2_cnt = MAX_RE2;
volatile int re3_cnt = MAX_RE3;

int re1_rep_flag = 0;
int re2_rep_flag = 0;
int re3_rep_flag = 0;
int rst_flag = 0;
int rst_slot = 0;

//AMAZON DRS parameters
const char* device_model = "041557cb-efe4-4233-1143-93ghb6077b01"; //Product model ID of your device 
const char* device_identifier = "ABCDE12345"; // Serial No of device any thing you wish at production end
const char* client_id = "amzn1.application-oa2-client.ec9o65392ace4941a345b06e68e28e33"; // your Amazon developer account client ID
const char* client_secret = "6430ef4a824b4ee04ce69c54266dd93bb51200b08da0b03126be2f9d7857a027"; // Your Amazon developer account secret code 
const char* slot_id1_diaper = "ab754ae9-f794-4060-8aad-a8694bb87432"; //Sloat ID for Diapers
const char* slot_id2_wipes = "f22b00be-a76d-4448-99f2-304283ce1c77";  //Sloat ID for Wipes
const char* slot_id3_soap = "09403b3c-f79b-42d5-aa80-ba06f984de53";   //Sloat ID for Soap
const char* redirect_uri = "https%3A%2F%2Fdrs4baby.blogspot.in%2F2016%2F11%2Fregistration.html"; //Encoded Return URL
                            //should be encoded and same as used at time of device creation
const char* con_length = "286"; // its Packet size for post request please count your encoded redirect_uri charecters and replace this string with these number as per (286 -66 +length of your redirect URI)---yes its ugly,, but I havent updated it so far 

int startup = 1,con_flag=0;


void setup() {
  int cnt;
  Serial.begin(115200); //  Initlize UART for Serial debug
  EEPROM.begin(2048);   // Initlize EEPROM with 2K Bytes
  
  pinMode(ST_LED,   OUTPUT); //
  pinMode(AP_PIN,   INPUT_PULLUP);
  pinMode(RST_PIN,  INPUT_PULLUP);
  pinMode(RE1_PIN,  INPUT_PULLUP);
  pinMode(RE2_PIN,  INPUT_PULLUP);
  pinMode(RE3_PIN,  INPUT_PULLUP);
  
  digitalWrite(ST_LED,LOW);
  Serial.println("\n DRS4BABY- Poweredby Amazon DRS");
   
  // set up the I2C port for ESP8266 and LCD's number of columns and rows:
  Wire.begin(D4,D3); //(SDA,SCL)
  lcd.begin(16, 2);

  lcd.setRGB(100,100,100); // White
   
  delay(100);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("--- DRS4Baby ---");
  lcd.setCursor(0, 1);
  lcd.print("By - Ravi Butani");
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Powered By");
  lcd.setCursor(0, 1);
  lcd.print("---> AMAZON DRS");
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("For Config NW ");
  lcd.setCursor(0, 1);
  lcd.print("   Press CONFIG");
  for(cnt=0; cnt<=500; cnt++)
  {
    if ( digitalRead(AP_PIN) == LOW ) 
    {
      lcd.setRGB(0,0,100); // Blue
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SSID: DRS4BABY");
      lcd.setCursor(0, 1);
      lcd.print("URL: 192.168.4.1");
      config_ap_auth();
    }
    delay(20);
    lcd.setCursor(0, 1);
    lcd.print(20-(millis()/1000));
    lcd.print(" ");
  }
  //Serial.println("refcode commited to eeprom");
  attachInterrupt(RST_PIN,rst_isr,FALLING);
  attachInterrupt(RE1_PIN,re1_isr,RISING);
  attachInterrupt(RE2_PIN,re2_isr,RISING);
  attachInterrupt(RE3_PIN,re3_isr,RISING);
}

void loop()
{
  if(is_wifi_connected() == 1)
  {
    if(startup==1)
    {
      lcd.setRGB(100,0,100); // RB
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Connecting.....");
      lcd.setCursor(0, 1);
      lcd.print("to Access Point");
      Serial.println("connecting to AP.....");
      delay(2000);
      unsigned int x = obtain_access_and_refresh_token();
      Serial.println(x);
      lcd.setRGB(100,100,100); // White
      digitalWrite(ST_LED,HIGH);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Connected to AP.");
      lcd.setCursor(0, 1);
      lcd.print("DRS Enabled...");
      Serial.println("Connected to AP......and DRS Enabled...");
      delay(4000);
      startup++;
    }

    if(rst_flag == 0)
    {
      lcd.setRGB(100,100,100); // White
      digitalWrite(ST_LED,HIGH);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Diper Wipe Soap");
      lcd.setCursor(0, 1);
      lcd.print(re1_cnt);
      lcd.print("     ");
      lcd.print(re2_cnt);
      lcd.print("    ");
      lcd.print(re3_cnt);
      delay(200);
    }
    
    else if(rst_flag == 1)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("RESET : Slot...?");
      lcd.setCursor(0, 1);
      if(rst_slot == 1)lcd.print("Diapers");
      else if(rst_slot == 2)lcd.print("Wipes");
      else if(rst_slot == 3)lcd.print("Soap");
      delay(100);
    }
    else if(rst_flag == 2)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Success Reset");
      lcd.setCursor(0, 1);
      if(rst_slot == 1)lcd.print("Diapers");
      else if(rst_slot == 2)lcd.print("Wipes");
      else if(rst_slot == 3)lcd.print("Soap");
      rst_flag = 0;
      delay(3000);
    }
    
    if(re1_rep_flag == 1)
      {
        con_flag=1;
        lcd.setRGB(0,100,0); // Green
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Replenishing..");
        lcd.setCursor(0, 1);
        lcd.print("Diapers");
        Serial.println(".............REPLENISHING DIAPERS..............");
        unsigned int y = refresh_access_token();
        Serial.println(y);
        delay(500);
        unsigned int z=end_point_request_replenishment(slot_id1_diaper);
        Serial.println(z);
        if(z==11)
        {
          lcd.setRGB(100,100,100); // white
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Order Placed..");
          lcd.setCursor(0, 1);
          lcd.print("Diapers");
          re1_rep_flag = 0;
        }
        else if(z==13)
        {
          lcd.setRGB(100,100,100); // white
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("In Progress..");
          lcd.setCursor(0, 1);
          lcd.print("Diapers");
          re1_rep_flag = 0;
        }
        else
        {
          lcd.setRGB(200,100,100); // wwrb
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Order Failed..");
          lcd.setCursor(0, 1);
          lcd.print("Diapers");
        }
        delay(5000);
      }
      
    else if(re2_rep_flag == 1)
      {
        con_flag=1;
        lcd.setRGB(0,100,0); // Green
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Replenishing..");
        lcd.setCursor(0, 1);
        lcd.print("Wipes");
        Serial.println(".............REPLENISHING WIPES..............");
        unsigned int y = refresh_access_token();
        Serial.println(y);
        delay(500);
        unsigned int z=end_point_request_replenishment(slot_id2_wipes);
        Serial.println(z);
        if(z==11)
        {
          lcd.setRGB(100,100,100); // white
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Order Placed..");
          lcd.setCursor(0, 1);
          lcd.print("Wipes");
          re2_rep_flag = 0;
        }
        else if(z==13)
        {
          lcd.setRGB(100,100,100); // white
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("In Progress..");
          lcd.setCursor(0, 1);
          lcd.print("Wipes");
          re2_rep_flag = 0;
        }
        else
        {
          lcd.setRGB(100,200,100); // wwrb
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Order Failed");
          lcd.setCursor(0, 1);
          lcd.print("Wipes");
        }
        delay(5000);
      }
      else if(re3_rep_flag == 1)
      {
        con_flag=1;
        lcd.setRGB(0,100,0); // Green
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Replenishing..");
        lcd.setCursor(0, 1);
        lcd.print("Soap");
        Serial.println(".............REPLENISHING SOAP..............");
        unsigned int y = refresh_access_token();
        Serial.println(y);
        delay(500);
        unsigned int z=end_point_request_replenishment(slot_id3_soap);
        Serial.println(z);
        if(z==11)
        {
          lcd.setRGB(100,100,100); // white
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Order Placed..");
          lcd.setCursor(0, 1);
          lcd.print("Soap");
          re3_rep_flag = 0;
        }
        else if(z==13)
        {
          lcd.setRGB(100,100,100); // white
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("In Progress..");
          lcd.setCursor(0, 1);
          lcd.print("Soap");
          re3_rep_flag = 0;
        }
        else
        {
          lcd.setRGB(200,100,100); // wwrb
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Order Failed..");
          lcd.setCursor(0, 1);
          lcd.print("Soap");
         }
         delay(5000);
      }
      if(con_flag==1)
      {
        //lcd.setRGB(100,100,100); // White
        //lcd.clear();
        //lcd.setCursor(0, 0);
        //lcd.print("Connected..");
        //lcd.setCursor(0, 1);
        //lcd.print("All perfect..");
        //Serial.println("connected to AP");
        con_flag = 0;  
      }
  }
  else
  {
    digitalWrite(ST_LED,HIGH);
    delay(200);
    digitalWrite(ST_LED,LOW);
    lcd.setRGB(100,0,0); // Red
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Not Connected..");
    lcd.setCursor(0, 1);
    lcd.print("Waiting for AP..");
    Serial.println("not connected... Waiting for AP");
    delay(3000);
  }
}

unsigned int obtain_access_and_refresh_token() 
{
  char _tmp_token[500];
  unsigned int _len_str;
  Serial.println();
  Serial.println("OBTAIN ACCESS AND REFRESH TOKEN FROM AUTH CODE:\r\nConnecting to Host: api.amazon.com");
  _len_str = eepromReadString(0, 30, _tmp_token);
  Serial.println(_tmp_token);
  // Use WiFiClientSecure class to create TCP connections
  if (!client.connect("api.amazon.com", 443)) {
    Serial.println("Error 5 --> Connection failed");
    return 5;
  }
  Serial.println("Connected\r\nSending Request for obtain Tokens");
  // This will send request for refresh of Access token to the server
  client.println("POST /auth/o2/token HTTP/1.1");
  client.println("Host: api.amazon.com"); 
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Cache-Control: no-cache");
  client.print("Content-Length: ");client.println(con_length);
  client.println();
  client.print("grant_type=authorization_code");
  client.print("&code=");
  client.print(_tmp_token);
  client.print("&client_id=");
  client.print(client_id);
  client.print("&client_secret=");
  client.print(client_secret);
  client.print("&redirect_uri=");
  client.println(redirect_uri);
  Serial.println("Request Send");
  
  unsigned long timeout = millis();
  while (client.available() == 0) 
  {
    if (millis() - timeout > 35000) 
    {
      Serial.println("Error 4 --> Connection Timeout !");
      client.stop();
      return 4;
    }
  }
  // Read all the lines of the reply from server and print them to Serial
  String _line = "temp";
  unsigned int _line_no = 1;
  while(client.available())
  {
    _line = client.readStringUntil('\r');
    Serial.print(_line);
    if ((_line_no == 1)&& (_line != "HTTP/1.1 200 OK"))
    {
      Serial.println();
      Serial.println("Error 3 --> Invalid Response");
      client.stop();
      return 3;
    }
    _line_no++;
  }
  DynamicJsonBuffer jsonBuffer;
  // Responce last line have access token and referesh token json oblect 
  JsonObject& root = jsonBuffer.parseObject(_line);

  // Test if parsing succeeds.
  if (!root.success()) {
    Serial.println("Error 2 --> Json parseObject() failed");
    return 2;
  }
  const char* _refresh_token = root["refresh_token"];
  // Print values
  Serial.println();
  Serial.print("Refresh token: ");
  Serial.println(_refresh_token);
  _len_str = eepromWriteString1(100, 500, _refresh_token);
  EEPROM.commit();
  Serial.print("Length: ");
  Serial.println(_len_str);
  //_len_str = eepromReadString(105, 500, _tmp_token);
  //Serial.print("Refresh token in EEPROM: ");
  //Serial.println(_tmp_token);
  //Serial.println(_len_str);
  Serial.println("SUCCESS");
  return 1;
}


unsigned int refresh_access_token() 
{
  char _tmp_token[500];
  unsigned int _len_str;
  Serial.println();
  Serial.println("REQUEST NEW ACCESS TOKEN BY PROVIDING REFRESH TOKEN:\r\nConnecting to Host: api.amazon.com");
  _len_str = eepromReadString(105, 500, _tmp_token);
  //Serial.println(_tmp_token);
  // Use WiFiClientSecure class to create TCP connections
  if (!client.connect("api.amazon.com", 443)) {
    Serial.println("Error 5 --> Connection failed");
    return 5;
  }
  Serial.println("Connected\r\nSending Request for obtain Tokens");
  // This will send request for refresh of Access token to the server
  client.println("POST /auth/o2/token HTTP/1.1");
  client.println("Host: api.amazon.com"); 
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Cache-Control: no-cache");
  client.print("Content-Length: ");client.println(_len_str+197);
  client.println();
  client.print("grant_type=refresh_token");
  client.print("&refresh_token=Atzr%7C");
  client.print(_tmp_token);
  client.print("&client_id=");
  client.print(client_id);
  client.print("&client_secret=");
  client.println(client_secret);
  Serial.println("Request Send");
  
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 35000) {
      Serial.println("Error 4 --> Connection Timeout !");
      client.stop();
      return 4;
    }
  }
  // Read all the lines of the reply from server and print them to Serial
  String _line = "temp";
  unsigned int _line_no = 1;
  while(client.available())
  {
    _line = client.readStringUntil('\r');
    Serial.print(_line);
    if ((_line_no == 1)&& (_line != "HTTP/1.1 200 OK"))
    {
      Serial.println();
      Serial.println("Error 3 --> Invalid Response");
      client.stop();
      return 3;
    }
    _line_no++;
  }
  DynamicJsonBuffer jsonBuffer;
  // Responce last line have access token and referesh token json oblect 
  JsonObject& root = jsonBuffer.parseObject(_line);

  // Test if parsing succeeds.
  if (!root.success()) {
    Serial.println("Error 2 --> Json parseObject() failed");
    return 2;
  }
  const char* access_token = root["access_token"];
  // Print values
  Serial.println();
  Serial.print("Access token: ");
  Serial.println(access_token);
  _len_str = eepromWriteString1(600, 500, access_token);
  EEPROM.commit();
  Serial.print("Length: ");
  Serial.println(_len_str);
  //len_str = eepromReadString(605,500, _tmp_token);
  //Serial.print("Access token in EEPROM: ");
  //Serial.println(_tmp_token);
  //Serial.println(_len_str);
  Serial.println("SUCCESS");
  return 1;
}

unsigned int end_point_request_replenishment(const char* _slot_id)
{
  char _tmp_token[500];
  unsigned int _len_str;
  Serial.println();
  Serial.println("REQUEST FOR REPLENISHMENT:\r\nConnecting to Host: dash-replenishment-service-na.amazon.com");
  _len_str = eepromReadString(605,500, _tmp_token);
  Serial.println(_tmp_token);
  // Use WiFiClientSecure class to create TCP connections
  if(!client.connect("dash-replenishment-service-na.amazon.com", 443)) 
  {
    Serial.println("Error 5 --> Connection failed");
    return 5;
  }
  Serial.println("Connected\r\nSending Request for Replenishment");
  //This will send the replenishment request to the Amazon DRS server
  client.print("POST /replenish/");client.print(_slot_id); client.println(" HTTP/1.1");
  client.println("Host: dash-replenishment-service-na.amazon.com");
  client.print("Authorization: Bearer Atza|");
  client.println(_tmp_token);
  client.println("Content-Length: 0");
  client.println("X-Amzn-Accept-Type: com.amazon.dash.replenishment.DrsReplenishResult@1.0");
  client.println("X-Amzn-Type-Version: com.amazon.dash.replenishment.DrsReplenishInput@1.0");
  client.println();
  Serial.println("Request Send");
  
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 20000) {
      Serial.println("Error 4 --> Connection Timeout !");
      client.stop();
      return 4;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  String _line = "temp";
  unsigned int _line_no = 1;
  while(client.available())
  {
    _line = client.readStringUntil('\r');
    Serial.print(_line);
    if ((_line_no == 1)&& (_line != "HTTP/1.1 200 OK"))
    {
      Serial.println();
      Serial.println("Error 3 --> Invalid Response");
      client.stop();
      return 3;
    }
    _line_no++;
  }
  DynamicJsonBuffer jsonBuffer;
  // Responce last line have access token and referesh token json oblect 
  JsonObject& root = jsonBuffer.parseObject(_line);

  // Test if parsing succeeds.
  if (!root.success()) {
    Serial.println("Error 2 --> Json parseObject() failed");
    return 2;
  }
  const char* _detailCode = root["detailCode"];
  // Print values
  Serial.println();
  Serial.print("detailCode: ");
  Serial.println(_detailCode);
  //_len_str = eepromWriteString1(600, 500, _detailCode);
  //EEPROM.commit();
  //Serial.print("Length: ");
  //Serial.println(_len_str);
  //len_str = eepromReadString(605,500, _tmp_token);
  //Serial.print("Access token in EEPROM: ");
  //Serial.println(_tmp_token);
  //Serial.println(_len_str);
  if(strcmp(_detailCode,"STANDARD_ORDER_PLACED")==0)
  {
    Serial.println("SUCCESS 11 --> Standard Order Placed");
    return 11;
  }
  else if(strcmp(_detailCode,"TEST_ORDER_PLACED")==0)
  {
    Serial.println("SUCCESS 12 --> Test Order Placed");
    return 12;
  }
  else if(strcmp(_detailCode,"ORDER_INPROGRESS")==0)
  {
    Serial.println("SUCCESS 13 --> Order In Progress");
    return 13;
  }
  else
  {
    return 0;
  }
}

unsigned int end_point_request_subscriptioninfo(const char* _slot_id)
{
  char _tmp_token[500];
  unsigned int _len_str;
  Serial.println();
  Serial.println("REQUEST FOR REPLENISHMENT:\r\nConnecting to Host: dash-replenishment-service-na.amazon.com");
  _len_str = eepromReadString(605,500, _tmp_token);
  Serial.println(_tmp_token);
  // Use WiFiClientSecure class to create TCP connections
  if(!client.connect("dash-replenishment-service-na.amazon.com", 443)) 
  {
    Serial.println("Error 5 --> Connection failed");
    return 5;
  }
  Serial.println("Connected\r\nSending Request for Replenishment");
  //This will send the replenishment request to the Amazon DRS server
  client.print("POST /replenish/");client.print(_slot_id); client.println("HTTP/1.1");
  client.println("Host: dash-replenishment-service-na.amazon.com");
  client.print("Authorization: Bearer Atza|");
  client.println(_tmp_token);
  client.println("Content-Length: 0");
  client.println("X-Amzn-Accept-Type: com.amazon.dash.replenishment.DrsReplenishResult@1.0");
  client.println("X-Amzn-Type-Version: com.amazon.dash.replenishment.DrsReplenishInput@1.0");
  client.println();
  Serial.println("Request Send");
  
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 20000) {
      Serial.println("Error 4 --> Connection Timeout !");
      client.stop();
      return 4;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  String _line = "temp";
  unsigned int _line_no = 1;
  while(client.available())
  {
    _line = client.readStringUntil('\r');
    Serial.print(_line);
    if ((_line_no == 1)&& (_line != "HTTP/1.1 200 Accepted"))
    {
      Serial.println();
      Serial.println("Error 3 --> Invalid Response");
      client.stop();
      return 3;
    }
    _line_no++;
  }
  DynamicJsonBuffer jsonBuffer;
  // Responce last line have access token and referesh token json oblect 
  JsonObject& root = jsonBuffer.parseObject(_line);

  // Test if parsing succeeds.
  if (!root.success()) {
    Serial.println("Error 2 --> Json parseObject() failed");
    return 2;
  }
  const char* _detailCode = root["detailCode"];
  // Print values
  Serial.println();
  Serial.print("detailCode: ");
  Serial.println(_detailCode);
  //_len_str = eepromWriteString1(600, 500, _detailCode);
  //EEPROM.commit();
  //Serial.print("Length: ");
  //Serial.println(_len_str);
  //len_str = eepromReadString(605,500, _tmp_token);
  //Serial.print("Access token in EEPROM: ");
  //Serial.println(_tmp_token);
  //Serial.println(_len_str);
  if(strcmp(_detailCode,"STANDARD_ORDER_PLACED")==0)
  {
    Serial.println("SUCCESS 11 --> Standard Order Placed");
    return 11;
  }
  else if(strcmp(_detailCode,"TEST_ORDER_PLACED")==0)
  {
    Serial.println("SUCCESS 12 --> Test Order Placed");
    return 12;
  }
  else if(strcmp(_detailCode,"ORDER_INPROGRESS")==0)
  {
    Serial.println("SUCCESS 13 --> Order In Progress");
    return 13;
  }
  else
  {
    return 0;
  }
}

void config_ap_auth(void) 
{
  char auth_temp[30];
  eepromReadString(0, 30, auth_temp);
  //WiFiManager
  WiFiManagerParameter custom_auth_code("Authorization", "authorization code", auth_temp, 25);
  WiFiManagerParameter custom_text("<br/>DRS Authorization Code");
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.addParameter(&custom_text);
  wifiManager.addParameter(&custom_auth_code);
  if (!wifiManager.startConfigPortal("DRS4Baby")) 
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  strcpy(auth_temp, custom_auth_code.getValue());
  Serial.println(auth_temp);
  if(auth_temp[0] != 0x00)
  {
    eepromWriteString(0,21, auth_temp);
    EEPROM.commit();
    Serial.println("Auth code updated");
  }
  else
  {
    Serial.println("Auth code not Supplied");
  }
  //if you get here you have connected to the WiFi
  Serial.println("AP and Auth config done :)");
}

unsigned int is_wifi_connected()
{
  if (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.print(".");
    return 0;
  }
  else
  {
     return 1;
  }  
}


void re1_isr()
{
  int tm = millis();
  detachInterrupt(RE1_PIN);
  while((millis()-tm) < 500);
  if(re1_cnt > 0)re1_cnt = re1_cnt - 1;
  if(re1_cnt == TH_RE1) re1_rep_flag = 1; 
  attachInterrupt(RE1_PIN,re1_isr,RISING);
}

void re2_isr()
{
  int tm = millis();
  detachInterrupt(RE2_PIN);
  while((millis()-tm) < 500);
  if(re2_cnt > 0)re2_cnt = re2_cnt - 1;
  if(re2_cnt == TH_RE2) re2_rep_flag = 1; 
  attachInterrupt(RE2_PIN,re2_isr,RISING);
}

void re3_isr()
{
  int tm = millis();
  detachInterrupt(RE3_PIN);
  while((millis()-tm) < 500);
  if(re3_cnt > 0)re3_cnt = re3_cnt - 1;
  if(re3_cnt == TH_RE3) re3_rep_flag = 1; 
  attachInterrupt(RE3_PIN,re3_isr,RISING);
}

void rst_isr()
{
  int tm = millis();
  detachInterrupt(RST_PIN);
  tm = millis();
  while((millis()-tm) < 2000);
  if(digitalRead(RST_PIN)==LOW && rst_flag == 0)
  {
    rst_flag = 1;
    rst_slot = 0; 
  }
  else if (digitalRead(RST_PIN)==LOW && rst_flag == 1)
  {
    rst_flag = 2;
    if(rst_slot == 1)re1_cnt = MAX_RE1;
    else if(rst_slot == 2)re2_cnt = MAX_RE2; 
    else if(rst_slot == 3)re3_cnt = MAX_RE3;   
  }
  else if(digitalRead(RST_PIN)==HIGH && rst_flag == 1)
  {
    rst_slot = rst_slot + 1;
    if(rst_slot >=3)rst_slot = 3;
  }
  attachInterrupt(RST_PIN,rst_isr,FALLING);
}


