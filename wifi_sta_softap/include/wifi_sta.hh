#pragma once

#include <string.h>
#include <common.hh>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_system.h>
#include <esp_check.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <esp_wifi.h>

#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>
#include <lwip/ip4_addr.h>
#include <driver/gpio.h>
#include <nvs.h>
#include <spi_flash_mmap.h>
#include <esp_sntp.h>
#include <time.h>

#define TAG "WIFISTA"

namespace WIFISTA
{
    enum class ConnectionState{
        UNCONNECTED,
        CONNECTING,
        CONNECTED,
        NONE,
    };
    esp_ip4_addr_t ipAddress{0};
    char* hostname{nullptr};
    esp_netif_t *wifi_netif_sta{nullptr};
    void (*_callbackOnSntpSet)(){nullptr};

    ConnectionState state{ConnectionState::UNCONNECTED};

    ConnectionState GetState(){
        return state;
    }

    static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:
            {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "Got IP-Address" IPSTR, IP2STR(&event->ip_info.ip));
                state=ConnectionState::CONNECTED;
                ipAddress=event->ip_info.ip;
                esp_sntp_init();
                break;
            }
            
        }
    }

    static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
         ESP_LOGD(TAG, "WIFI Event %ld", event_id);
         if (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED) {
            state=ConnectionState::CONNECTING;
            esp_wifi_connect();
         }
    }

    static void time_sync_notification_cb(struct timeval *tv)
    {
        time_t now;
        char strftime_buf[64];
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Real Time is available thanks to SNTP. The current date/time in Berlin is: %s", strftime_buf);
        if(_callbackOnSntpSet) _callbackOnSntpSet();
    }

    const char* GetHostname(){
        return hostname;
    }

    const esp_ip4_addr_t GetIpAddress(){
        return ipAddress;
    }

    esp_err_t InitAndRun(const char* ssid, const char* password, const char *hostnamePattern="esp32_%02x%02x%02x", bool init_netif_and_create_event_loop=true, void (*callbackOnSntpSet)()=nullptr)
    {
        esp_log_level_set("wifi", ESP_LOG_WARN);

        if (init_netif_and_create_event_loop)
        {
            ESP_ERROR_CHECK(esp_netif_init());
            ESP_ERROR_CHECK(esp_event_loop_create_default());
        }
        _callbackOnSntpSet=callbackOnSntpSet;
        
        wifi_netif_sta = esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

        wifi_config_t wifi_config_sta = {}; 
        strcpy((char *)wifi_config_sta.sta.ssid, ssid);
        strcpy((char *)wifi_config_sta.sta.password, password);


        
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        asprintf(&hostname, hostnamePattern, mac[3], mac[4], mac[5]);
        ESP_ERROR_CHECK(esp_netif_set_hostname(wifi_netif_sta, hostname));

        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(hostname));
        const char* MDNS_INSTANCE="WEBMAN_MDNS_INSTANCE";
        ESP_ERROR_CHECK(mdns_instance_name_set(MDNS_INSTANCE));

        ESP_LOGI(TAG, "about to connect to ssid %s with password %s.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
        ESP_ERROR_CHECK(esp_wifi_start());

        // prepare simple network time protocol client and start it, when we got an IP-Adress (see event handler)
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
        esp_sntp_setservername(0, "pool.ntp.org");
        sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        // Set timezone to Berlin
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
        return ESP_OK;
    }
}
#undef TAG