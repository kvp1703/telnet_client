/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_console.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include<rom/ets_sys.h>

#include "telnet_client.h"


#define PORT                        CONFIG_EXAMPLE_PORT
#define LOG_BUFFER_SIZE             (1024UL)
#define COMMAND_LEN                 128

char prompt[] = "esp>";

static const char *TAG = "example";
int sock;
app_cli_cb_t g_app_cli_cb;


static int systemLogApiVprintf(const char* fmt, va_list args)
{
    vprintf(fmt, args); // sends the log print to the normal serial output.
    int status = 0;
    
    char *logBuff = malloc(LOG_BUFFER_SIZE);
    for(int i = 0; i < LOG_BUFFER_SIZE; i++)
        logBuff[i] = 0;
    
    if(logBuff != NULL)
    {
        status = vsnprintf(logBuff, LOG_BUFFER_SIZE, fmt, args);
        int written = send(sock, logBuff, status, 0);

        if (written < 0) {

            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            return -1;

        }

        written = send(sock, "\r", 1, 0);
        free(logBuff);
    }
    
    return status;
}

static void handle_command_request(const int sock)
{
    int len = 0;
    char rx_buffer[COMMAND_LEN] = {'\0'};
    int index = 0;
    char cmd[COMMAND_LEN] = {'\0'};
    bool is_option_code = 0;

    vprintf_like_t serial_log_printer = NULL;

    // len = recv(sock, rx_buffer, COMMAND_LEN, 0);

    // for(int i = 0; i < COMMAND_LEN; i++){
    //     printf("%d ",  (int)rx_buffer[i]);
    // }

    // printf("\n");

    int is_expecting = 0;

    do{
        len = recv(sock, rx_buffer, 1, 0);

        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else if (index == 128){
            ESP_LOGE(TAG, "Command is too long to execute, only %d charaters allowed!", COMMAND_LEN);
            for(int i = 0; i < COMMAND_LEN; i++)
                cmd[i] = 0;
            index = 0;
        } else {

            // Handling the case where TELNET commands are received. refer https://datatracker.ietf.org/doc/html/rfc854

            if((int) rx_buffer[0] == 255){
                // code 255 : IAC 
                is_expecting = 1;
                continue;
            }

            if(is_expecting){
                int code = (int)rx_buffer[0];
                if(code <= 250 || code >= 255){
                    is_expecting = 0;
                } 
                continue;
            }

            rx_buffer[len] = 0;

            //Space at the beginning of the command
            if(index == 0 && (int)rx_buffer[0] == 32) 
                continue;

            if((int) rx_buffer[0] == 13 || (int) rx_buffer[0] == 10){

                if((int) rx_buffer[0] == 13)
                    len = recv(sock, rx_buffer, 1, 0); 
                // This will receive a char with ASCII letter 10. 
                // In linux, when we press enter, two chars are received with ASCII value 13, 10 respectively.

                cmd[index] = 0; // Completing command.

                if(strlen(cmd) == 0){

                } else if (strcmp(cmd, "quit_telnet") == 0){
                        
                    ESP_LOGI(TAG, "Quitting telnet...");

                    if(serial_log_printer != NULL){
                        vprintf_like_t telnet_log_printer = esp_log_set_vprintf(serial_log_printer);
                        serial_log_printer = NULL;
                    }

                    break;

                } else if (strcmp(cmd, "start_console") == 0){

                    if(serial_log_printer != NULL ){
                        ESP_LOGW(TAG, "Console is already in runnig state!");
                    }else{
                        ESP_LOGI(TAG, "Starting the console on telnet client...");
                        serial_log_printer = esp_log_set_vprintf(systemLogApiVprintf);
                        ESP_LOGI(TAG, "Now, you can see all the logs of the server here. You can also enter registered commands.");
                    }

                } else if (strcmp(cmd, "quit_console") == 0){

                    if(serial_log_printer == NULL ){
                        ESP_LOGW(TAG, "Console can not be turned off when it is not in runnig state!");
                    }else{
                        ESP_LOGI(TAG, "Quitting the console on telnet client...");
                        vprintf_like_t telnet_log_printer = esp_log_set_vprintf(serial_log_printer);
                        serial_log_printer = NULL;
                    }

                } else{

                    (*g_app_cli_cb)(cmd);
                    
                }

                for(int i = 0; i < index; i++)
                    cmd[i] = 0;

                index = 0;
                
                if(serial_log_printer != NULL)
                    send(sock, prompt, 4, 0);

            }else{

                cmd[index++] = rx_buffer[0];

            }
        }
    } while(1);

    // Sending log via serial connection
    vprintf_like_t telnet_log_printer = esp_log_set_vprintf(serial_log_printer);
}

static void telnet_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
#ifdef CONFIG_EXAMPLE_IPV6
    else if (addr_family == AF_INET6) {
        struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
        bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
        dest_addr_ip6->sin6_family = AF_INET6;
        dest_addr_ip6->sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }
#endif

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto clean_up;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto clean_up;
    }

    ESP_LOGI(TAG, "Socket listening");

    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t addr_len = sizeof(source_addr);
    sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        goto clean_up;
    }

    // Convert ip address to string
    if (source_addr.ss_family == PF_INET) {
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    }
#ifdef CONFIG_EXAMPLE_IPV6
    else if (source_addr.ss_family == PF_INET6) {
        inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
    }
#endif
    ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);
    handle_command_request(sock);
    shutdown(sock, 0);
    close(sock);


clean_up:
    close(listen_sock);

vTaskDelete(NULL);
}

void telnet_client_init(app_cli_cb_t app_cli_cb)
{

    printf("\nOpen a new terminal and use the following command to connect with the server: telnet <ip_address>\n");

    g_app_cli_cb = app_cli_cb;

#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(telnet_server_task, "telnet_server", 8192, (void*)AF_INET, 5, NULL);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(telnet_server_task, "telnet_server", 8192, (void*)AF_INET6, 5, NULL);
#endif
    return;
}

