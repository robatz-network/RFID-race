#ifndef _SNIPPETS_H
#define _SNIPPETS_H
#include <Arduino.h>

void setupLed(const uint8_t pin);
void onLed(const uint8_t pin);
void offLed(const uint8_t pin);
void blinkLed(const uint8_t pin, const uint32_t ms, int num = 1);

#endif
