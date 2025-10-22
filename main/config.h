#ifndef CONFIG_H
#define CONFIG_H

// پین‌های نمایشگر TM1640
#define TM1640_DIO_PIN GPIO_NUM_21
#define TM1640_CLK_PIN GPIO_NUM_22

// تعریف حالت‌ها یا تنظیمات پروژه
#define DEBUG_MODE       1       // فعال کردن حالت دیباگ
#define MAX_BUFFER_SIZE  1024    // اندازه بافر پیش‌فرض
#define USE_FEATURE_X    1       // فعال‌سازی Feature X
#define USE_FEATURE_Y    0       // غیرفعال کردن Feature Y

// تعاریف سیستم یا پلتفرم
#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX   0

// پین LED وضعیت
#define STATUS_LED_PIN GPIO_NUM_2

// تعاریف عمومی
#define TRUE  1
#define FALSE 0

// UART
#define UART_USER_PORT UART_NUM_1   // یا UART_NUM_0 / UART_NUM_2 بر اساس سخت‌افزار شما
#define UART_TX_PIN GPIO_NUM_17
#define UART_RX_PIN GPIO_NUM_16
#define UART_BAUD_RATE 115200

// هر تعریف دیگر مورد نیاز پروژه را اینجا اضافه کنید
// مثال:
// #define ENABLE_LOGGING 1

#endif // CONFIG_H
