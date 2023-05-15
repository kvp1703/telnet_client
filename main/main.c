/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include<stdio.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <rom/ets_sys.h>

#include "telnet_client.h"
static const char *TAG = "example";

#define MAC_ADDR_SIZE 6

void app_cb(const char* command){
    
    ESP_LOGI(TAG, "Executing command %s ...", command);

    if(strcmp(command, "print_mac") == 0){

        uint8_t mac_address[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
        uint8_t mac[MAC_ADDR_SIZE];
        esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
        ESP_LOGI(TAG, "MAC address: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    telnet_client_init(app_cb);
}
