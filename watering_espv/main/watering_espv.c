#include <stdio.h>
#include "freeRTOS/FreeRTOS.h"
#include "freertos/task.h"
#include "drivers/gpio.h"

#define PIN_VALVE GPIO_NUM_1
#define PIN_BUTTON GPIO_NUM_2
#define PIN_SOLAR GPIO_NUM_3

#define RELAY_ACTIVE_HIGH 0

#if RELAY_ACTIVE_HIGH
 {
    #define VALVE_ON 1
    #define VALVE_OFF 0
 }
#else
 {
    #define VALVE_ON 0
    #define VALVE_OFF 1
 } 

#define TEST_MODE 1
const unsigned long TON_MS=0;
const unsigned long TOFF_MS=0;
const unsigned long LASTWATER_MS=0;
const unsigned long MAX_VALVE_ON_MS=0;
const unsigned long MIN_DAY_ON=0;

#if TEST_MODE
{
    TON_MS=0;
    TOFF_MS=0;
    LASTWATER_MS=0;
    MAX_VALVE_ON_MS=0;
    MIN_DAY_ON=0;
}
#else
{
    TON_MS=0;
    TOFF_MS=0;
    LASTWATER_MS=0;
    MAX_VALVE_ON_MS=0;
    MIN_DAY_ON=0;
}

const int day_thresh = 400;
const int night_thresh =500;

const unsigned long s1_end = TON_MS;
const unsigned long s1gap_end = TON_MS + TOFF_MS;
const unsigned long s2_end = TON_MS + TOFF_MS + TON_MS;

struct{
    night;
    day;
}daystate;

struct{
    warmup;
    sesh1;
    gap;
    sesh2;
    idle;
    night;
}waterstate;

daystate dstate = night;
waterstate wstate = night;

bool virtualday = false;

unsigned long lastdaystart=0;
unsigned long lastfallbackreset=0;
unsigned long valveonsince=0;

unsigned long confirm start;
bool pendingtransition =false;
daystate pendingstate =night;

bool valveison false;

void app_main(void)
{

}
