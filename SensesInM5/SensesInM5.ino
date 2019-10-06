//Senses Platform in M5Stack
//Display + Send Heartbeat pulse data & Temperature probe data
#define USE_ARDUINO_INTERRUPTS false

#include <OneWire.h>
#include <DallasTemperature.h>
#include <PulseSensorPlayground.h>
#include <M5Stack.h>
#include "Senses_wifi_esp32.h"

#define PULSE_INPUT 36
#define SAMPLES_PER_SERIAL_SAMPLE 10
#define THRESHOLD 550
#define TEMP_BUS G26

#define TIME_SLOT 2

#define SENSES_DELAY 5000
#define SENSES_TEMP_SLOT 1
#define SENSES_BMP_SLOT 2

#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define BASE_TEXT_WIDTH 5
#define BASE_TEXT_SPACE 1
#define BASE_TEXT_HEIGHT 7

#define BAR_LERP 0.5f

#define strPWidth(str, m) (str.length() * (BASE_TEXT_WIDTH * m) + (str.length() - 1) * BASE_TEXT_SPACE)
#define lerp(t, a, b) (a * (1 - t) + b * t)

PulseSensorPlayground pulseSensor;
OneWire oneWire(TEMP_BUS);
DallasTemperature sensors(&oneWire);

byte samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
int pulseBPM = 72;
float tempVal = 20;

int slotth;
unsigned long pms, ms, dms;
unsigned long timer[TIME_SLOT];

const char *ssid = "your_wifi_ssid";
const char *passw = "your_wifi_password";
const char *userid = "your_senses_id";
const char *key = "your_device_key";

Senses_wifi_esp32 myiot;
String response;

//temp val for draw lcd
int posX, posY, strw;
int tempBarPos[] = {-1, -1};
int bpmBarPos[] = {-1, -1};
float tempFrac, bpmFrac, barFrac;

//Read Heartbeat periodically
void taskReadBeat(void * pvParameters) {
  pulseSensor.analogInput(PULSE_INPUT);
  pulseSensor.blinkOnPulse(0);
  pulseSensor.fadeOnPulse(0);

  pulseSensor.setSerial(Serial);
  pulseSensor.setOutputType(SERIAL_PLOTTER);
  pulseSensor.setThreshold(THRESHOLD);

  while(!pulseSensor.begin()) { delay(50); }

  while(true) {
    if (pulseSensor.sawNewSample()) {
      if (--samplesUntilReport == (byte) 0) {
        samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
        pulseBPM = constrain(pulseSensor.getBeatsPerMinute(), 0, 200);
      }
    }
    delay(2);
  }
}
//Read Tempurture periodically
void taskReadTemp(void * pvParameters) {
  sensors.begin();
  while(true) {
    sensors.requestTemperatures();
    tempVal = constrain(sensors.getTempCByIndex(0), -20, 150);
    delay(1000);
  }
}
//Send data to Senses Platform periodically
void taskSensesSend(void * pvParameters) {
  response = myiot.connect(ssid, passw, userid, key);
  while(true) {
    myiot.send(SENSES_TEMP_SLOT, tempVal);
    myiot.send(SENSES_BMP_SLOT, pulseBPM);
    delay(SENSES_DELAY);
  }
}
//Tick timers
void tickTimers() {
  pms = ms; ms = millis();
  if(pms < ms) { dms = ms - pms; }
  else { dms = ULONG_MAX - pms + ms; }
  for(slotth = 0; slotth < TIME_SLOT; slotth++) { timer[slotth] += dms; }
}
//Draw Texts
void drawValueTexts() {
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  posY = LCD_HEIGHT - (25 + BASE_TEXT_HEIGHT * 5);
  
  //Draw text Temp
  response = String((int) tempVal);
  strw = strPWidth(response, 3);
  posX = (LCD_WIDTH - 2 * strw) / 4;
  M5.Lcd.fillRect(0, posY, (LCD_WIDTH / 2) - 1, BASE_TEXT_HEIGHT * 3, BLACK);
  M5.Lcd.setCursor(posX, posY);
  M5.Lcd.println(response);
      
  //Draw text BPM
  response = String(pulseBPM);
  strw = strPWidth(response, 3);
  posX = (3 * LCD_WIDTH - 2 * strw) / 4;
  M5.Lcd.fillRect(LCD_WIDTH / 2, posY, (LCD_WIDTH / 2) - 1, BASE_TEXT_HEIGHT * 3, BLACK);
  M5.Lcd.setCursor(posX, posY);
  M5.Lcd.print(response);
    
  M5.Lcd.drawFastVLine(LCD_WIDTH / 2, 0, LCD_HEIGHT, WHITE);
}
//Draw Caption Texts
void drawCaptionTexts() {
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  
  posY = LCD_HEIGHT - 30;

  response = String(F("Temp."));
  strw = strPWidth(response, 2);
  M5.Lcd.fillRect(0, posY, (LCD_WIDTH / 2) - 1, BASE_TEXT_HEIGHT * 2, BLACK);
  M5.Lcd.setCursor((LCD_WIDTH - 2 * strw) / 4, posY);
  M5.Lcd.print(response);
  
  response = String(F("BPM."));
  strw = strPWidth(response, 2);
  M5.Lcd.fillRect(LCD_WIDTH / 2, posY, (LCD_WIDTH / 2) - 1, BASE_TEXT_HEIGHT * 2, BLACK);
  M5.Lcd.setCursor((3 * LCD_WIDTH - 2 * strw) / 4, posY);
  M5.Lcd.print(response);
}
//Draw Bar Temp
void drawBarTemp() {
  posY = LCD_HEIGHT - (25 + BASE_TEXT_HEIGHT * 5) - 10;
  
  tempBarPos[0] = (tempBarPos[0] == -1) ? posY : tempBarPos[1];
  tempFrac = (tempVal + 20) / 170.0f;
  tempBarPos[1] = posY - (int) (tempFrac * (posY - 20)) ;
  tempBarPos[1] = lerp(BAR_LERP, tempBarPos[0], tempBarPos[1]);
    
  if(tempBarPos[0] > tempBarPos[1]) { //fill lines
    while(tempBarPos[0] >= tempBarPos[1]) {
      barFrac = (float)(tempBarPos[0] - 20) / (posY - 20);
      M5.Lcd.fillRect(LCD_WIDTH / 6, tempBarPos[0], LCD_WIDTH / 6, 1, M5.Lcd.color565(255 * (1 - barFrac), 0, 255 * barFrac));
      tempBarPos[0]--;
    }
  } else { //erase lines
    while(tempBarPos[0] <= tempBarPos[1]) {
      M5.Lcd.fillRect(LCD_WIDTH / 6, tempBarPos[0], LCD_WIDTH / 6, 1, BLACK);
      tempBarPos[0]++;
    }
  }
}
//draw Bar BPM
void drawBarBPM() {
  posY = LCD_HEIGHT - (25 + BASE_TEXT_HEIGHT * 5) - 10;
  
  bpmBarPos[0] = (bpmBarPos[0] == -1) ? posY : bpmBarPos[1];
  bpmFrac = pulseBPM / 200.0f;
  bpmBarPos[1] = posY - (int) (bpmFrac * (posY - 20)) ;
  bpmBarPos[1] = lerp(BAR_LERP, bpmBarPos[0], bpmBarPos[1]);
  
  if(bpmBarPos[0] > bpmBarPos[1]) { //fill lines
    while(bpmBarPos[0] >= bpmBarPos[1]) {
      barFrac = (float)(bpmBarPos[0] - 20) / (posY - 20);
      M5.Lcd.fillRect(LCD_WIDTH * 4 / 6, bpmBarPos[0], LCD_WIDTH / 6, 1, M5.Lcd.color565(255 * (1 - barFrac), 0, 255 * barFrac));
      bpmBarPos[0]--;
    }
  } else { //erase lines
    while(bpmBarPos[0] <= bpmBarPos[1]) {
      M5.Lcd.fillRect(LCD_WIDTH * 4 / 6, bpmBarPos[0], LCD_WIDTH / 6, 1, BLACK);
      bpmBarPos[0]++;
    }
  }
}

void setup() {
  Serial.begin(115200);
  ms = millis();
  M5.begin(true, false, false);
  Serial.println(F("Senses Platform in M5Stack"));
  
  xTaskCreatePinnedToCore(taskReadBeat, "taskReadBeat", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskReadTemp, "taskReadTemp", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(taskSensesSend, "taskSensesSend", 4096, NULL, 3, NULL, 0);
  
  M5.Power.begin();
  
  M5.Lcd.clear(BLACK);
  drawCaptionTexts();
  drawValueTexts();
  drawBarTemp();    
  drawBarBPM();
}

void loop() {
  tickTimers();
  M5.update();
  if(timer[0] >= 500) {
    timer[0] = 0;
    drawCaptionTexts();
    drawValueTexts();
  }
  if(timer[1] >= 100) {
    timer[1] = 0;
    drawBarTemp();    
    drawBarBPM();
  }
  delay(1);
}
