#ifndef CONFIG_H
#define CONFIG_H
// تعریف حالت‌ها یا تنظیمات پروژه
#define DEBUG_MODE       1       // فعال کردن حالت دیباگ
#define MAX_BUFFER_SIZE  1024    // اندازه بافر پیش‌فرض
#define USE_FEATURE_X    1       // فعال‌سازی Feature X
#define USE_FEATURE_Y    0       // غیرفعال کردن Feature Y

// ---------- Display TM1640 ----------
#define TM1640_DIO_PIN GPIO_NUM_34   // DIO
#define TM1640_CLK_PIN GPIO_NUM_35   // CLK

// ---------- Status LED ----------
#define STATUS_LED_PIN GPIO_NUM_0    // LED0

// ---------- UART ----------
#define UART_USER_PORT UART_NUM_1
#define UART_TX_PIN GPIO_NUM_36      // TX0-IN
#define UART_RX_PIN GPIO_NUM_37      // RX0-OUT
#define UART_BAUD_RATE 115200

// ---------- Keyboard (ROW/COL) ----------
#define ROW1_PIN GPIO_NUM_32
#define ROW2_PIN GPIO_NUM_33
#define ROW3_PIN GPIO_NUM_34
#define ROW4_PIN GPIO_NUM_35

#define COL1_PIN GPIO_NUM_31
#define COL2_PIN GPIO_NUM_32
#define COL3_PIN GPIO_NUM_33
#define COL4_PIN GPIO_NUM_34

// ---------- ADC AD7191 ----------
#define ADC_DATA_INT_PIN GPIO_NUM_39
#define ADC_A0_PIN GPIO_NUM_38
#define ADC_SCK_PIN GPIO_NUM_34

// ---------- Buzzer ----------
#define BUZZER_PIN GPIO_NUM_12

// ---------- Power Management ----------
#define BAT_SAMP_PIN GPIO_NUM_7
#define DC_SAMP_PIN GPIO_NUM_6
#define UPS_CTRL_PIN GPIO_NUM_8

// ---------- USB ----------
#define USB_D_PIN GPIO_NUM_13
#define USB_DP_PIN GPIO_NUM_14

// ---------- Misc / Micro ----------
#define MICRO_EN_PIN GPIO_NUM_2   // EN
#define TX1_OUT_PIN GPIO_NUM_10
#define RX1_IN_PIN GPIO_NUM_9

// سایر تنظیمات پروژه
#define MAIN_LOOP_DELAY_MS 200


#endif // CONFIG_H
