#include <Adafruit_RGBLCDShield.h>
#include <Adafruit_MCP23017.h>
#include <SoftwareSerial.h>
#include <Wire.h>

/* 
 * TODO: Somehow, [4] is only powered when the USB cable is plugged
 *       in (remember that [2] only has one 5V dedicated power output,
 *       which is why [4] receives it's power from the FTDI power line)
 * 
 * HARDWARE SUMMARY:
 * This code assumes that the following hardware is present:
 *   [1] Saitek X6-33M Gamepad
 *   [2] Adafruit Trinket Pro 5V
 *   [3] XBee adapter and antenna
 *   [4] Adafruit 16x2 LCD multi-color display with Arduino shield
 *   [5] Battery compartment for 4*AA (since [1] originally wasn't
 *       wirelss)
 *   [6] blue power status LED (since [1] originally didn't feature
 *       any visual feedback components)
 *   [7] a couple of pull-up resistors on all Midi channels (this
 *       was, on a Midi port, the job of the soundcard)
 *
 * HARDWARE SETTINGS:
 * We currently assume that the four switches
 *   [1a] R1/R2
 *   [1b] C
 *   [1c] D
 *   [1d] T1/T2
 * are all set in the "Normal" position, i.e., they are neither
 * "Off" nor in "Turbo" mode. In other words, keeping a button
 * pressed will not result in repeatedly firing the command.
 * In addition, we assume that the swtich
 *   [1e] Analog/Digital
 * is set to "Digital", i.e., we use the thumb pad, not the joystick
 * connected to Axis 1. Furthermore, we assume that the switch
 *   [1f] Throttle/T1/T2
 * is set to "Throttle".
 * 
 * IMPORTANT:
 * A) Keep in mind that [1] is following the Midi standard for
 *    game controllers. This means that the many buttons of the
 *    gamepad only work by using the trick of mapping some of the
 *    buttons (digital inputs) to two of the available joystick axes
 *    (analog inputs). Saitek decided to have [1a] as the extremum
 *    values of the horizontal axis of Joystick 2, and [1d] as the
 *    extremum values of its vertical axis. Since [1] also features
 *    a throttle control, not all available hardware inputs can be
 *    used simultaneously. In this case, [1d] and the throttle
 *    share the same axis; the selection is done via [1f].
 * B) A second overload on [1] is the presence of a digital thumb
 *    pad and a joystick, both sharing the analog X and Y axes of
 *    Joystick 1 (Midi standard). By means of [1f], we can choose
 *    whether to use the thumb pad or the joystick. The thumb pad
 *    is a 8-direction one, giving the corresponding extremum
 *    readings of both joystick axes, while the thumb stick gives
 *    analog readings in the full range of both axes.
 */

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
  obtainDefaultAxesValues();
  calibrateAxes();

  // read buttons A, B, C, D
  button1status = digitalRead(button1pin);
  button2status = digitalRead(button2pin);
  button3status = digitalRead(button3pin);
  button4status = digitalRead(button4pin);

  // read the analog axes
  joy1Xstatus = analogRead(joy1Xpin);
  joy1Ystatus = analogRead(joy1Ypin);
  buttons56status = analogRead(joy2Xpin);
  throttleStatus = analogRead(joy2Ypin);

  // because of the pull-up resistors, Buttons A, B, C, D are
  // pressd if they read 0
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
  
  // we allow for some slack before we consider a variation
  // on the joystick axes an intentional movement
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

  // since Buttons 5, 6 are digital on an analog axis, the slack
  // is smaller here
  if (abs(buttons56status - buttons56status_mid) >= 10) {
    if (buttons56status > buttons56status_mid)
      cmd[0] += 16;
    else
      cmd[0] += 32;
    sendChange = true;
  }

  // not sure what makes sense for the sliding throttle control;
  // for now we keep it as if it were digital buttons on the analog
  // axis
  if (abs(throttleStatus - throttleStatus_old) >= 10) {
    sendChange = true;
    throttleStatus_old = throttleStatus;
  }
  throttleStatus = map(throttleStatus, throttleStatus_min, throttleStatus_max, 0, 255);
  throttleStatus = constrain(throttleStatus, 0, 255); // in case the user forgot calibration
  cmd[3] += throttleStatus;

  // we'll only occupy the XBee channel if there was some user input
  if (sendChange) {
    XBee.write(cmd, 4);
  }

  // as long as we've only a serial monitor on the receiving and of the
  // XBee connection, we need to use the RobotRemote's LCD display to
  // show some debugging information
  if (debug) {
    lcd.clear();
    // let's do the backward computation, just to see that things are working
    sprintf(lcdDebugLine, "A%dB%dC%dD%dR1%dR2%d", cmd[0] & 1, (cmd[0] & 2) >> 1, (cmd[0] & 4) >> 2, (cmd[0] & 8) >> 3,
                                                  (cmd[0] & 16) >> 4, (cmd[0] & 32) >> 5);

    lcd.setCursor(0, 0);
    lcd.print(lcdDebugLine);
  }

  // reset all variables before we do another loop
  cmd[0] = cmd[1] = cmd[2] = cmd[3] = 0;
  sendChange = false;
  delay(100);
}

/* During the first second, we obtain the default (i.e., center)
 * values of the various joystick axes. The user isn't supposed
 * to be moving any stick/pressing any button during this time.
 */
void obtainDefaultAxesValues() {
  lcd.setBacklight(TEAL);
  lcd.setCursor(0, 0);
  lcd.print("Reading defaults");

  while (millis() <= 1000) {
    lcd.setCursor(0, 1);
    lcd.print(millis() / 1000);
    
    joy1Xstatus_mid = analogRead(joy1Xpin);
    joy1Xstatus_min = joy1Xstatus_max = joy1Xstatus_mid;
    joy1Ystatus_mid = analogRead(joy1Ypin);
    joy1Ystatus_min = joy1Ystatus_max = joy1Ystatus_mid;
    buttons56status_mid = analogRead(joy2Xpin);
    throttleStatus_old = analogRead(joy2Ypin);
    throttleStatus_min = throttleStatus_max = throttleStatus_old;
  }
}

/* In the period [2, ..., 10] seconds after boot-up, we obtain
 * the extremum values of all joystick axes. This is where the
 * user has to move all sticks/press all buttons.
 */
void calibrateAxes() {
  int tmp;

  lcd.setBacklight(TEAL);
  lcd.setCursor(0, 0);
  lcd.print("Please move axes");

  while (millis() > 1000 && millis() <= 10000) {
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
}