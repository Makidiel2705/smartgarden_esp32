#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include "DHT.h"
#include <SimpleTimer.h>
#include <ArduinoJson.h>
#include <wifi_provisioning/manager.h>
#include <HTTPClient.h>
#include "EasyNextionLibrary.h"

EasyNex myNex(Serial2);

// Set Defalt Values
#define DEFAULT_RELAY_MODE true
#define DEFAULT_Temperature 0
#define DEFAULT_Humidity 0

#define DEFAULT_DIMMER_LEVEL 0

// BLE Credentils
const char *service_name = "PROV_12345";
const char *pop = "1234567";

// GPIO
static uint8_t gpio_reset = 0;
static uint8_t PIN_DHT = 32;
static uint8_t PIN_LIGHT = 35;
static uint8_t PIN_SOIL = 33;
static uint8_t PIN_CURTAIN_OPEN = 22; // Chân điều khiển mở rèm
static uint8_t PIN_CURTAIN_CLOSE = 23; // Chân điều khiển đóng rèm
static uint8_t PIN_LAMP = 18;//
static uint8_t PIN_PUMP = 5;//
static uint8_t PIN_FAN = 19;//
static uint8_t PIN_BUZZER = 21;//

bool lamp_state = true;
bool pump_state = true;
bool fan_state = true;
bool isAuto_state = true; 
bool isAlert_state = true;
bool isAlertactive = false;
bool curtain_state = false; // false: đóng, true: mở

float brightness_sensorValue = 0;
float moisture_sensorValue = 0;
float humidity_sensorValue = 0;
float temperature_sensorValue = 0;

bool wifi_connected = 0;

// Value API
String description_API;
float temperature_API; //value_auto_tempAir
float humidity_API; //value_auto_humAir
float windSpeed_API; //value_auto_humSoil
float pressure_API; //value_auto_light
int windDeg_API; //roof
float feelLike_API;

//Auto values
int temperature_auto_value = 0; //value_auto
int moisture_auto_value = 0; //value_auto
int brightness_auto_value = 0; //value_auto
int humidity_auto_value = 0; //value_auto
int curtain_auto_value = 100;

// location
String lat = "16.074146"; //Truong DHBK
String lon = "108.150764";

//URL Endpoint for the API openweathermap.org
String URL = "http://api.openweathermap.org/data/2.5/weather?";
String ApiKey = "b8061abfee9aedb75a08e52f53fad5a8";

DHT dht(PIN_DHT, DHT11);

SimpleTimer Timer;

// Temp
int temp;

//------------------------------------------- Declaring Devices -----------------------------------------------------//

//The framework provides some standard device types like switch, lightbulb, fan, temperature sensor.
static TemperatureSensor temperature("Temperature");
static TemperatureSensor humidity("Humidity");
static TemperatureSensor moisture("Moisture");
static TemperatureSensor brightness("Brightness");  
static Switch lamp_switch("LAMP", &PIN_LAMP);
static Switch pump_switch("PUMP", &PIN_PUMP);
static Fan fan_switch("FAN", &PIN_FAN);
static Switch curtain_switch("CURTAIN");
static Switch auto_switch("AUTO");
static Switch alert_switch("ALERT");


void sysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
      printQR(service_name, pop, "ble");
#else
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
      printQR(service_name, pop, "softap");
#endif
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("\nConnected to Wi-Fi!\n");
      wifi_connected = 1;
      delay(500);
      break;
    case ARDUINO_EVENT_PROV_CRED_RECV: {
        Serial.println("\nReceived Wi-Fi credentials");
        Serial.print("\tSSID : ");
        Serial.println((const char *) sys_event->event_info.prov_cred_recv.ssid);
        Serial.print("\tPassword : ");
        Serial.println((char const *) sys_event->event_info.prov_cred_recv.password);
        break;
      }
    case ARDUINO_EVENT_PROV_INIT:
      wifi_prov_mgr_disable_auto_stop(10000);
      break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      Serial.println("Stopping Provisioning!!!");
      wifi_prov_mgr_stop_provisioning();
      break;
  }
}

void GET_weather() {
    if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    //Set HTTP Request Final URL with Location and API key information
    http.begin(URL + "lat=" + lat + "&lon=" + lon + "&units=metric&appid=" + ApiKey);

    // start connection and send HTTP Request
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {

      //Read Data as a JSON string
      String JSON_Data = http.getString();
      // Serial.println(JSON_Data);

      //Retrieve some information about the weather from the JSON format
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, JSON_Data);
      JsonObject obj = doc.as<JsonObject>();

      //Display the Current Weather Info
      description_API = obj["weather"][0]["description"].as<String>();
      temperature_API = obj["main"]["temp"].as<float>();
      humidity_API = obj["main"]["humidity"].as<float>();
      windSpeed_API = obj["wind"]["speed"].as<float>();
      pressure_API = obj["main"]["pressure"].as<float>();
      windDeg_API = obj["wind"]["deg"].as<int>();
      feelLike_API = obj["main"]["feels_like"].as<float>();            


      Serial.print("Wind Direction: ");
      Serial.println(windDeg_API);
      Serial.print("THOI TIET: ");
      Serial.println(description_API);
      Serial.print("NHIET DO: ");
      Serial.print(temperature_API); //
      Serial.println("°C");
      Serial.print("DO AM: ");
      Serial.print(humidity_API); //
      Serial.print("%  -   HUONG GIO:");
      Serial.print(windDeg_API);
      Serial.println("o");
      Serial.print("Toc do gio: ");
      Serial.print(windSpeed_API);
      Serial.println("m/s");
      Serial.print("Ap suat khong khi: ");
      Serial.print(pressure_API);
      Serial.println("hPa");
      Serial.print("Feel like: ");
      Serial.print(feelLike_API);
      Serial.println("°C");
      Serial.println("");
    }
  }
}

void sendWeatherDisplay() {
  myNex.writeStr("t_desapi.txt", description_API);

  temp = temperature_API*100;                        
  myNex.writeNum("x_tempapi.val", temp); 

  temp = feelLike_API*100;                        
  myNex.writeNum("x_feelapi.val", temp); 

  temp = humidity_API;
  myNex.writeNum("n_humiapi.val", temp);

  temp = pressure_API;
  myNex.writeNum("n_pressapi.val", temp);

  temp = windSpeed_API*100;                        
  myNex.writeNum("x_windapi.val", temp); 
}

void activeBuzzerShort() {
    digitalWrite(PIN_BUZZER, HIGH); 
    delay(100);             
    digitalWrite(PIN_BUZZER, LOW);  
}

void autoControl() {
  Serial.println("CHANGE STATE AUTO......");
  if (isAuto_state == true) {
    if (brightness_auto_value > brightness_sensorValue && lamp_state == false) {
    digitalWrite(PIN_LAMP, HIGH);   
    activeBuzzerShort();
    lamp_state = true;
    myNex.writeNum("b_lamp.bco", 2016); 
    myNex.writeStr("b_lamp.txt", "ON");             
    lamp_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, lamp_state);  
    Serial.println("Lamp on");
    }
    else if (brightness_auto_value <= brightness_sensorValue && lamp_state == true) {
    digitalWrite(PIN_LAMP, LOW);    
    activeBuzzerShort();
    lamp_state = false;     
    myNex.writeNum("b_lamp.bco", 63488); 
    myNex.writeStr("b_lamp.txt", "OFF");  
    lamp_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, lamp_state);  
    Serial.println("Lamp off");
    }
  }
  
    if (isAuto_state == true) {
    if (moisture_auto_value > moisture_sensorValue && pump_state == false) {
    digitalWrite(PIN_PUMP, HIGH);
    activeBuzzerShort();   
    pump_state = true;  
    myNex.writeNum("b_pump.bco", 2016); 
    myNex.writeStr("b_pump.txt", "ON");           
    pump_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, pump_state);  
    Serial.println("Pump on");
    }
    else if (moisture_auto_value <= moisture_sensorValue && pump_state == true){
    digitalWrite(PIN_PUMP, LOW); 
    activeBuzzerShort();   
    pump_state = false;       
    myNex.writeNum("b_pump.bco", 63488); 
    myNex.writeStr("b_pump.txt", "OFF");      
    pump_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, pump_state);  
    Serial.println("Pump off");
    }
  }

    if (isAuto_state == true) {
    if (temperature_auto_value < temperature_sensorValue && fan_state == false) {
    digitalWrite(PIN_FAN, HIGH);
    activeBuzzerShort();  
    fan_state = true;     
    myNex.writeNum("b_fan.bco", 2016); 
    myNex.writeStr("b_fan.txt", "ON");       
    fan_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, fan_state);  
    Serial.println("Fan on");
    }
    else if (temperature_auto_value >= temperature_sensorValue && fan_state == true) {
    digitalWrite(PIN_FAN, LOW);  
    activeBuzzerShort();  
    fan_state = false;        
    myNex.writeNum("b_fan.bco", 63488); 
    myNex.writeStr("b_fan.txt", "OFF");    
    fan_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, fan_state);  
    Serial.println("Fan off");
    }
  }

    if (isAuto_state == true) {
      if (brightness_sensorValue > curtain_auto_value && !curtain_state) { // Độ sáng thấp, mở rèm
        digitalWrite(PIN_CURTAIN_OPEN, HIGH);
        digitalWrite(PIN_CURTAIN_CLOSE, LOW);
        delay(9000);
        digitalWrite(PIN_CURTAIN_OPEN, LOW);
        digitalWrite(PIN_CURTAIN_CLOSE, LOW);
        curtain_state = true;
        myNex.writeNum("b_curtain.bco", 2016); 
        myNex.writeStr("b_curtain.txt", "ON");
        curtain_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, curtain_state);
        Serial.println("Curtain opened (auto).");
      } else if (brightness_sensorValue <= curtain_auto_value && curtain_state) { // Độ sáng cao, đóng rèm
        digitalWrite(PIN_CURTAIN_OPEN, LOW);
        digitalWrite(PIN_CURTAIN_CLOSE, HIGH);
        delay(9000);
        digitalWrite(PIN_CURTAIN_OPEN, LOW);
        digitalWrite(PIN_CURTAIN_CLOSE, LOW);
        curtain_state = false;
        myNex.writeNum("b_curtain.bco", 63488); 
        myNex.writeStr("b_curtain.txt", "OFF");  
        curtain_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, curtain_state);
        Serial.println("Curtain closed (auto).");
      }
    }
}

void autoAlert() {
  if (isAlert_state == true) {
    if ((temperature_auto_value < temperature_sensorValue && fan_state == false) || (moisture_auto_value > moisture_sensorValue && pump_state == false) || (brightness_auto_value > brightness_sensorValue && lamp_state == false)) {
      digitalWrite(PIN_BUZZER, HIGH);
      isAlertactive = true;
      Serial.println("ALERT!!!");
      if (temperature_auto_value < temperature_sensorValue && fan_state == false) {
        esp_rmaker_raise_alert("Garden's Temperature is too high!"); 
        myNex.writeStr("t_alert1.txt", "Temperature is too high!");
        myNex.writeStr("t_alert2.txt", "You should turn on fan");
      }
      if (moisture_auto_value > moisture_sensorValue && pump_state == false) {
        esp_rmaker_raise_alert("Garden's Moisture is too high!"); 
        myNex.writeStr("t_alert1.txt", "Moisture is too low!");
        myNex.writeStr("t_alert2.txt", "You should turn on pump");
      }
      if (brightness_auto_value > brightness_sensorValue && lamp_state == false) {
        esp_rmaker_raise_alert("Garden's Brightness is too low!"); 
        myNex.writeStr("t_alert1.txt", "Brightness is too low!");
        myNex.writeStr("t_alert2.txt", "You should turn on lamp");
      }
    }
  else {
      digitalWrite(PIN_BUZZER, LOW);
      isAlertactive = false;
  }
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx) {
  const char *device_name = device->getDeviceName();
  Serial.println(device_name);
  const char *param_name = param->getParamName();

  if (strcmp(device_name, "CURTAIN") == 0) {
    if (strcmp(param_name, "Power") == 0) {
        if (isAuto_state == false) { // Chỉ cho phép điều khiển thủ công khi tắt chế độ tự động
        Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
        curtain_state = val.val.b;

        if (curtain_state == true) {
            digitalWrite(PIN_CURTAIN_OPEN, HIGH);
            digitalWrite(PIN_CURTAIN_CLOSE, LOW);
            myNex.writeNum("b_curtain.bco", 2016); 
            myNex.writeStr("b_curtain.txt", "ON"); 
            delay(9000);
            digitalWrite(PIN_CURTAIN_OPEN, LOW);
            digitalWrite(PIN_CURTAIN_CLOSE, LOW);
        } else {
            digitalWrite(PIN_CURTAIN_OPEN, LOW);
            digitalWrite(PIN_CURTAIN_CLOSE, HIGH);
            myNex.writeNum("b_curtain.bco", 63488); 
            myNex.writeStr("b_curtain.txt", "OFF");
            delay(9000);
            digitalWrite(PIN_CURTAIN_OPEN, LOW);
            digitalWrite(PIN_CURTAIN_CLOSE, LOW);
        }

        param->updateAndReport(val);
        } else {
            esp_rmaker_raise_alert("You must turn off auto to toggle this device.");
            myNex.writeStr("t_alert1.txt", "AUTO is ON");
            myNex.writeStr("t_alert2.txt", "Turn off auto to toggle device!");

        }
    }
    if (strcmp(param_name, "Level_curtain") == 0) 
    {
      curtain_auto_value = val.val.i;
      Serial.print("Curtain auto value set to: "); 
      Serial.println(curtain_auto_value); 
      param->updateAndReport(val);

    }
  }

  if (strcmp(device_name, "LAMP") == 0) {
    if (strcmp(param_name, "Power") == 0) {
      if (isAuto_state == false) {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      lamp_state = val.val.b;
      // (lamp_state == false) ? digitalWrite(PIN_LAMP, LOW) : digitalWrite(PIN_LAMP, HIGH);
      if (lamp_state == false){
        digitalWrite(PIN_LAMP, LOW);
        myNex.writeNum("b_lamp.bco", 63488); 
        myNex.writeStr("b_lamp.txt", "OFF");
      }
      else {
        digitalWrite(PIN_LAMP, HIGH);
        myNex.writeNum("b_lamp.bco", 2016); 
        myNex.writeStr("b_lamp.txt", "ON"); 
      }
      activeBuzzerShort();
      param->updateAndReport(val);
      }
      else {
      esp_rmaker_raise_alert("You must turn off auto to toggle this device."); 
      myNex.writeStr("t_alert1.txt", "AUTO is ON");
      myNex.writeStr("t_alert2.txt", "Turn off auto to toggle device!");
      }
    }
    if (strcmp(param_name, "Level_lamp") == 0) 
    {
      brightness_auto_value = val.val.i;
      myNex.writeNum("n_lamp.val", brightness_auto_value);
      myNex.writeNum("h_lamp.val", brightness_auto_value);
      Serial.print("Brightness auto value set to: "); 
      Serial.println(brightness_auto_value); 
      param->updateAndReport(val);

    }
  }
  if (strcmp(device_name, "PUMP") == 0) {
    if (strcmp(param_name, "Power") == 0) {
      if (isAuto_state == false) {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      pump_state = val.val.b;
      // (pump_state == false) ? digitalWrite(PIN_PUMP, LOW) : digitalWrite(PIN_PUMP, HIGH);
      if (pump_state == false){
        digitalWrite(PIN_PUMP, LOW);
        myNex.writeNum("b_pump.bco", 63488); 
        myNex.writeStr("b_pump.txt", "OFF");
      }
      else {
        digitalWrite(PIN_PUMP, HIGH);
        myNex.writeNum("b_pump.bco", 2016); 
        myNex.writeStr("b_pump.txt", "ON"); 
      }
      activeBuzzerShort();
      param->updateAndReport(val);
      }
      else {
      esp_rmaker_raise_alert("You must turn off auto to toggle this device."); 
      myNex.writeStr("t_alert1.txt", "AUTO is ON");
      myNex.writeStr("t_alert2.txt", "Turn off auto to toggle device!");
      }
    }
    if (strcmp(param_name, "Level_pump") == 0) 
    {
      moisture_auto_value = val.val.i;
      myNex.writeNum("n_pump.val", moisture_auto_value);
      myNex.writeNum("h_pump.val", moisture_auto_value);
      Serial.print("Moisture auto value set to: "); 
      Serial.println(moisture_auto_value); 
      param->updateAndReport(val);

    }
  }
  if (strcmp(device_name, "FAN") == 0) {
    if (strcmp(param_name, "Power") == 0) {
      if (isAuto_state == false) {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      fan_state = val.val.b;
      // (fan_state == false) ? digitalWrite(PIN_FAN, LOW) : digitalWrite(PIN_FAN, HIGH);
      if (fan_state == false){
        digitalWrite(PIN_FAN, LOW);
        myNex.writeNum("b_fan.bco", 63488); 
        myNex.writeStr("b_fan.txt", "OFF");
      }
      else {
        digitalWrite(PIN_FAN, HIGH);
        myNex.writeNum("b_fan.bco", 2016); 
        myNex.writeStr("b_fan.txt", "ON"); 
      }
      activeBuzzerShort();
      param->updateAndReport(val);
      }
      else {
      esp_rmaker_raise_alert("You must turn off auto to toggle this device."); 
      myNex.writeStr("t_alert1.txt", "AUTO is ON");
      myNex.writeStr("t_alert2.txt", "Turn off auto to toggle device!");
      }
    }
    if (strcmp(param_name, "Level_fan") == 0) 
    {
      temperature_auto_value = val.val.i;
      myNex.writeNum("n_fan.val", temperature_auto_value);
      myNex.writeNum("h_fan.val", temperature_auto_value);
      Serial.print("Temperature auto value set to: "); 
      Serial.println(temperature_auto_value); 
      param->updateAndReport(val);
    }
  }

  if (strcmp(device_name, "AUTO") == 0)
  {
    if (strcmp(param_name, "Power") == 0)
    {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      isAuto_state = val.val.b;
      if (isAuto_state == true) {
      myNex.writeNum("b_auto.bco", 2016); 
      myNex.writeStr("b_auto.txt", "ON");
      }
      else {
      myNex.writeNum("b_auto.bco", 63488); 
      myNex.writeStr("b_auto.txt", "OFF");
      }
      param->updateAndReport(val);
      Serial.print("AUTO IS: "); 
      Serial.println(isAuto_state); 
    }
  }

    if (strcmp(device_name, "ALERT") == 0)
  {
    if (strcmp(param_name, "Power") == 0)
    {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      isAlert_state = val.val.b;
      if (isAlert_state == true) {
      myNex.writeNum("b_alert.bco", 2016); 
      myNex.writeStr("b_alert.txt", "ON");
      }
      else {
      myNex.writeNum("b_alert.bco", 63488); 
      myNex.writeStr("b_alert.txt", "OFF");
      }
      param->updateAndReport(val);
      if (isAlertactive == true && isAlert_state == false) {
        digitalWrite(PIN_BUZZER, LOW);
        isAlertactive = false;
  
      }
      Serial.print("ALERT IS: "); 
      Serial.println(isAlert_state); 
    }
  }
}

void Send_Sensor() {
  // Send temperature
  if (abs(temperature_sensorValue - dht.readTemperature()) >= 0.2) {
  temperature_sensorValue = dht.readTemperature();
  Serial.println("Sending temperature_sensorValue......");
  temperature.updateAndReportParam("Temperature", temperature_sensorValue);
  temp = temperature_sensorValue*10;                        
  myNex.writeNum("x_temp.val", temp); 
  }

  if (abs(humidity_sensorValue - dht.readHumidity()) >= 2) {
  humidity_sensorValue = dht.readHumidity();
  Serial.println("Sending humidity_sensorValue......");
  humidity.updateAndReportParam("Temperature", humidity_sensorValue);
  temp = humidity_sensorValue*10;                        
  myNex.writeNum("x_humi.val", temp);   
  }

  if (moisture_sensorValue != map(analogRead(PIN_SOIL), 4095, 0, 0, 100)) {
  moisture_sensorValue = map(analogRead(PIN_SOIL), 4095, 0, 0, 100);
  Serial.println("Sending moisture_sensorValue......");
  moisture.updateAndReportParam("Temperature", moisture_sensorValue);
  temp = moisture_sensorValue*10;                        
  myNex.writeNum("x_mois.val", temp); 
  }

  if(abs(brightness_sensorValue - map(analogRead(PIN_LIGHT), 4095, 0, 0, 100)) >= 5) {
  brightness_sensorValue = map(analogRead(PIN_LIGHT), 4095, 0, 0, 100);
  Serial.println("Sending brightness_sensorValue......");
  brightness.updateAndReportParam("Temperature", brightness_sensorValue);
  temp = brightness_sensorValue*10;                        
  myNex.writeNum("x_bright.val", temp); 
  }
  // humidity_sensorValue = dht.readHumidity();
  // temperature_sensorValue = dht.readTemperature();
  // brightness_sensorValue = map(analogRead(PIN_LIGHT), 4095, 0, 0, 100);
  // moisture_sensorValue = map(analogRead(PIN_SOIL), 4095, 0, 0, 100);

  Serial.print("Temperature - "); 
  Serial.println(temperature_sensorValue); 

  Serial.print("Humidity - "); 
  Serial.println(humidity_sensorValue);


  Serial.print("Moisture - "); 
  Serial.println(moisture_sensorValue);

  Serial.print("Brightness - "); 
  Serial.println(brightness_sensorValue);
}

// control lamp via display printh 23 02 54 00
void trigger0() {
  if (isAuto_state == false) {
    if (lamp_state == false) {
      myNex.writeNum("b_lamp.bco", 2016); 
      myNex.writeStr("b_lamp.txt", "ON");  
      Serial.println("LAMP: ON");
      digitalWrite(PIN_LAMP, HIGH);
      lamp_state = true;

    } else {
      myNex.writeNum("b_lamp.bco", 63488); 
      myNex.writeStr("b_lamp.txt", "OFF");
      Serial.println("LAMP: OFF");
      digitalWrite(PIN_LAMP, LOW);  
      lamp_state = false;
    }
    lamp_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, lamp_state);
  }
}

// control fan via display printh 23 02 54 01
void trigger1() {
  if (isAuto_state == false) {
    if (fan_state == false) {
      myNex.writeNum("b_fan.bco", 2016); 
      myNex.writeStr("b_fan.txt", "ON");  
      Serial.println("FAN: ON");
      digitalWrite(PIN_FAN, HIGH); 
      fan_state = true;

    } else {
      myNex.writeNum("b_fan.bco", 63488); 
      myNex.writeStr("b_fan.txt", "OFF");
      Serial.println("FAN: OFF");
      digitalWrite(PIN_FAN, LOW);  
      fan_state = false;
    }
    fan_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, fan_state);
  }
}

// control pump via display printh 23 02 54 02
void trigger2() {
  if (isAuto_state == false) {
    if (pump_state == false) {
      myNex.writeNum("b_pump.bco", 2016); 
      myNex.writeStr("b_pump.txt", "ON");  
      Serial.println("PUMP: ON");
      digitalWrite(PIN_PUMP, HIGH);  
      pump_state = true;

    } else {
      myNex.writeNum("b_pump.bco", 63488); 
      myNex.writeStr("b_pump.txt", "OFF");
      Serial.println("PUMP: OFF");
      digitalWrite(PIN_PUMP, LOW);  
      pump_state = false;
    }
    pump_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, pump_state);
  }
}

// control curtain via display printh 23 02 54 03
void trigger3() {
  if (isAuto_state == false) {
    if (curtain_state == false) {
      myNex.writeNum("b_curtain.bco", 2016); 
      myNex.writeStr("b_curtain.txt", "ON");  
      Serial.println("curtain: ON");
      digitalWrite(PIN_CURTAIN_OPEN, HIGH);
      digitalWrite(PIN_CURTAIN_CLOSE, LOW);
      delay(9000);
      digitalWrite(PIN_CURTAIN_OPEN, LOW);
      digitalWrite(PIN_CURTAIN_CLOSE, LOW);
      curtain_state = true;

    } else {
      myNex.writeNum("b_curtain.bco", 63488); 
      myNex.writeStr("b_curtain.txt", "OFF");
      Serial.println("curtain: OFF");
      digitalWrite(PIN_CURTAIN_OPEN, LOW);
      digitalWrite(PIN_CURTAIN_CLOSE, HIGH);
      delay(9000);
      digitalWrite(PIN_CURTAIN_OPEN, LOW);
      digitalWrite(PIN_CURTAIN_CLOSE, LOW);
      curtain_state = false;
    }
    curtain_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, curtain_state);
  }
}

// control auto via display printh 23 02 54 04
void trigger4() {
  if (isAuto_state == true) {
    isAuto_state = false;
    myNex.writeNum("b_auto.bco", 63488); 
    myNex.writeStr("b_auto.txt", "OFF");
    Serial.println("AUTO: OFF");
  } else {
    isAuto_state = true;
    myNex.writeNum("b_auto.bco", 2016); 
    myNex.writeStr("b_auto.txt", "ON");
    Serial.println("AUTO: ON");
  }
  auto_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, isAuto_state);
}

// control alert via display printh 23 02 54 05
void trigger5() {
  if (isAlert_state == true) {
    isAlert_state = false;
    myNex.writeNum("b_alert.bco", 63488); 
    myNex.writeStr("b_alert.txt", "OFF");
    Serial.println("ALERT: OFF");
  } else {
    isAlert_state = true;
    myNex.writeNum("b_alert.bco", 2016); 
    myNex.writeStr("b_alert.txt", "ON");
    Serial.println("ALERT: ON");
  }
  alert_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, isAlert_state);
}

// control lamp slider via display printh 23 02 54 06
void trigger6() {
  brightness_auto_value = myNex.readNumber("h_lamp.val"); 
  if (brightness_auto_value != 777777) { 
    Serial.print("brightness_auto_value: ");
    Serial.println(brightness_auto_value);
  }
  myNex.writeNum("n_lamp.val", brightness_auto_value);
  lamp_switch.updateAndReportParam("Level_lamp", brightness_auto_value);
}

// control pump slider via display printh 23 02 54 07
void trigger7() {
  moisture_auto_value = myNex.readNumber("h_pump.val"); 
  if (moisture_auto_value != 777777) { 
    Serial.print("moisture_auto_value: ");
    Serial.println(moisture_auto_value);
  }
  myNex.writeNum("n_pump.val", moisture_auto_value);
  pump_switch.updateAndReportParam("Level_pump", moisture_auto_value);
}

// control fan slider via display printh 23 02 54 08
void trigger8() {
  temperature_auto_value = myNex.readNumber("h_fan.val"); 
  if (temperature_auto_value != 777777) { 
    Serial.print("temperature_auto_value: ");
    Serial.println(temperature_auto_value);
  }
  myNex.writeNum("n_fan.val", temperature_auto_value);
  fan_switch.updateAndReportParam("Level_fan", temperature_auto_value);
}

void updateDisplay() {
  if (lamp_state) {
    myNex.writeNum("b_lamp.bco", 2016); 
    myNex.writeStr("b_lamp.txt", "ON");  
  }
  else {
    myNex.writeNum("b_lamp.bco", 63488); 
    myNex.writeStr("b_lamp.txt", "OFF");
  }

  if (pump_state) {
    myNex.writeNum("b_pump.bco", 2016); 
    myNex.writeStr("b_pump.txt", "ON"); 
  }
  else {
    myNex.writeNum("b_pump.bco", 63488); 
    myNex.writeStr("b_pump.txt", "OFF");
  }

  if (fan_state) {
    myNex.writeNum("b_fan.bco", 2016); 
    myNex.writeStr("b_fan.txt", "ON"); 
  }
  else {
    myNex.writeNum("b_fan.bco", 63488); 
    myNex.writeStr("b_fan.txt", "OFF");
  }

  if (curtain_state) {
    myNex.writeNum("b_curtain.bco", 2016); 
    myNex.writeStr("b_curtain.txt", "ON");
  }
  else {
    myNex.writeNum("b_curtain.bco", 63488); 
    myNex.writeStr("b_curtain.txt", "OFF");
  }

  if (isAuto_state) {
    myNex.writeNum("b_auto.bco", 2016); 
    myNex.writeStr("b_auto.txt", "ON");
  }
  else {
    myNex.writeNum("b_auto.bco", 63488); 
    myNex.writeStr("b_auto.txt", "OFF");
  }

  if (isAlert_state) {
    myNex.writeNum("b_alert.bco", 2016); 
    myNex.writeStr("b_alert.txt", "ON");
  }
  else {
    myNex.writeNum("b_alert.bco", 63488); 
    myNex.writeStr("b_alert.txt", "OFF");
  }

  myNex.writeNum("n_lamp.val", brightness_auto_value);
  myNex.writeNum("h_lamp.val", brightness_auto_value);
  myNex.writeNum("n_fan.val", temperature_auto_value);
  myNex.writeNum("h_fan.val", temperature_auto_value);
  myNex.writeNum("n_pump.val", moisture_auto_value);
  myNex.writeNum("h_pump.val", moisture_auto_value);

  myNex.writeNum("x_humi.val", humidity_sensorValue * 10);
  myNex.writeNum("x_temp.val", temperature_sensorValue * 10);
  myNex.writeNum("x_mois.val", moisture_sensorValue * 10);
  myNex.writeNum("x_bright.val", brightness_sensorValue * 10);
}

void trigger10() {
  myNex.writeStr("t_alert1.txt", "");
  myNex.writeStr("t_alert2.txt", "");
}

void setup() {
  myNex.begin(9600);
  Serial.begin(115200);

  // Configure the input GPIOs
  pinMode(gpio_reset, INPUT);
  pinMode(PIN_LAMP, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  pinMode(PIN_CURTAIN_OPEN, OUTPUT);
  pinMode(PIN_CURTAIN_CLOSE, OUTPUT);
  digitalWrite(PIN_CURTAIN_OPEN, LOW);
  digitalWrite(PIN_CURTAIN_CLOSE, LOW);

  digitalWrite(PIN_LAMP, DEFAULT_RELAY_MODE);
  digitalWrite(PIN_PUMP, DEFAULT_RELAY_MODE);
  digitalWrite(PIN_FAN, DEFAULT_RELAY_MODE);

  //Beginning Sensor
  dht.begin();

  //------------------------------------------- Declaring Node -----------------------------------------------------//
  Node my_node;
  my_node = RMaker.initNode("SMART GARDEN");

  Param level_param_lamp("Level_lamp", "custom.param.level", value(DEFAULT_DIMMER_LEVEL), PROP_FLAG_READ | PROP_FLAG_WRITE);
  Param level_param_pump("Level_pump", "custom.param.level", value(DEFAULT_DIMMER_LEVEL), PROP_FLAG_READ | PROP_FLAG_WRITE);
  Param level_param_fan("Level_fan", "custom.param.level", value(DEFAULT_DIMMER_LEVEL), PROP_FLAG_READ | PROP_FLAG_WRITE);
  Param level_param_curtain("Level_curtain", "custom.param.level", value(DEFAULT_DIMMER_LEVEL), PROP_FLAG_READ | PROP_FLAG_WRITE);
  level_param_lamp.addBounds(value(0), value(100), value(1));
  level_param_pump.addBounds(value(0), value(100), value(1));
  level_param_fan.addBounds(value(0), value(100), value(1));
  level_param_curtain.addBounds(value(0), value(100), value(1));
  level_param_lamp.addUIType(ESP_RMAKER_UI_SLIDER);
  level_param_pump.addUIType(ESP_RMAKER_UI_SLIDER);
  level_param_fan.addUIType(ESP_RMAKER_UI_SLIDER);
  level_param_curtain.addUIType(ESP_RMAKER_UI_SLIDER);

  //Standard switch device
  lamp_switch.addCb(write_callback);
  pump_switch.addCb(write_callback);
  fan_switch.addCb(write_callback);
  auto_switch.addCb(write_callback);
  alert_switch.addCb(write_callback);
  curtain_switch.addCb(write_callback);

  lamp_switch.addParam(level_param_lamp);
  pump_switch.addParam(level_param_pump);
  fan_switch.addParam(level_param_fan);
  curtain_switch.addParam(level_param_curtain);
  //------------------------------------------- Adding Devices in Node -----------------------------------------------------//
  my_node.addDevice(temperature);
  my_node.addDevice(humidity);
  my_node.addDevice(moisture);
  my_node.addDevice(brightness);
  my_node.addDevice(lamp_switch);
  my_node.addDevice(pump_switch);
  my_node.addDevice(fan_switch);
  my_node.addDevice(auto_switch);
  my_node.addDevice(alert_switch);
  my_node.addDevice(curtain_switch);


  //This is optional
  RMaker.enableOTA(OTA_USING_PARAMS);
  //If you want to enable scheduling, set time zone for your region using setTimeZone().
  //The list of available values are provided here https://rainmaker.espressif.com/docs/time-service.html
  // RMaker.setTimeZone("Asia/Shanghai");
  // Alternatively, enable the Timezone service and let the phone apps set the appropriate timezone
  RMaker.enableTZService();
  RMaker.enableSchedule();

  Serial.printf("\nStarting ESP-RainMaker\n");
  RMaker.start();
  Serial.printf("\nStarting ESP-RainMaker 111\n");

  // Timer for Sending Sensor's Data
  Timer.setInterval(5000);

  WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
#else
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
#endif

}


void loop() {
  if (Timer.isReady() && wifi_connected) {                    // Check is ready a second timer   
    Serial.println("Sending Data.....");
    Send_Sensor();
    autoControl();
    autoAlert();
    myNex.NextionListen();
    GET_weather(); 
    sendWeatherDisplay();
    updateDisplay();
    // delay(5000);
    Timer.reset();                        // Reset a second timer
  }

  //-----------------------------------------------------------  Logic to Reset RainMaker

  // Read GPIO0 (external button to reset device
  if (digitalRead(gpio_reset) == LOW) { //Push button pressed
    Serial.printf("Reset Button Pressed!\n");
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(gpio_reset) == LOW) delay(50);
    int endTime = millis();

    if ((endTime - startTime) > 10000) {
      // If key pressed for more than 10secs, reset all
      Serial.printf("Reset to factory.\n");
      wifi_connected = 0;
      RMakerFactoryReset(2);
    } else if ((endTime - startTime) > 3000) {
      Serial.printf("Reset Wi-Fi.\n");
      wifi_connected = 0;
      // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
      RMakerWiFiReset(2);
    }
  }
  delay(100);
}
