#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>


#include <SPI.h>
#include <FS.h>
#include <SD.h>
#define FS_NO_GLOBALS

#include "setup_pins.h"
#include "wifi_ssid_password.h"

#include <GCodeParser.h>
GCodeParser GCode = GCodeParser();

#include <AccelStepper.h>
AccelStepper stepper1(8, stepper_pin_1, stepper_pin_3, stepper_pin_2, stepper_pin_4); // NOTE: The sequence 1-3-2-4 is required for proper sequencing of 28BYJ-48

#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

#define USE_LINE_BUFFER
#include "support_functions.h"

int layerheight = 0;
int exposure_time = 0;
int bottom_exposure_time = 0;
int exposure_time_print;
int stepper_speed = 1000;
int stepper_accl = 1000;
int rise_height = 0;
int steps_per_layer;
float steps_per_um = 1.0189432; // or, to be more precise - 1.018943209876543209876543209876543209876543209876543209876543209876543209876543209876543209876543209876543209876543209876543209877
int layer = 1;
bool print_started = false;
bool calibrating = false;
bool raising = false;
int rise_height_steps = 0;
bool not_home = 1;
String filename;
int bottom_layer_count = 0;
int layercount;
int bottom_rise_height = 0;
int bottom_stepper_speed = 1000;
int layerPercent = 0;
unsigned long layerTimerStart, layerTimerFinish, layerTimerElapsed;
int printTime;
String printTimeHMS = "";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String message = "";
String inputValue1 = String(layerheight);
String inputValue2 = String(exposure_time);
String inputValue3 = String(bottom_exposure_time);
String inputValue4 = String(rise_height);
String inputValue5 = String(bottom_rise_height);

JSONVar inputValues;
String getInputValues(){

  inputValues["inputValue1"] = String(inputValue1);
  inputValues["inputValue2"] = String(inputValue2);
  inputValues["inputValue3"] = String(inputValue3);
  inputValues["inputValue4"] = String(inputValue4);
  inputValues["inputValue5"] = String(inputValue5);
  inputValues["fileName"] = filename;
  inputValues["estimatedTime"] = printTimeHMS;
  inputValues["progressBar"] = String("width: " + String(layerPercent) + "%;");
  String jsonString = JSON.stringify(inputValues);
  return jsonString;
}

void initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
   Serial.println("SPIFFS mounted successfully");
  }
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String inputValues) {
  ws.textAll(inputValues);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    if (message.indexOf("1s") >= 0) {
      inputValue1 = message.substring(2);
      layerheight = inputValue1.toInt();
      notifyClients(getInputValues());
    }
    if (message.indexOf("2s") >= 0) {
      inputValue2 = message.substring(2);
      exposure_time = inputValue2.toInt();
      notifyClients(getInputValues());
    }    
    if (message.indexOf("3s") >= 0) {
      inputValue3 = message.substring(2);
      bottom_exposure_time = inputValue3.toInt();
      notifyClients(getInputValues());
    }
    if (message.indexOf("4s") >= 0) {
      inputValue4 = message.substring(2);
      rise_height = inputValue4.toInt();
      notifyClients(getInputValues());
    }
    if (message.indexOf("5s") >= 0) {
      inputValue5 = message.substring(2);
      bottom_rise_height = inputValue5.toInt();
      notifyClients(getInputValues());
    }
    if (message.indexOf("6s") >= 0) {
      print_started = true;
      notifyClients(getInputValues());
    }
    if (message.indexOf("7s") >= 0) {
      print_started = false;
      calibrating = false;
      raising = false;
      tft.fillScreen(0);
      layer = 1;
      Serial.println("stop");
      notifyClients(getInputValues());
    }
    if (message.indexOf("8s") >= 0) {
      calibrating = true;
      Serial.println("home");
      notifyClients(getInputValues());
    }
    if (message.indexOf("9s") >= 0) {
      raising = true;
      Serial.println("raise");
      notifyClients(getInputValues());
    }
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients(getInputValues());
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void secToHMS(){
  int t, h, m, s;
  t = printTime;
  
  s = t % 60;
  t = (t - s)/60;
  m = t % 60;
  t = (t - m)/60;
  h = t;

  printTimeHMS = "Estimated print time: "+String(h)+":"+String(m)+":"+String(s);

  Serial.println(printTimeHMS);
}

void parseGcode() {
      File gCodeFile = SD.open("/run.gcode");
    if (gCodeFile)
    {
      int settingsLenght = 1;
      while (gCodeFile.available() && settingsLenght <= 27)
      {
        if (GCode.AddCharToLine(gCodeFile.read()))
        {
          GCode.ParseLine();
          GCode.RemoveCommentSeparators();
          if(settingsLenght == 8){
              layerheight = String(GCode.comments).substring(12).toFloat()*1000;
              Serial.println(layerheight);
          }if(settingsLenght == 15){
              exposure_time = String(GCode.comments).substring(19).toInt();
              Serial.println(exposure_time);
          }if(settingsLenght == 17){
              bottom_exposure_time = String(GCode.comments).substring(24).toInt();
              Serial.println(bottom_exposure_time);
          }if(settingsLenght == 19){
              rise_height = String(GCode.comments).substring(22).toInt();
              Serial.println(rise_height);
          }if(settingsLenght == 21){
              stepper_speed = String(GCode.comments).substring(21).toInt();
              Serial.println(stepper_speed);
          }if(settingsLenght == 1){
              filename = String(GCode.comments).substring(9);
              Serial.println(filename);
          }if(settingsLenght == 23){
              bottom_layer_count = String(GCode.comments).substring(17).toInt();
              Serial.println(bottom_layer_count);
          }if(settingsLenght == 25){
              layercount = String(GCode.comments).substring(11).toInt();
              Serial.println(layercount);
          }if(settingsLenght == 26){
              bottom_rise_height = String(GCode.comments).substring(22).toInt();
              Serial.println(bottom_rise_height);
          }if(settingsLenght == 27){
              bottom_stepper_speed = String(GCode.comments).substring(21).toInt();
              Serial.println(bottom_stepper_speed);
          }settingsLenght++;
        }
      }  
    }
  inputValue1 = String(layerheight);
  inputValue2 = String(exposure_time);
  inputValue3 = String(bottom_exposure_time);
  inputValue4 = String(rise_height);
  inputValue5 = String(bottom_rise_height);
  printTime = bottom_layer_count*bottom_rise_height*2+(layercount-bottom_layer_count)*rise_height*2+bottom_layer_count*bottom_exposure_time+(layercount-bottom_layer_count)*exposure_time;
  Serial.println(printTime);
  secToHMS();
}


void setup() {

  pinMode(Home_Switch, INPUT_PULLUP); // homing switch
  pinMode(UV_LED, OUTPUT);       // uv led driver (high is off)
  digitalWrite(UV_LED, 1);       // turn led off

  Serial.begin(115200);

  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD.begin failed!"));
  }

  tft.begin();
  tft.setRotation(1);

  parseGcode();

  initFS();
  initWiFi();
  initWebSocket();
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/preview", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/preview.png", "image/png");
  });
  
  server.serveStatic("/", SPIFFS, "/");

  server.begin();

}

void loop() {

  stepper1.setAcceleration(stepper_accl);
  stepper1.setSpeed(stepper_speed);
  stepper1.setMaxSpeed(stepper_speed);
   
  not_home = digitalRead(Home_Switch);

  if(raising == true){
    Serial.println("raisung");
    stepper1.moveTo(stepper1.currentPosition() + steps_per_um * 20000);
    do {
     stepper1.run();
    } while (stepper1.distanceToGo() != 0 && raising == true);
    raising = false;
  }

  if(calibrating == true){
    Serial.println("calibrating");
    stepper1.moveTo(stepper1.currentPosition() - steps_per_um * 100000);
    do {
     stepper1.run();
     not_home = digitalRead(Home_Switch);
    } while (not_home == 1 && stepper1.distanceToGo() != 0 && calibrating == true);
      not_home = digitalRead(Home_Switch);
    if(not_home == 0){
      calibrating = false;
      stepper1.moveTo(0);
      stepper1.setCurrentPosition(0);
    }
  }

  if(print_started == true){

    Serial.println(printTimeHMS);

    String layerstr = String(layer);
    String layerfilename = "/"+layerstr+".png";
    const char* layerfilenameconst = layerfilename.c_str();
    setPngPosition(0, 0);
    load_file(SD, layerfilenameconst);

    if(layer <= bottom_layer_count){
      exposure_time_print = bottom_exposure_time;
    }else {
      exposure_time_print = exposure_time;
    }

    digitalWrite(UV_LED, 0);
    delay(exposure_time_print*1000);
    digitalWrite(UV_LED, 1);

    steps_per_layer = steps_per_um * layerheight + 0.5;
    if(layer <= bottom_layer_count){
      rise_height_steps = bottom_rise_height * 1000 * steps_per_um;
      stepper1.setSpeed(bottom_stepper_speed);
      stepper1.setMaxSpeed(bottom_stepper_speed);
    }else {
      rise_height_steps = rise_height * 1000 * steps_per_um;
      stepper1.setSpeed(stepper_speed);
      stepper1.setMaxSpeed(stepper_speed);
    }


    layerTimerStart=millis();

    stepper1.moveTo(stepper1.currentPosition() + steps_per_layer + rise_height_steps);

    do {
      stepper1.run();
    } while (stepper1.distanceToGo() != 0);

    stepper1.moveTo(stepper1.currentPosition()-rise_height_steps);

    do {
      stepper1.run();
    } while (stepper1.distanceToGo() != 0);

    layerTimerFinish=millis();
    layerTimerElapsed=layerTimerFinish-layerTimerStart;
    Serial.println(layerTimerElapsed);

    if(layer <= bottom_layer_count){
      printTime = (bottom_layer_count-layer)*layerTimerElapsed/1000+(layercount-bottom_layer_count)*rise_height*(layerTimerElapsed/1000/bottom_rise_height)+(bottom_layer_count-layer)*bottom_exposure_time+(layercount-bottom_layer_count)*exposure_time;
    }else {
      printTime = (layercount-layer)*layerTimerElapsed/1000+(layercount-layer)*exposure_time;
    }
    Serial.println(printTime);
    secToHMS();

    layer = layer + 1;
    layerPercent = layer*100/layercount;
      
    if (layercount < layer) {
      print_started = false;
      tft.fillScreen(0);
      layer = 1;
      raising = true;
    }
     
  }

  ws.cleanupClients();
}
