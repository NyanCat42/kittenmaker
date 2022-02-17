#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <AccelStepper.h>
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

#define ssid        // WiFi SSID
#define password    // WiFi password
#define FS_NO_GLOBALS
#define SD_CS   5
#define HALFSTEP 8


TFT_eSPI tft = TFT_eSPI();
AccelStepper stepper1(HALFSTEP, 27, 12, 14, 13);

int layerheight = 100;
int exposure_time = 15;
int exposure_time_print;
int stepper_speed = 1500;
int stepper_accl = 1500;
int rise_height = 5;
int steps_per_layer;
float steps_per_um = 5.094716;
int stepperlayer = 0;
int layer = 1;
bool print_started = false;
bool calibrating = false;
bool raising = false;
int file_count = 0;
int rise_height_steps = 0;
bool not_home = 1;


WebServer server ( 80 );

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

String getHTML(){
  String html ="<!DOCTYPE html> <html> <head> <meta charset=\"UTF-8\" /> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" /> <meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\" /> <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.0.13/css/all.css\" integrity=\"sha384-DNOHZ68U8hZfKXOrtjWvjxusGo9WQnrNx2sqG0tfsghAvtVlRW3tvkXWZh58N9jp\" crossorigin=\"anonymous\" /> <link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css\" integrity=\"sha384-WskhaSGFgHYWDcbwN70/dfYBj47jz9qbsMId/iRN3ewGhXQFZCSftd1LZCfmhktB\" crossorigin=\"anonymous\" /> <link rel=\"stylesheet\" href=\"https://drive.google.com/uc?export=view&id=1mhw1DqORWk2bltFuDr9H43TA1usdWe1Y\" /> <title>kittenmaker v0.2</title> </head> <body> <div class=\"container-fluid\"> <div class=\"row\"> <div class=\"col-md-4\"> </div> <div class=\"col-md-4\"> <h6> &zwnj; </h6> <h2 class=\"text-center\"> kittenmaker 0.2.0 </h2> </div> <div class=\"col-md-4\"> </div> </div> <div class=\"row\"> <div class=\"col-md-4\"> </div> <div class=\"col-md-4\"> <h3> &zwnj; </h3> <form action='/' method='POST'><button type='button submit' name='P' class='btn btn-success btn-block btn-lg mt-1 mb-1'>Print</button></form> <h3> &zwnj; </h3> <h3 class=\"text-center\"> Layerheight: ";
  html += layerheight;
  html +="μm </h3> <div class=\"row\"> <div class=\"col-md-6\"> <form action='/' method='POST'><button type='button submit' name='LD' class='btn btn-primary btn-block mt-1 mb-1'>lower</button></form></div> <div class=\"col-md-6\"> <form action='/' method='POST'><button type='button submit' name='LU' class='btn btn-primary btn-block mt-1 mb-1'>raise</button></form></div> </div> <h3 class=\"text-center\"> Exposure time: ";
  html += exposure_time;
  html +="s </h3> <div class=\"row\"> <div class=\"col-md-6\"> <form action='/' method='POST'><button type='button submit' name='TD' class='btn btn-primary btn-block mt-1 mb-1'>lower</button></form></div> <div class=\"col-md-6\"> <form action='/' method='POST'><button type='button submit' name='TU' class='btn btn-primary btn-block mt-1 mb-1'>raise</button></form></div> </div> </div> <div class=\"col-md-4\"> </div> </div> <div class=\"row\"> <div class=\"col-md-4\"> </div> <div class=\"col-md-4\"> <h1> &zwnj; </h1> <h3 class=\"text-muted text-center\"> Stepper speed, acceleration: </h3> <div class=\"row\"> <div class=\"col-md-6\"> <h3 class=\"text-muted text-center\"> ";
  html += stepper_speed;
  html +="mm/s </h3> </div> <div class=\"col-md-6\"> <h3 class=\"text-muted text-center\"> ";
  html += stepper_accl;
  html +="mm/ss </h3> </div> </div> <div class=\"row\"> <div class=\"col-md-3\"> <form action='/' method='POST'><button type='button submit' name='SD' class='btn btn-secondary btn-block mt-1 mb-1'>lower</button></form></div> <div class=\"col-md-3\"> <form action='/' method='POST'><button type='button submit' name='SU' class='btn btn-secondary btn-block mt-1 mb-1'>raise</button></form></div> <div class=\"col-md-3\"> <form action='/' method='POST'><button type='button submit' name='AD' class='btn btn-secondary btn-block mt-1 mb-1'>lower</button></form></div> <div class=\"col-md-3\"> <form action='/' method='POST'><button type='button submit' name='AU' class='btn btn-secondary btn-block mt-1 mb-1'>raise</button></form></div> </div> <h3 class=\"text-muted text-center\"> Rise height: ";
  html += rise_height;
  html +="mm </h3> <div class=\"row\"> <div class=\"col-md-6\"> <form action='/' method='POST'><button type='button submit' name='RD' class='btn btn-secondary btn-block mt-1 mb-1'>lower</button></form></div> <div class=\"col-md-6\"> <form action='/' method='POST'><button type='button submit' name='RU' class='btn btn-secondary btn-block mt-1 mb-1'>raise</button></form></div> </div> <h3> &zwnj; </h3> <form action='/' method='POST'><button type='button submit' name='CX' class='btn btn-warning btn-block mt-1 mb-1'>calibrate</button></form></div> <div class=\"col-md-4\"> </div> </div> </div>";
return html;
}

void handle_print() {
  print_started = true;
} 
void handle_layerheight_up() {
  layerheight = layerheight + 25;
} 
void handle_layerheight_down() {
  layerheight = layerheight - 25;
} 
void handle_exposure_time_up() {
  exposure_time = exposure_time + 5;
} 
void handle_exposure_time_down() {
  exposure_time = exposure_time - 5;
} 
void handle_stepper_speed_up() {
  stepper_speed = stepper_speed + 100;
} 
void handle_stepper_speed_down() {
  stepper_speed = stepper_speed - 100;
} 
void handle_stepper_accl_up() {
  stepper_accl = stepper_accl + 100;
} 
void handle_stepper_accl_down() {
  stepper_accl = stepper_accl - 100;
} 
void handle_rise_height_up() {
  rise_height = rise_height + 1;
} 
void handle_rise_height_down() {
  rise_height = rise_height - 1;
} 
void handle_calibration() {
  if(not_home == true){
    calibrating = true;
  }else if(not_home == false){
    raising = true;
  }
}


void handleRoot(){ 
    if ( server.hasArg("P") ) {
    handle_print();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("LU") ) {
    handle_layerheight_up();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("LD") ) {
    handle_layerheight_down();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("TU") ) {
    handle_exposure_time_up();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("TD") ) {
    handle_exposure_time_down();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("SU") ) {
    handle_stepper_speed_up();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("SD") ) {
    handle_stepper_speed_down();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("AU") ) {
    handle_stepper_accl_up();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("AD") ) {
    handle_stepper_accl_down();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("RU") ) {
    handle_rise_height_up();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("RD") ) {
    handle_rise_height_down();
    server.send ( 200, "text/html", getHTML() );
  } 
  else if ( server.hasArg("CX") ) {
    handle_calibration();
    server.send ( 200, "text/html", getHTML() );
  } 
  else {
    server.send ( 200, "text/html", getHTML() );
  }  
}



void setup() {
  
  pinMode(25, INPUT_PULLUP); // homing switch
  pinMode(26, OUTPUT);       // uv led driver (high is off)
  digitalWrite(26, 1);       // turn led off
  
  Serial.begin ( 115200 );
  
  WiFi.mode(WIFI_STA);
  WiFi.begin ( ssid, password );
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 ); Serial.print ( "." );
  }
  Serial.println ( "IP address: " ); Serial.println ( WiFi.localIP() );
 
  server.on ( "/", handleRoot );
  server.begin();
  Serial.println ( "HTTP server started" );

  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD.begin failed!"));
    while (1) delay(0);
  }

  tft.begin();
  tft.setTextColor(0xFFFF, 0x0000);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  tft.setRotation(1);
  TJpgDec.setCallback(tft_output);

}

void loop() {
  
  stepper1.setAcceleration(stepper_accl);
  stepper1.setSpeed(stepper_speed);
  stepper1.setMaxSpeed(stepper_speed);
  
  server.handleClient(); 

  not_home = digitalRead(25);

  if(raising == true){
    
    stepper1.moveTo(stepper1.currentPosition() + steps_per_um * 20000);

    server.handleClient();

    do {
     stepper1.run();
    } while (stepper1.distanceToGo() != 0);

    raising = false;
  }

  if(calibrating == true){

    stepper1.moveTo(stepper1.currentPosition() - steps_per_um * 100000);

    server.handleClient();

    do {
     stepper1.run();
     not_home = digitalRead(25);
    } while (not_home == 1 && stepper1.distanceToGo() != 0);

    not_home = digitalRead(25);

    if(not_home == 0){
    calibrating = false;
    stepper1.moveTo(0);
    stepper1.setCurrentPosition(0);
    }

    }

  if(print_started == true){

    server.handleClient();

    String layerstr = String(layer);
    TJpgDec.drawSdJpg(0, 0, "/"+layerstr+".jpg");


    if(layer < 3){
      exposure_time_print = exposure_time * 4;
    }else {
      exposure_time_print = exposure_time;
    }

    digitalWrite(26, 0);
    delay(exposure_time_print*1000);
    digitalWrite(26, 1);

    steps_per_layer = steps_per_um * layerheight + 0.5;
    rise_height_steps = rise_height * 1000 * steps_per_um;

    stepper1.moveTo(stepper1.currentPosition() + steps_per_layer + rise_height_steps);

    do {
      stepper1.run();
    } while (stepper1.distanceToGo() != 0);

    stepper1.moveTo(stepper1.currentPosition()-rise_height_steps);

    do {
      stepper1.run();
    } while (stepper1.distanceToGo() != 0);


    layer = layer + 1;
    stepperlayer = stepperlayer + 1;
      
    File image;
    image = SD.open("/"+layerstr+".jpg");
    Serial.println ( image.name() ); 
      
    if (! image) {
      print_started = false;
      tft.fillScreen(TFT_BLACK);
      layer = 1;
      stepperlayer = 0;
    }

    image.close();

      
   }

}