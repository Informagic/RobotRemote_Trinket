#include <Adafruit_RGBLCDShield.h>
#include <Adafruit_MCP23017.h>
#include <SoftwareSerial.h>
#include <Wire.h>

// XBee's DOUT (TX) is connected to pin 10 (Arduino's Software RX)
// XBee's DIN (RX) is connected to pin 11 (Arduino's Software TX)
SoftwareSerial XBee(10, 11); // RX, TX

bool debug = true;
char* lcdDebugLine = new char[16];

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

int button1pin = 3;  // A
int button2pin = 4;  // B
int button3pin = 5;  // C
int button4pin = 6;  // D

int joy1Xpin = A0;   // high: left, low: right
int joy1Ypin = A1;   // high: up, low: down
int joy2Xpin = A2;   // high: R1, low: R2
int joy2Ypin = A3;   // throttle

int button1status = 0;
int button2status = 0;
int button3status = 0;
int button4status = 0;

int joy1Xstatus = 0;
int joy1Ystatus = 0;
int buttons56status = 0;
int throttleStatus = 0;

int joy1Xstatus_min, joy1Xstatus_mid, joy1Xstatus_max;
int joy1Ystatus_min, joy1Ystatus_mid, joy1Ystatus_max;
int buttons56status_mid;
int throttleStatus_old, throttleStatus_min, throttleStatus_max;

int tmp;
byte cmd[4];
bool sendChange = false;

int button1, button2, button3, button4, button5, button6,
joy1X, joy1Y, throttle;

void setup()
{
  pinMode(button1pin, INPUT);
  pinMode(button2pin, INPUT);
  pinMode(button3pin, INPUT);
  pinMode(button4pin, INPUT);

  pinMode(joy1Xpin, INPUT);
  pinMode(joy1Ypin, INPUT);
  pinMode(joy2Xpin, INPUT);
  pinMode(joy2Ypin, INPUT);

  XBee.begin(115200);

  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
}

void loop()
{
  // calibration of joystick axes
  while (millis() <= 1000) {
    lcd.setBacklight(TEAL);
    lcd.setCursor(0, 0);
    lcd.print("Reading defaults");
    lcd.setCursor(0, 1);
    lcd.print(millis() / 1000);

    joy1Xstatus_mid = analogRead(joy1Xpin);
    joy1Xstatus_min = joy1Xstatus_max = joy1Xstatus_min;
    joy1Ystatus_mid = analogRead(joy1Ypin);
    joy1Ystatus_min = joy1Ystatus_max = joy1Ystatus_min;
    buttons56status_mid = analogRead(joy2Xpin);
    throttleStatus_old = analogRead(joy2Ypin);
    throttleStatus_min = throttleStatus_max = throttleStatus_old;
  }
  while (millis() > 1000 && millis() <= 10000) {
    lcd.setBacklight(TEAL);
    lcd.setCursor(0, 0);
    lcd.print("Please move axes");

    lcd.setCursor(0, 1);
    lcd.print(millis() / 1000);
    tmp = analogRead(joy1Xpin);
    if (tmp > joy1Xstatus_max)
      joy1Xstatus_max = tmp;
    if (tmp < joy1Xstatus_min)
      joy1Xstatus_min = tmp;

    tmp = analogRead(joy1Ypin);
    if (tmp > joy1Ystatus_max)
      joy1Ystatus_max = tmp;
    if (tmp < joy1Ystatus_min)
      joy1Ystatus_min = tmp;

    tmp = analogRead(joy2Ypin);
    if (tmp > throttleStatus_max)
      throttleStatus_max = tmp;
    if (tmp < throttleStatus_min)
      throttleStatus_min = tmp;
  }
  
  /*if (millis() > 10000) {
    lcd.clear();
    lcd.setBacklight(TEAL);
    lcd.setCursor(0, 0);
    lcd.print("Start Playing");
  }*/

  button1status = digitalRead(button1pin);
  button2status = digitalRead(button2pin);
  button3status = digitalRead(button3pin);
  button4status = digitalRead(button4pin);

  joy1Xstatus = analogRead(joy1Xpin);
  joy1Ystatus = analogRead(joy1Ypin);
  buttons56status = analogRead(joy2Xpin);
  throttleStatus = analogRead(joy2Ypin);

  if (button1status == 0) {
    cmd[0] += 1;
    sendChange = true;
  }
  if (button2status == 0) {
    cmd[0] += 2;
    sendChange = true;
  }
  if (button3status == 0) {
    cmd[0] += 4;
    sendChange = true;
  }
  if (button4status == 0) {
    cmd[0] += 8;
    sendChange = true;
  }
  
  if (abs(joy1Xstatus - joy1Xstatus_mid) >= 25) {
    joy1Xstatus = map(joy1Xstatus, joy1Xstatus_min, joy1Xstatus_max, 0, 255);
    cmd[1] += joy1Xstatus;
    sendChange = true;
  } else {
    cmd[1] += map(joy1Xstatus_mid, joy1Xstatus_min, joy1Xstatus_max, 0, 255);
  }

  if (abs(joy1Ystatus - joy1Ystatus_mid) >= 25) {
    joy1Ystatus = map(joy1Ystatus, joy1Ystatus_min, joy1Ystatus_max, 0, 255);
    cmd[2] += joy1Ystatus;
    sendChange = true;
  } else {
    cmd[2] += map(joy1Ystatus_mid, joy1Ystatus_min, joy1Ystatus_max, 0, 255);
  }

  if (abs(buttons56status - buttons56status_mid) >= 10) {
    if (buttons56status > buttons56status_mid)
      cmd[0] += 16;
    else
      cmd[0] += 32;
    sendChange = true;
  }

  if (abs(throttleStatus - throttleStatus_old) >= 10) {
    sendChange = true;
    throttleStatus_old = throttleStatus;
  }
  throttleStatus = map(throttleStatus, throttleStatus_min, throttleStatus_max, 0, 255);
  throttleStatus = constrain(throttleStatus, 0, 255); // in case the user forgot calibration
  cmd[3] += throttleStatus;

  if (sendChange) {
    XBee.write(cmd, 4);
  }

  if (debug) {
    lcd.clear();
    // let's do the backward computation, just to see that things are working
    sprintf(lcdDebugLine, "A%dB%dC%dD%dR1%dR2%d", cmd[0] & 1, (cmd[0] & 2) >> 1, (cmd[0] & 4) >> 2, (cmd[0] & 8) >> 3,
                                                  (cmd[0] & 16) >> 4, (cmd[0] & 32) >> 5);

    lcd.setCursor(0, 0);
    lcd.print(lcdDebugLine);
  }

  cmd[0] = cmd[1] = cmd[2] = cmd[3] = 0;
  sendChange = false;
  delay(100);
}

