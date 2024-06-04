#include "dap_config.h"
#include "freertos/portmacro.h"
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_netif.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

#include "led.h"
#include "usb.h"
#include "delay.h"
#include "network-dap.h"
#include "dap.h"

#define PORT 2806
#define KEEPALIVE_IDLE 5
#define KEEPALIVE_INTERVAL 5
#define KEEPALIVE_COUNT 3
#define TAG "network-dap"

#define DAP_TAG "dap2"

#define DAP_SERVER_PRIORITY 5

typedef struct {
    bool connected;
    int socket_id;
} NetworkDAP;

static NetworkDAP network_dap;

void dap_callback_connect(void) {
    ESP_LOGI(TAG, "connected");
    led_set(0, 0, 0);
    network_dap.connected = true;
}

void dap_callback_disconnect(void) {
    ESP_LOGI(TAG, "disconnected");
    led_set(255, 0, 0);
    network_dap.connected = false;
}

bool dap_is_connected(void) {
    return network_dap.connected;
}

bool network_dap_connected(void) {
    return network_dap.connected;
}

static void receive_and_send_to_dap(void) {
    size_t rx_size = SIZE_MAX;
    size_t gdb_packet_size = DAP_CONFIG_PACKET_SIZE;
    uint8_t* buffer_rx = malloc(gdb_packet_size);

    uint8_t tx_data[DAP_CONFIG_PACKET_SIZE];

    TickType_t prev_send = 0;
    do {
        // size_t max_len = gdb_glue_get_free_size();
        size_t max_len = 64;
        if(max_len > gdb_packet_size) max_len = gdb_packet_size;
        led_set_red(255);
        rx_size = recv(network_dap.socket_id, buffer_rx, max_len, 0);

        // ESP_LOGW(TAG, "+%d", xTaskGetTickCount() - prev_send);
        if(rx_size > 0) {
            // ESP_LOGW(TAG, "Received %d bytes", rx_size);
            // gdb_glue_receive(buffer_rx, rx_size);
            // send(network_dap.socket_id, buffer_rx, rx_size, 0);
            TickType_t ticks_pre = xTaskGetTickCount();

            led_set_red(0);

            led_set_blue(255);
            size_t tx_size = dap_process_request(buffer_rx, rx_size, tx_data, sizeof(tx_data));
            led_set_blue(0);

            // ESP_LOGW(TAG, "Processed to %d bytes", (uint32_t)tx_size);

            led_set_green(255);
            send(network_dap.socket_id, tx_data, tx_size, 0);
            prev_send = xTaskGetTickCount();
            // ESP_LOGW(TAG, ">%d(%d)", tx_size, prev_send - ticks_pre);
            led_set_green(0);
        }
    } while(rx_size > 0);

    free(buffer_rx);
}

static void network_dap_server_task(void* pvParameters) {
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    network_dap.connected = false;
    struct sockaddr_storage dest_addr;

    if(addr_family == AF_INET) {
        struct sockaddr_in* dest_addr_ip4 = (struct sockaddr_in*)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
        // ip_protocol = IPPROTO_UDP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if(listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if(err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if(err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while(1) {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
        if(sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        // setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        int nodelay = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));
        // setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        // setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        // setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        // Convert ip address to string
        if(source_addr.ss_family == PF_INET) {
            inet_ntoa_r(
                ((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        // continue only if DAP-Link is not connected
        if(!dap_is_connected()) {
            led_set_blue(255);
            delay(5);
            led_set_blue(0);

            ESP_LOGI(TAG, "Accepting networked DAP-Link connection");

            network_dap.socket_id = sock;
            network_dap.connected = true;

            receive_and_send_to_dap();

            network_dap.connected = false;
            network_dap.socket_id = -1;

            led_set_blue(255);
            delay(5);
            led_set_blue(0);
        } else {
            ESP_LOGE(TAG, "DAP-Link is connected, not accepting connection");
        }

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void network_dap_server_init(void) {
    network_dap.connected = false;
    network_dap.socket_id = -1;

    esp_wifi_set_ps(WIFI_PS_NONE);
    xTaskCreate(network_dap_server_task, "network_dap_server", 4096, (void*)AF_INET, DAP_SERVER_PRIORITY, NULL);
}
