/*
 * modules/EthernetManager.cpp
 */

#include "EthernetManager.h"
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// این هدر اکنون به دلیل تغییر شما در dependencies در دسترس است
#include "ethernet_init.h" 

static const char *TAG = "EthManager";

// --- پیاده‌سازی عمومی ---

EthernetManager::EthernetManager() : eth_handle(nullptr), eth_netif(nullptr) {
    memset(mac_addr, 0, sizeof(mac_addr));
}

EthernetManager::~EthernetManager() {
    // پاکسازی در صورت نیاز
}

void EthernetManager::start() {
    ESP_LOGI(TAG, "Starting Ethernet Manager...");
    
    // 1. راه‌اندازی درایور با استفاده از کامپوننت ethernet_init
    init_ethernet_driver();
    
    // 2. ایجاد لایه شبکه
    create_netif();
    
    // 3. ثبت رویدادها
    register_event_handlers();
    
    // 4. استارت درایور
    start_driver();
}

// --- پیاده‌سازی خصوصی ---

void EthernetManager::init_ethernet_driver() {
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles = nullptr;

    // فراخوانی تابع کمکی که در dependencies اضافه کردید
    esp_err_t ret = example_eth_init(&eth_handles, &eth_port_cnt);
    
    if (ret != ESP_OK || eth_handles == nullptr || eth_port_cnt == 0) {
        ESP_LOGE(TAG, "Failed to initialize Ethernet driver via example_eth_init");
        return;
    }

    // ما فقط اولین پورت را برمی‌داریم
    eth_handle = eth_handles[0];

    // خواندن و تنظیم مک آدرس
    ESP_ERROR_CHECK(esp_read_mac(mac_addr, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    ESP_LOGI(TAG, "Ethernet Driver Initialized. MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

void EthernetManager::create_netif() {
    // 1. مقداردهی اولیه پشته شبکه
    ESP_ERROR_CHECK(esp_netif_init());

    // 2. ایجاد Event Loop به صورت ایمن (Safe)
    // اگر قبلاً توسط WiFi ساخته شده باشد، خطا نمی‌دهد و برنامه ادامه می‌یابد
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // اگر خطایی غیر از "قبلاً ساخته شده" بود، آنگاه گزارش کن
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return; // یا می‌توانید از ESP_ERROR_CHECK(ret) استفاده کنید اگر می‌خواهید متوقف شود
    }

    // 3. ایجاد اینترفیس اترنت
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    
    esp_netif_config_t netif_cfg = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };

    esp_netif_config.if_key = "ETH_W5500";
    esp_netif_config.if_desc = "w5500";
    esp_netif_config.route_prio = 50; 

    eth_netif = esp_netif_new(&netif_cfg);

    // اتصال درایور به شبکه
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    
    ESP_LOGI(TAG, "Network Interface Created (DHCP Enabled)");
}

void EthernetManager::register_event_handlers() {
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, 
                                               &EthernetManager::eth_event_handler_static, this));
    
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, 
                                               &EthernetManager::got_ip_event_handler_static, this));
}

void EthernetManager::start_driver() {
    if (eth_handle) {
        ESP_LOGI(TAG, "Starting Ethernet Driver...");
        ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    }
}

// --- هندلرهای رویداد ---

void EthernetManager::eth_event_handler_static(void *arg, esp_event_base_t event_base,
                                         int32_t event_id, void *event_data) {
    EthernetManager *manager = static_cast<EthernetManager *>(arg);
    if (manager) {
        manager->handle_eth_event(event_base, event_id, event_data);
    }
}

void EthernetManager::got_ip_event_handler_static(void *arg, esp_event_base_t event_base,
                                            int32_t event_id, void *event_data) {
    EthernetManager *manager = static_cast<EthernetManager *>(arg);
    if (manager) {
        manager->handle_got_ip_event(event_base, event_id, event_data);
    }
}

void EthernetManager::handle_eth_event(esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Up");
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;
        default:
            break;
    }
}

void EthernetManager::handle_got_ip_event(esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETH IP: " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETH MASK: " IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETH GW: " IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}