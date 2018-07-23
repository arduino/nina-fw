/*
  This file is part of the Arduino NINA firmware.
  Copyright (c) 2018 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <rom/uart.h>

extern "C" {
  #include <driver/periph_ctrl.h>
  #include <driver/uart.h>
  #include <esp_bt.h>
}

#include <Arduino.h>

#include <SPIS.h>
#include <WiFi.h>

#include "CommandHandler.h"

#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN

int debug = 0;

uint8_t* commandBuffer;
uint8_t* responseBuffer;

void dumpBuffer(const char* label, uint8_t data[], int length) {
  ets_printf("%s: ", label);

  for (int i = 0; i < length; i++) {
    ets_printf("%02x", data[i]);
  }

  ets_printf("\r\n");
}

void setDebug(int d) {
  debug = d;

  if (debug) {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[1], 0);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[3], 0);

    const char* default_uart_dev = "/dev/uart/0";
    _GLOBAL_REENT->_stdin  = fopen(default_uart_dev, "r");
    _GLOBAL_REENT->_stdout = fopen(default_uart_dev, "w");
    _GLOBAL_REENT->_stderr = fopen(default_uart_dev, "w");

    uart_div_modify(CONFIG_CONSOLE_UART_NUM, (APB_CLK_FREQ << 4) / 115200);

    // uartAttach();
    ets_install_uart_printf();
    uart_tx_switch(CONFIG_CONSOLE_UART_NUM);
  } else {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[1], PIN_FUNC_GPIO);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[3], PIN_FUNC_GPIO);

    _GLOBAL_REENT->_stdin  = (FILE*) &__sf_fake_stdin;
    _GLOBAL_REENT->_stdout = (FILE*) &__sf_fake_stdout;
    _GLOBAL_REENT->_stderr = (FILE*) &__sf_fake_stderr;

    ets_install_putc1(NULL);
    ets_install_putc2(NULL);
  }
}

void setupWiFi();
void setupBluetooth();

void setup() {
  setDebug(debug);

  // put SWD and SWCLK pins connected to SAMD as inputs
  pinMode(15, INPUT);
  pinMode(21, INPUT);

  pinMode(5, INPUT);
  if (digitalRead(5) == LOW) {
    setupBluetooth();
  } else {
    setupWiFi();
  }
}

#ifndef HCI_UART
#define HCI_UART 1
#endif

void setupBluetooth() {
#if HCI_UART == 0
  periph_module_enable(PERIPH_UART0_MODULE);
#else
  periph_module_enable(PERIPH_UART1_MODULE);
#endif
  periph_module_enable(PERIPH_UHCI0_MODULE);

#if HCI_UART == 0
  uart_set_pin(UART_NUM_0, 1, 3, 18, 5);
  uart_set_hw_flow_ctrl(UART_NUM_0, UART_HW_FLOWCTRL_DISABLE, 0);
#else
  uart_set_pin(UART_NUM_1, 23, 12, 18, 5);
#endif

  esp_bt_controller_config_t btControllerConfig = {
    .controller_task_stack_size = ESP_TASK_BT_CONTROLLER_STACK,
    .controller_task_prio = ESP_TASK_BT_CONTROLLER_PRIO,
#if HCI_UART == 0
    .hci_uart_no = UART_NUM_0,
#else
    .hci_uart_no = UART_NUM_1,
#endif
    .hci_uart_baudrate = 115200,
  };

  esp_bt_controller_init(&btControllerConfig);
  esp_bt_controller_enable(ESP_BT_MODE_BLE);

  while (1) {
    vTaskDelay(portMAX_DELAY);
  }
}

void setupWiFi() {
  SPIS.begin();

  if (WiFi.status() == WL_NO_SHIELD) {
    while (1); // no shield
  }
  
  commandBuffer = (uint8_t*)heap_caps_malloc(SPI_BUFFER_LEN, MALLOC_CAP_DMA);
  responseBuffer = (uint8_t*)heap_caps_malloc(SPI_BUFFER_LEN, MALLOC_CAP_DMA);

  CommandHandler.begin();
}

void loop() {
  // wait for a command
  memset(commandBuffer, 0x00, SPI_BUFFER_LEN);
  int commandLength = SPIS.transfer(NULL, commandBuffer, SPI_BUFFER_LEN);

  if (commandLength == 0) {
    return;
  }

  if (debug) {
    dumpBuffer("COMMAND", commandBuffer, commandLength);
  }

  // process
  memset(responseBuffer, 0x00, SPI_BUFFER_LEN);
  int responseLength = CommandHandler.handle(commandBuffer, responseBuffer);

  SPIS.transfer(responseBuffer, NULL, responseLength);

  if (debug) {
    dumpBuffer("RESPONSE", responseBuffer, responseLength);
  }
}
