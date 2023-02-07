#include "BleMouse.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

#define LED_PIN   2

#define TOUCH_KEY_QUANTIY   3
#define TOUCH_THRESHOLD     60
#define TOUCH_KEY_SCAN_PERIOD      10 /*100msec*/
#define LONG_PRESS_TIME         (1000/TOUCH_KEY_SCAN_PERIOD)

#define TOUCH_KEY_NO_CHANGE           0x00
#define TOUCH_KEY_DOWN                0x20
#define TOUCH_KEY_UP                  0x40
#define TOUCH_KEY_LONG_PRESS          0x80

BleMouse bleMouse;
MPU6050 accelgyro;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleMouse.begin();

  Wire.begin();
  Wire.setClock(400000);

  // initialize device
  Serial.println("Initializing I2C devices...");
  accelgyro.initialize();
  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  accelgyro.setXGyroOffset(-55);
  accelgyro.setYGyroOffset(66);
  accelgyro.setZGyroOffset(65);

  // configure Arduino LED pin for output
  pinMode(LED_PIN, OUTPUT);

}

void loop()
{
  static bool s_bClickStable=false;
  static bool s_bMiddleKeyPressed=false;
  if(bleMouse.isConnected())
  {
    int16_t gx, gy, gz;
    accelgyro.getRotation(&gx, &gy, &gz);
    uint8_t u8Key = u8TouchKeyScan();
    // if (u8Key){Serial.println(u8Key);}
    // Serial.print(touchRead(T0));
    // Serial.print(",");
    // Serial.print(touchRead(T4));
    // Serial.print(",");
    // Serial.println(touchRead(T5));
    switch (u8Key)
    {
      case TOUCH_KEY_DOWN:
      bleMouse.press(MOUSE_LEFT);
      s_bClickStable = true;
      break;
      case TOUCH_KEY_UP:
      case TOUCH_KEY_LONG_PRESS|TOUCH_KEY_UP:
      bleMouse.release(MOUSE_LEFT);
      s_bClickStable = false;
      break;
      case TOUCH_KEY_LONG_PRESS|1:
      // bleMouse.press(MOUSE_MIDDLE);
      // s_bClickStable = true;
      s_bMiddleKeyPressed = true;
      break;
      case TOUCH_KEY_UP|1:
      case TOUCH_KEY_LONG_PRESS|TOUCH_KEY_UP|1:
      // bleMouse.release(MOUSE_MIDDLE);
      // s_bClickStable = false;
      s_bMiddleKeyPressed = false;
      break;
      case TOUCH_KEY_DOWN|2:
      bleMouse.press(MOUSE_RIGHT);
      s_bClickStable = true;
      break;
      case TOUCH_KEY_UP|2:
      case TOUCH_KEY_LONG_PRESS|TOUCH_KEY_UP|2:
      bleMouse.release(MOUSE_RIGHT);
      s_bClickStable = false;
      break;
      default:
      break;
    }
    signed char x = gz/256;
    signed char y = gy/256;
    if (abs(x)>10 || abs(y)>10)
    {
      s_bClickStable = false;      
    }
    if (!s_bClickStable)
    {
      if (s_bMiddleKeyPressed)
      {
        x /= 10;
        y /= 10;
        if (x!=0 || y!=0)
        {
          bleMouse.move(0,0,y,x);
        }
      }
      else
      {
        if (x!=0 || y!=0)
        {
          bleMouse.move(-x, -y);
        }
      }
      digitalWrite(LED_PIN, 1);
    }
    delay(10);
    digitalWrite(LED_PIN, 0);
  }
}
uint16_t u16AverageFilter(uint16_t Value[])
{
  uint16_t val, average;
  uint8_t min;
  uint32_t sum;
  if (Value[0] < Value[1])
  {
    val = Value[0];
    min = 0;
  }
  else
  {
    val = Value[1];
    min = 1;
  }
  if (Value[2] < val)
  {
    val = Value[2];
    min = 2;
  }
  sum = (uint32_t)Value[0] + Value[1] + Value[2];
  average = sum / 3;
  if ((average - Value[min]) > 10)
  {
    average = (sum - Value[min]) / 2;
  }
  return average;
}
/*----------------------------------------------
Function: Touch Key scan
Parameter: void
Return: bit0~4 Touch Key number,
    bit5~7 Touch Key status: 0x00 no press, 0x20 Touch Key down, 
    0x40 Touch Key up, 0x80 Touch Key long press,
    0xC0 Touch Key up after long press.
Comments: Scan Touch Keys in 100mS
----------------------------------------------*/
uint8_t u8TouchKeyScan(void)
{
    static uint8_t s_u8ScanedTime, s_u8Sequence=0;
    static uint8_t s_a_u8TouchKey[TOUCH_KEY_QUANTIY];
    static uint8_t s_a_u8TouchKeyTimeCount[TOUCH_KEY_QUANTIY];
    static uint8_t s_a_u8Status[TOUCH_KEY_QUANTIY];
    static uint16_t s_aa_u16TouchValue[TOUCH_KEY_QUANTIY][3];
    uint8_t u8i;
    u8i = millis();
    if ((uint8_t)(u8i - s_u8ScanedTime) >= TOUCH_KEY_SCAN_PERIOD)
    {
        s_u8ScanedTime = u8i;
        for (u8i=0;u8i<TOUCH_KEY_QUANTIY;u8i++)
        {
            s_a_u8TouchKey[u8i] = (s_a_u8TouchKey[u8i] << 1) & 0x02;
        }
        s_u8Sequence++;
        if (s_u8Sequence >= 3)
        {
          s_u8Sequence = 0;
        }
        s_aa_u16TouchValue[0][s_u8Sequence] = touchRead(T0); /* left key */
        s_aa_u16TouchValue[1][s_u8Sequence] = touchRead(T4); /* middle key */
        s_aa_u16TouchValue[2][s_u8Sequence] = touchRead(T5); /* right key */
        for (u8i=0;u8i<TOUCH_KEY_QUANTIY;u8i++)
        {
            if(u16AverageFilter(s_aa_u16TouchValue[u8i]) < TOUCH_THRESHOLD) /*Touch Key0 pressed*/
            {
                s_a_u8TouchKey[u8i] |= 0x01;
            }
            switch (s_a_u8TouchKey[u8i])
            {
            case 0x01:
                s_a_u8Status[u8i] = TOUCH_KEY_DOWN;
                s_a_u8TouchKeyTimeCount[u8i] = 0;
                return (TOUCH_KEY_DOWN | u8i);
            case 0x02:
                return (((s_a_u8Status[u8i] & TOUCH_KEY_LONG_PRESS) | TOUCH_KEY_UP) | u8i);
            case 0x03:
                if (TOUCH_KEY_DOWN == s_a_u8Status[u8i])
                {
                    s_a_u8TouchKeyTimeCount[u8i]++;
                    if (s_a_u8TouchKeyTimeCount[u8i] >= LONG_PRESS_TIME)
                    {
                        s_a_u8Status[u8i] = TOUCH_KEY_LONG_PRESS;
                        return (TOUCH_KEY_LONG_PRESS | u8i);
                    }
                }
                break;
            default:    /*case 0x00*/
                break;
            }
        }
    }
    return TOUCH_KEY_NO_CHANGE;
}
