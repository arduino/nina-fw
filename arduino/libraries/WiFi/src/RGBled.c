#include "esp_log.h"
#include <esp_wifi.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "RGBled.h"
#include "esp_event_base.h"
#include "../private_include/esp_event_internal.h"

#include "wiring_digital.h"

// Event loop
static esp_event_loop_handle_t RGB_event_loop;

bool rgb_stop = false;
bool ap_connected = true;
int blink_time = 0;

ESP_EVENT_DEFINE_BASE(RGB_LED_EVENT)


void RGBmode(uint32_t mode)
{
  pinMode(25, mode);
  pinMode(26, mode);
  pinMode(27, mode);
}

void clearLed(void)
{
  analogWrite(25, 0);  //GREEN
  analogWrite(26, 0);  //RED
  analogWrite(27, 0);  //BLU
}

void RGBcolor(uint8_t red, uint8_t green, uint8_t blue)
{
    if (rgb_stop == true) {
        clearLed();
    }
    else {
        analogWrite(25, green);
        analogWrite(26, red);
        analogWrite(27, blue);
    }
}

void APconnection(void)
{
    wifi_sta_list_t stadevList;
    esp_wifi_ap_get_sta_list(&stadevList);

    uint8_t num_dev = stadevList.num;
    int blink_time = 1000 - 250*(num_dev-1);

    RGBcolor(127, 0, 255);  //VIOLET
    delay(blink_time);
    RGBcolor(0, 255, 0);  //GREEN
    delay(blink_time);
    if (uxQueueMessagesWaiting(((esp_event_loop_instance_t*) RGB_event_loop)->queue) == 0) {
        esp_event_post_to(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_AP_STACONNECTED, NULL, 0, 0);
    }
}

void APdisconnection(void)
{
    wifi_sta_list_t stadevList;
    esp_wifi_ap_get_sta_list(&stadevList);

    uint8_t num_dev = stadevList.num;
    int blink_time = 1000 - 250*num_dev;

    RGBcolor(127, 0, 255);  //VIOLET
    delay(blink_time);
    RGBcolor(255, 0, 0);  //GREEN
    delay(blink_time);
    if (uxQueueMessagesWaiting(((esp_event_loop_instance_t*) RGB_event_loop)->queue) == 0) {
        esp_event_post_to(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_AP_STADISCONNECTED, NULL, 0, 0);
    }
}

static void run_on_rgb_event(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    switch (id)
    {
    case RGB_EVENT_STA_CONNECTED:
        ESP_EARLY_LOGI(TAG, "State: RGB_EVENT_STA_CONNECTED");

        RGBcolor(0, 255, 0);
        break;

    case RGB_EVENT_STA_DISCONNECTED:
        ESP_EARLY_LOGI(TAG, "State: RGB_EVENT_STA_DISCONNECTED");

        RGBcolor(255, 0, 0);
        break;

    case RGB_EVENT_AP_START:
        ESP_EARLY_LOGI(TAG, "State: RGB_EVENT_AP_START");

        RGBcolor(255, 255, 0);
        break;

    case RGB_EVENT_AP_STACONNECTED:
        ESP_EARLY_LOGI(TAG, "State: RGB_EVENT_AP_STACONNECTED");

        ap_connected = true;

        if (uxQueueMessagesWaiting(((esp_event_loop_instance_t*) RGB_event_loop)->queue) == 0) {
            APconnection();
        }
        break;

    case RGB_EVENT_AP_STADISCONNECTED:
        ESP_EARLY_LOGI(TAG, "State: RGB_EVENT_AP_STADISCONNECTED");

        ap_connected = false;

        if (uxQueueMessagesWaiting(((esp_event_loop_instance_t*) RGB_event_loop)->queue) == 0) {
            APdisconnection();
        }
        break;

    case RGB_EVENT_LED_STOP:
        ESP_EARLY_LOGI(TAG, "State: RGB_EVENT_LED_STOP");

        rgb_stop = true;

        clearLed();

        break;

    case RGB_EVENT_LED_RESTART:
        ESP_EARLY_LOGI(TAG, "State: RGB_EVENT_LED_RESTART");

        rgb_stop = false;
        break;
    
    default:
        break;
    }
}

void ledRGBEventSource(int32_t event_id)
{
    if (event_id == SYSTEM_EVENT_STA_CONNECTED || event_id == RGB_EVENT_STA_DISCONNECTED || event_id == RGB_EVENT_AP_START || event_id == RGB_EVENT_AP_STACONNECTED || event_id == RGB_EVENT_AP_STADISCONNECTED || event_id == RGB_EVENT_LED_STOP || event_id == RGB_EVENT_LED_RESTART) {
        esp_event_post_to(RGB_event_loop, RGB_LED_EVENT, event_id, NULL, 0, 0);
    }
}

void rgb_init()
{
    RGBmode(OUTPUT);

    esp_event_loop_args_t loop_args = {
        .queue_size = 5,
        .task_name = "RGB_loop_task",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 2048,
        .task_core_id = tskNO_AFFINITY
    };

    esp_event_loop_create(&loop_args, &RGB_event_loop);

    esp_event_handler_register_with(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_STA_CONNECTED, run_on_rgb_event, NULL);
    esp_event_handler_register_with(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_STA_DISCONNECTED, run_on_rgb_event, NULL);
    esp_event_handler_register_with(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_AP_START, run_on_rgb_event, NULL);
    esp_event_handler_register_with(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_AP_STACONNECTED, run_on_rgb_event, NULL);
    esp_event_handler_register_with(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_AP_STADISCONNECTED, run_on_rgb_event, NULL);
    esp_event_handler_register_with(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_LED_STOP, run_on_rgb_event, NULL);
    esp_event_handler_register_with(RGB_event_loop, RGB_LED_EVENT, RGB_EVENT_LED_RESTART, run_on_rgb_event, NULL);

}