#ifndef RGBLED_H_
#define RGBLED_H_

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

// Declarations for the event source
#define TASK_ITERATIONS_COUNT        10      // number of times the task iterates
#define TASK_PERIOD                  500     // period of the task loop in milliseconds

ESP_EVENT_DECLARE_BASE(RGB_LED_EVENT);         // declaration of the task events family

static const char* TAG = "RGBled";

enum {
    RGB_EVENT_STA_CONNECTED = SYSTEM_EVENT_STA_CONNECTED,           /* Connected to WiFi */
    RGB_EVENT_STA_DISCONNECTED = SYSTEM_EVENT_STA_DISCONNECTED,     /* Disconnected from WiFi */
    RGB_EVENT_AP_START = SYSTEM_EVENT_AP_START,                     /* Access point created */
    RGB_EVENT_AP_STACONNECTED = SYSTEM_EVENT_AP_STACONNECTED,       /* Device connected to Access Point */
    RGB_EVENT_AP_STADISCONNECTED = SYSTEM_EVENT_AP_STADISCONNECTED, /* Device disconnected from Access Point */
    RGB_EVENT_LED_STOP = (SYSTEM_EVENT_MAX + 1),                    /* Stop RGB led */
    RGB_EVENT_LED_RESTART = (SYSTEM_EVENT_MAX + 2)                  /* Restart RGB led */
};

void rgb_init(void);

void ledRGBEventSource(int32_t event_id);   //system_event_t* event

#ifdef __cplusplus
}
#endif

#endif // #ifndef RGBLED_H_