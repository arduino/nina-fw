#include "wiring_digital.h"

#include "WiFi.h"
#include <esp_wifi.h>
#include "esp_log.h"

#include "RGBled.h"

#define TAG "RGBled"

RGBClass::RGBClass()
{
}

void RGBClass::init(void)
{
  RGBmode(OUTPUT);
}

void RGBClass::RGBmode(uint32_t mode)
{
  pinMode(25, mode);
  pinMode(26, mode);
  pinMode(27, mode);
}

void RGBClass::RGBcolor(uint8_t red, uint8_t green, uint8_t blue)
{
  analogWrite(25, green);
  analogWrite(26, red);
  analogWrite(27, blue);
}

void RGBClass::clearLed(void)
{
  analogWrite(25, 0);  //GREEN
  analogWrite(26, 0);  //RED
  analogWrite(27, 0);  //BLU
}

void RGBClass::ledRGBEvent(system_event_t* event)
{
  switch (event->event_id) {
    case SYSTEM_EVENT_SCAN_DONE:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_SCAN_DONE");
      break;

    case SYSTEM_EVENT_STA_START:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_STA_START");
      break;

    case SYSTEM_EVENT_STA_STOP:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_STA_STOP");
      break;

    case SYSTEM_EVENT_STA_CONNECTED:

      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_STA_CONNECTED");

      RGBcolor(0, 255, 0);  //GREEN
      break;

    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_STA_GOT_IP");
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED: {
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_STA_DISCONNECTED");
      uint8_t status;
      status = WiFi.status();

      if (status == WL_CONNECT_FAILED) {
        RGBcolor(0, 127, 255); //LIGHT BLUE
      }
      else {
        RGBcolor(255, 0, 0);  //RED
      }
      break;
    }

    case SYSTEM_EVENT_STA_LOST_IP:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_STA_LOST_IP");
      break;

    case SYSTEM_EVENT_AP_START:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_AP_START");

      RGBcolor(255, 255, 0);  //YELLOW
      break;

    case SYSTEM_EVENT_AP_STOP:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_AP_STOP");
      break;

    case SYSTEM_EVENT_AP_STACONNECTED:
      ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_AP_STOP. Before asking tasks states");
      if (RGBtask_handle != NULL) {
        vTaskSuspend( RGBtask_handle );
        vTaskDelete( RGBtask_handle );
      }
      xTaskCreatePinnedToCore(RGBClass::APconnection, "APconnection", 8192, NULL, 1, &RGBtask_handle, 1);
      break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
      if (RGBtask_handle != NULL) {
        vTaskSuspend( RGBtask_handle );
        vTaskDelete( RGBtask_handle );
      }
      xTaskCreatePinnedToCore(RGBClass::APdisconnection, "APdisconnection", 8192, NULL, 1, &RGBtask_handle, 1);
      break;

    default:
      break;
  }
}

void RGBClass::RGBstop(void*)
{
  ESP_EARLY_LOGI(TAG, "RGBstop thread created");
  while(1) {
    if (RGB.RGBtask_handle != NULL) {
        vTaskSuspend( RGB.RGBtask_handle );
        vTaskDelete( RGB.RGBtask_handle );
    }
    
    RGB.clearLed();
    RGB.RGBmode(INPUT);
  }
}


void RGBClass::APconnection(void*)
{
  wifi_sta_list_t stadevList;
  
  ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_AP_STACONNECTED");
  esp_wifi_ap_get_sta_list(&stadevList);

  uint8_t num_dev = stadevList.num;
  int blink_time = 1000 - 250*(num_dev-1);

  for (int i = 0; i < 3; i++) {
    RGB.RGBcolor(127, 0, 255);  //VIOLET
    delay(blink_time);
    RGB.RGBcolor(0, 255, 0);  //GREEN
    delay(blink_time);
  }

  RGB.clearLed();

  vTaskSuspend( NULL );
  vTaskDelete( NULL );
}

void RGBClass::APdisconnection(void*)
{
  wifi_sta_list_t stadevList;

  ESP_EARLY_LOGI(TAG, "State: SYSTEM_EVENT_AP_STADISCONNECTED");
  esp_wifi_ap_get_sta_list(&stadevList);

  uint8_t num_dev = stadevList.num;
  int blink_time = 1000 - 250*(num_dev-1);

  for (int j = 0; j < 3; j++) {
    RGB.RGBcolor(127, 0, 255);  //VIOLET
    delay(blink_time);
    RGB.RGBcolor(255, 0, 0);  //GREEN
    delay(blink_time);
  }

  RGB.clearLed();

  vTaskSuspend( NULL );
  vTaskDelete( NULL );
}