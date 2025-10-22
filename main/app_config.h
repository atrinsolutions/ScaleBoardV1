#pragma once
#include "driver/gpio.h"
#include "driver/uart.h"

#define UART_USER_PORT     UART_NUM_0
#define UART_EXT_PORT      UART_NUM_1
#define ADC_DATA_PIN       GPIO_NUM_4
#define ADC_CLK_PIN        GPIO_NUM_5
#define TM1640_CLK_PIN     GPIO_NUM_6
#define TM1640_DIO_PIN     GPIO_NUM_7
#define CASH_DRAWER_PIN    GPIO_NUM_8
#define UPS_CTRL_PIN       GPIO_NUM_9
#define STATUS_LED_PIN     GPIO_NUM_2
