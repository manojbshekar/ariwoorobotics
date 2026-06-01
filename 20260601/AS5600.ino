//
/*
This code is for measuring RPM, and Angle of a motor shaft. 
NOTE: This Source is for reading values that has a D-Shaft with axially magnetised configuartion,
with AS5600 placed in a radial offset. In my configuration, the bot is placed around 2-3mm away from the nearest rotating magnet attached to shaft. 
Video reference: https://www.instagram.com/p/DY_T-oABRys/
*/
#include <Wire.h>

#define AS5600_ADDR    0x36
#define RAW_ANGLE_HIGH 0x0C
#define RAW_ANGLE_LOW  0x0D

const float COUNTS_PER_REV  = 4096.0;
const int   SAMPLE_INTERVAL_US = 100; // For every 0.1ms get the sample. 

// State
int16_t  lastAngle       = 0;
unsigned long lastTime   = 0;

// Revolution period RPM
float    rpm             = 0.0;
unsigned long lastRevTime = 0;
bool     revStarted      = false;
int16_t  prevAngle       = 0;
long     accumDelta      = 0; // accumulated counts since last full rev

int16_t readRawAngle() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(RAW_ANGLE_HIGH);
  Wire.endTransmission(false);
  Wire.requestFrom(AS5600_ADDR, 2);
  int16_t high = Wire.read();
  int16_t low  = Wire.read();
  return ((high << 8) | low) & 0x0FFF;
}

void setup() {
  //I also have 2 I2Cs, and need to confirm just before running the loop. one for AS5600 and another for SSD1306 OLED.
  Serial.begin(115200);
  while (!Serial);
  delay(1000);
  Wire.begin(5, 6);

  Serial.println("Scanning I2C...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Device found at 0x");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("AS5600 Ready.");
  Serial.println("timestamp_us,angle_deg,rpm");

  lastAngle    = readRawAngle();
  prevAngle    = lastAngle;
  lastTime     = micros();
  lastRevTime  = micros();
  revStarted   = true;
}

void loop() {
  unsigned long now = micros();

  if (now - lastTime >= SAMPLE_INTERVAL_US) {
    int16_t currentAngle = readRawAngle();

    // Angle in degrees
    float degrees = (currentAngle / COUNTS_PER_REV) * 360.0;

    // Revolution period RPM
    int16_t delta = currentAngle - prevAngle;
    if (delta >  2048) delta -= 4096; // wraparound CW
    if (delta < -2048) delta += 4096; // wraparound CCW

    accumDelta += delta;

    // Detect full revolution: +4096 counts = 1 full CW rev
    //                         -4096 counts = 1 full CCW rev
    if (accumDelta >= 4096) {
      unsigned long revPeriodUs = now - lastRevTime;
      rpm         = 60000000.0 / revPeriodUs; // CW positive
      lastRevTime = now;
      accumDelta -= 4096;
    } else if (accumDelta <= -4096) {
      unsigned long revPeriodUs = now - lastRevTime;
      rpm         = -60000000.0 / revPeriodUs; // CCW negative
      lastRevTime = now;
      accumDelta += 4096;
    }

    // Log
    Serial.print(now);
    Serial.print(",");
    Serial.print(degrees, 2);
    Serial.print(",");
    Serial.println(rpm, 2);

    prevAngle = currentAngle;
    lastTime  = now;
  }
}

