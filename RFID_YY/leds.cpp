#include <Arduino.h>

void onLed(const uint8_t pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void offLed(const uint8_t pin)
{
  digitalWrite(pin, HIGH);
  pinMode(pin, INPUT);
}

void blinkLed(const uint8_t pin, const uint32_t ms, int num)
{
  for (int i = 0; i < num; i++)
  {
    onLed(pin);
    delay(ms);
    offLed(pin);
    if (i < num - 1)
    {
      delay(ms);
    }
  }
}

void setupLed(const uint8_t pin)
{
  pinMode(pin, INPUT);
}
