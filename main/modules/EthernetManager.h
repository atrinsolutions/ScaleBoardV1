/*
 * modules/EthernetManager.h
 */

#ifndef ETHERNET_MANAGER_H
#define ETHERNET_MANAGER_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

class EthernetManager {
public:
    EthernetManager();
    ~EthernetManager();

    /**
     * @brief شروع فرآیند اتصال اترنت و دریافت IP
     */
    void start();

private:
    esp_eth_handle_t eth_handle;
    esp_netif_t *eth_netif;
    uint8_t mac_addr[6];

    // استفاده از example_eth_init
    void init_ethernet_driver();

    void create_netif();
    void register_event_handlers();
    void start_driver();

    // هندلرهای رویداد
    static void eth_event_handler_static(void *arg, esp_event_base_t event_base,
                                         int32_t event_id, void *event_data);

    static void got_ip_event_handler_static(void *arg, esp_event_base_t event_base,
                                            int32_t event_id, void *event_data);

    void handle_eth_event(esp_event_base_t event_base, int32_t event_id, void *event_data);
    void handle_got_ip_event(esp_event_base_t event_base, int32_t event_id, void *event_data);
};

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_MANAGER_H