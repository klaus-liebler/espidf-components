#pragma once
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>
#include <esp_system.h>
#include <esp_check.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi_types.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/ip4_addr.h>
#include <driver/gpio.h>
#include <nvs.h>
#include "dns_server.h"

#include <esp_spi_flash.h>

#define TAG "NET"

namespace wifimgr
{
    /**
     * @brief Defines the maximum number of access points that can be scanned.
     *
     * To save memory and avoid nasty out of memory errors,
     * we can limit the number of APs detected in a wifi scan.
     */
    constexpr size_t MAX_AP_NUM = 15;
    /**
     * @brief Defines the maximum number of failed retries allowed before the WiFi manager starts its own access point.
     * Setting it to 2 for instance means there will be 3 attempts in total (original request + 2 retries)
     */
    constexpr int WIFI_MANAGER_MAX_RETRY_START_AP = 5;
    /**
     * @brief Time (in ms) between each retry attempt
     * Defines the time to wait before an attempt to re-connect to a saved wifi is made after connection is lost or another unsuccesful attempt is made.
     */
    constexpr time_t WIFI_MANAGER_RETRY_TIMER = 5000;
    /**
     * @brief Time (in ms) to wait before shutting down the AP
     * Defines the time (in ms) to wait after a succesful connection before shutting down the access point.
     */
    constexpr time_t WIFI_MANAGER_SHUTDOWN_AP_TIMER = 60000;
    /** @brief Defines the task priority of the wifi_manager.
     *
     * Tasks spawn by the manager will have a priority of WIFI_MANAGER_TASK_PRIORITY-1.
     * For this particular reason, minimum task priority is 1. It it highly not recommended to set
     * it to 1 though as the sub-tasks will now have a priority of 0 which is the priority
     * of freeRTOS' idle task.
     */
    constexpr int WIFI_MANAGER_TASK_PRIORITY = 3;
    /** @brief Defines the auth mode as an access point
     *  Value must be of type wifi_auth_mode_t
     *  @see esp_wifi_types.h
     *  @warning if set to WIFI_AUTH_OPEN, passowrd me be empty. See DEFAULT_AP_PASSWORD.
     */
    constexpr wifi_auth_mode_t AP_AUTHMODE = WIFI_AUTH_WPA2_PSK;
    /** @brief Defines visibility of the access point. 0: visible AP. 1: hidden */

    /** @brief Defines access point's beacon interval. 100ms is the recommended default. */
    constexpr int DEFAULT_AP_BEACON_INTERVAL = 100;

    /** @brief Defines if esp32 shall run both AP + STA when connected to another AP.
     *  Value false will have the own AP always on (APSTA mode)
     *  Value true will turn off own AP when connected to another AP (STA only mode when connected)
     *  Turning off own AP when connected to another AP minimize channel interference and increase throughput
     */
    constexpr bool DEFAULT_STA_ONLY = true;

    /** @brief Defines if wifi power save shall be enabled.
     *  Value: WIFI_PS_NONE for full power (wifi modem always on)
     *  Value: WIFI_PS_MODEM for power save (wifi modem sleep periodically)
     *  Note: Power save is only effective when in STA only mode
     */
    constexpr wifi_ps_type_t DEFAULT_STA_POWER_SAVE = WIFI_PS_NONE;

    /* @brief indicate that the ESP32 is currently connected. */
    constexpr int BIT_IS_CONNECTED_TO_AP = BIT0;

    constexpr int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;

    /* @brief Set automatically once the SoftAP is started */
    constexpr int WIFI_MANAGER_AP_STARTED_BIT = BIT2;

    /* @brief When set, means a client requested to connect to an access point.*/
    constexpr int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;

    /* @brief This bit is set automatically as soon as a connection was lost */
    constexpr int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;

    /* @brief When set, means the wifi manager attempts to restore a previously saved connection at startup. */
    constexpr int WIFI_MANAGER_REQUEST_RESTORE_STA_BIT = BIT5;

    /* @brief When set, means a client requested to disconnect from currently connected AP. */
    constexpr int WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT = BIT6;

    /* @brief When set, means a scan is in progress */
    constexpr int WIFI_MANAGER_SCAN_BIT = BIT7;

    /* @brief When set, means user requested for a disconnect */
    constexpr int WIFI_MANAGER_REQUEST_DISCONNECT_BIT = BIT8;

    constexpr char UNIT_SEPARATOR{0x1F};
    constexpr char RECORD_SEPARATOR{0x1E};
    constexpr char GROUP_SEPARATOR{0x1D};
    constexpr char FILE_SEPARATOR{0x1C};
    constexpr size_t ONE_AP_LEN{120};

    constexpr char wifi_manager_nvs_namespace[]{"espwifimgr"};

    enum class message_code_t
    {
        WM_ORDER_START_WIFI_SCAN,
        WM_ORDER_LOAD_AND_RESTORE_STA,
        WM_ORDER_CONNECT_STA_USER,
        WM_ORDER_CONNECT_STA_RESTORE_CONNECTION,
        WM_ORDER_CONNECT_STA_AUTO_RECONNECT,
        WM_ORDER_DISCONNECT_STA,
        WM_ORDER_START_AP,
        WM_EVENT_STA_DISCONNECTED,
        WM_EVENT_SCAN_DONE,
        WM_EVENT_STA_GOT_IP,
        WM_ORDER_STOP_AP,
    };

    /**
     * @brief simplified reason codes for a lost connection.
     *
     * esp-idf maintains a big list of reason codes which in practice are useless for most typical application.
     */
    enum class UpdateReasonCode
    {
        NO_CHANGE=0,
        CONNECTION_ESTABLISHED = 1,
        FAILED_ATTEMPT = 2,
        USER_DISCONNECT = 3,
        LOST_CONNECTION = 4
    };

    struct queue_message
    {
        message_code_t code;
        void *param;
    };

    esp_netif_t *wifi_netif_sta = NULL;
    esp_netif_t *wifi_netif_ap = NULL;
    
    wifi_config_t wifi_config_sta = {};//132byte
    wifi_config_t wifi_config_ap = {};//132byte
    wifi_scan_config_t scan_config;//28byte

    QueueHandle_t wifi_manager_queue;
    TaskHandle_t wifi_manager_task_handle = NULL;
    TimerHandle_t wifi_manager_retry_timer = NULL;
    TimerHandle_t wifi_manager_shutdown_ap_timer = NULL;
    SemaphoreHandle_t nvs_sync_mutex = NULL;
    SemaphoreHandle_t wifi_manager_aplist_mutex = NULL;
    EventGroupHandle_t wifi_manager_event_group;
    httpd_handle_t wifi_manager_httpd_handle = NULL;

    wifi_ap_record_t accessp_records[MAX_AP_NUM]; //80byte*15=1200byte
    uint16_t accessp_records_len;

    char ssid_sta[MAX_SSID_LEN + 1];         // SSID of target AP, +1 for 0-termination
    char password_sta[MAX_PASSPHRASE_LEN + 1]; // Password of target AP., +1 for 0-termination


    uint8_t* http_buffer{nullptr};
    size_t   http_buffer_len{0};


    UpdateReasonCode urc{UpdateReasonCode::NO_CHANGE};

    BaseType_t wifi_manager_send_message(message_code_t code, void *param)
    {
        queue_message msg;
        msg.code = code;
        msg.param = param;
        return xQueueSend(wifi_manager_queue, &msg, portMAX_DELAY);
    }

    void wifi_manager_timer_retry_cb(TimerHandle_t xTimer)
    {
        ESP_LOGI(TAG, "Retry Timer Tick! Sending ORDER_CONNECT_STA with reason CONNECTION_REQUEST_AUTO_RECONNECT");
        xTimerStop(xTimer, (TickType_t)0);
        wifi_manager_send_message(message_code_t::WM_ORDER_CONNECT_STA_AUTO_RECONNECT, nullptr);
    }

    void wifi_manager_timer_shutdown_ap_cb(TimerHandle_t xTimer)
    {
        xTimerStop(xTimer, (TickType_t)0);
        wifi_manager_send_message(message_code_t::WM_ORDER_STOP_AP, nullptr);
    }

    static void wifi_manager_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base == WIFI_EVENT)
        {
            switch (event_id)
            {
            /* The scan-done event is triggered by esp_wifi_scan_start() and will arise in the following scenarios:
                  The scan is completed, e.g., the target AP is found successfully, or all channels have been scanned.
                  The scan is stopped by esp_wifi_scan_stop().
                  The esp_wifi_scan_start() is called before the scan is completed. A new scan will override the current
                     scan and a scan-done event will be generated.
                The scan-done event will not arise in the following scenarios:
                  It is a blocked scan.
                  The scan is caused by esp_wifi_connect().
                Upon receiving this event, the event task does nothing. The application event callback needs to call
                esp_wifi_scan_get_ap_num() and esp_wifi_scan_get_ap_records() to fetch the scanned AP list and trigger
                the Wi-Fi driver to free the internal memory which is allocated during the scan (do not forget to do this)!
             */
            case WIFI_EVENT_SCAN_DONE:
            {
                ESP_LOGD(TAG, "WIFI_EVENT_SCAN_DONE");
                xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
                wifi_event_sta_scan_done_t *event_sta_scan_done = (wifi_event_sta_scan_done_t *)malloc(sizeof(wifi_event_sta_scan_done_t));
                *event_sta_scan_done = *((wifi_event_sta_scan_done_t *)event_data);
                wifi_manager_send_message(message_code_t::WM_EVENT_SCAN_DONE, event_sta_scan_done);
                break;
            }
            /* If esp_wifi_connect() returns ESP_OK and the station successfully connects to the target AP, the connection event
             * will arise. Upon receiving this event, the event task starts the DHCP client and begins the DHCP process of getting
             * the IP address. Then, the Wi-Fi driver is ready for sending and receiving data. This moment is good for beginning
             * the application work, provided that the application does not depend on LwIP, namely the IP address. However, if
             * the application is LwIP-based, then you need to wait until the got ip event comes in. */
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
                break;

            /* This event can be generated in the following scenarios:
             *
             *     When esp_wifi_disconnect(), or esp_wifi_stop(), or esp_wifi_deinit(), or esp_wifi_restart() is called and
             *     the station is already connected to the AP.
             *
             *     When esp_wifi_connect() is called, but the Wi-Fi driver fails to set up a connection with the AP due to certain
             *     reasons, e.g. the scan fails to find the target AP, authentication times out, etc. If there are more than one AP
             *     with the same SSID, the disconnected event is raised after the station fails to connect all of the found APs.
             *
             *     When the Wi-Fi connection is disrupted because of specific reasons, e.g., the station continuously loses N beacons,
             *     the AP kicks off the station, the AP’s authentication mode is changed, etc.
             *
             * Upon receiving this event, the default behavior of the event task is: - Shuts down the station’s LwIP netif.
             * - Notifies the LwIP task to clear the UDP/TCP connections which cause the wrong status to all sockets. For socket-based
             * applications, the application callback can choose to close all sockets and re-create them, if necessary, upon receiving
             * this event.
             *
             * The most common event handle code for this event in application is to call esp_wifi_connect() to reconnect the Wi-Fi.
             * However, if the event is raised because esp_wifi_disconnect() is called, the application should not call esp_wifi_connect()
             * to reconnect. It’s application’s responsibility to distinguish whether the event is caused by esp_wifi_disconnect() or
             * other reasons. Sometimes a better reconnect strategy is required, refer to <Wi-Fi Reconnect> and
             * <Scan When Wi-Fi Is Connecting>.
             *
             * Another thing deserves our attention is that the default behavior of LwIP is to abort all TCP socket connections on
             * receiving the disconnect. Most of time it is not a problem. However, for some special application, this may not be
             * what they want, consider following scenarios:
             *
             *    The application creates a TCP connection to maintain the application-level keep-alive data that is sent out
             *    every 60 seconds.
             *
             *    Due to certain reasons, the Wi-Fi connection is cut off, and the <WIFI_EVENT_STA_DISCONNECTED> is raised.
             *    According to the current implementation, all TCP connections will be removed and the keep-alive socket will be
             *    in a wrong status. However, since the application designer believes that the network layer should NOT care about
             *    this error at the Wi-Fi layer, the application does not close the socket.
             *
             *    Five seconds later, the Wi-Fi connection is restored because esp_wifi_connect() is called in the application
             *    event callback function. Moreover, the station connects to the same AP and gets the same IPV4 address as before.
             *
             *    Sixty seconds later, when the application sends out data with the keep-alive socket, the socket returns an error
             *    and the application closes the socket and re-creates it when necessary.
             *
             * In above scenario, ideally, the application sockets and the network layer should not be affected, since the Wi-Fi
             * connection only fails temporarily and recovers very quickly. The application can enable “Keep TCP connections when
             * IP changed” via LwIP menuconfig.*/
            case WIFI_EVENT_STA_DISCONNECTED:
            {
                ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");

                wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t *)malloc(sizeof(wifi_event_sta_disconnected_t));
                *wifi_event_sta_disconnected = *((wifi_event_sta_disconnected_t *)event_data);

                /* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan to never work again. For this reason SCAN_BIT is cleared too */
                xEventGroupClearBits(wifi_manager_event_group, BIT_IS_CONNECTED_TO_AP | WIFI_MANAGER_SCAN_BIT);

                /* post disconnect event with reason code */
                wifi_manager_send_message(message_code_t::WM_EVENT_STA_DISCONNECTED, (void *)wifi_event_sta_disconnected);
                break;
            }
            case WIFI_EVENT_AP_START:{
                esp_netif_ip_info_t ip_info;
                ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
                esp_netif_get_ip_info(wifi_netif_ap, &ip_info);
                wifi_config_t conf={};
                esp_wifi_get_config(WIFI_IF_AP, &conf);
                ESP_LOGI(TAG, "Set up Wifi Access Point with ssid %s and password %s and IP:" IPSTR, conf.ap.ssid, conf.ap.password, IP2STR(&ip_info.ip));
                xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
                break;
            }
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
                xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
                break;

            /* Every time a station is connected to ESP32 AP, the <WIFI_EVENT_AP_STACONNECTED> will arise. Upon receiving this
             * event, the event task will do nothing, and the application callback can also ignore it. However, you may want
             * to do something, for example, to get the info of the connected STA, etc. */
            case WIFI_EVENT_AP_STACONNECTED:
            {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
                break;
            }
            /* This event can happen in the following scenarios:
             *   The application calls esp_wifi_disconnect(), or esp_wifi_deauth_sta(), to manually disconnect the station.
             *   The Wi-Fi driver kicks off the station, e.g. because the AP has not received any packets in the past five minutes, etc.
             *   The station kicks off the AP.
             * When this event happens, the event task will do nothing, but the application event callback needs to do
             * something, e.g., close the socket which is related to this station, etc. */
            case WIFI_EVENT_AP_STADISCONNECTED:
            {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
                break;
            }
            /* This event is disabled by default. The application can enable it via API esp_wifi_set_event_mask().
             * When this event is enabled, it will be raised each time the AP receives a probe request. */
            case WIFI_EVENT_AP_PROBEREQRECVED:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
                break;

            } /* end switch */
        }
        else if (event_base == IP_EVENT)
        {

            switch (event_id)
            {

            /* This event arises when the DHCP client successfully gets the IPV4 address from the DHCP server,
             * or when the IPV4 address is changed. The event means that everything is ready and the application can begin
             * its tasks (e.g., creating sockets).
             * The IPV4 may be changed because of the following reasons:
             *    The DHCP client fails to renew/rebind the IPV4 address, and the station’s IPV4 is reset to 0.
             *    The DHCP client rebinds to a different address.
             *    The static-configured IPV4 address is changed.
             * Whether the IPV4 address is changed or NOT is indicated by field ip_change of ip_event_got_ip_t.
             * The socket is based on the IPV4 address, which means that, if the IPV4 changes, all sockets relating to this
             * IPV4 will become abnormal. Upon receiving this event, the application needs to close all sockets and recreate
             * the application when the IPV4 changes to a valid one. */
            case IP_EVENT_STA_GOT_IP:
            {
                ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
                xEventGroupSetBits(wifi_manager_event_group, BIT_IS_CONNECTED_TO_AP);
                ip_event_got_ip_t *ip_event_got_ip = (ip_event_got_ip_t *)malloc(sizeof(ip_event_got_ip_t));
                *ip_event_got_ip = *((ip_event_got_ip_t *)event_data);
                wifi_manager_send_message(message_code_t::WM_EVENT_STA_GOT_IP, (void *)(ip_event_got_ip));
                break;
            }
            /* This event arises when the IPV4 address become invalid.
             * IP_STA_LOST_IP doesn’t arise immediately after the WiFi disconnects, instead it starts an IPV4 address lost timer,
             * if the IPV4 address is got before ip lost timer expires, IP_EVENT_STA_LOST_IP doesn’t happen. Otherwise, the event
             * arises when IPV4 address lost timer expires.
             * Generally the application don’t need to care about this event, it is just a debug event to let the application
             * know that the IPV4 address is lost. */
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(TAG, "IP_EVENT_STA_LOST_IP");
                break;
            }
        }
    }

    esp_err_t erase_sta_config(){
        nvs_handle handle;
        esp_err_t ret=ESP_OK;
        ESP_LOGI(TAG, "About to erase config in flash!!");
        RETURN_FAIL_ON_FALSE(xSemaphoreTake(nvs_sync_mutex, portMAX_DELAY), "Unable to aquire lock");
        GOTO_ERROR_ON_ERROR(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle), "Unable to open nvs partition");
        GOTO_ERROR_ON_ERROR(nvs_erase_all(handle),"Unable to to erase all keys");
        ret = nvs_commit(handle);
    error:
        nvs_close(handle);
        xSemaphoreGive(nvs_sync_mutex);
        return ret;
    }

    esp_err_t wifi_manager_save_sta_config()
    {
        nvs_handle handle;
        esp_err_t ret = ESP_OK;
        char tmp_ssid[33];     /**< SSID of target AP. */
        char tmp_password[64]; /**< Password of target AP. */
        bool change{false};
        size_t sz{0};

        ESP_LOGI(TAG, "About to save config to flash!!");
        RETURN_FAIL_ON_FALSE(xSemaphoreTake(nvs_sync_mutex, portMAX_DELAY), "Unable to aquire lock");
        GOTO_ERROR_ON_ERROR(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle), "Unable to open nvs partition");
        sz = sizeof(tmp_ssid);
        ret = nvs_get_str(handle, "ssid", tmp_ssid, &sz);
        if ((ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_ssid, (char *)ssid_sta) != 0)
        {
            /* different ssid or ssid does not exist in flash: save new ssid */
            GOTO_ERROR_ON_ERROR(nvs_set_str(handle, "ssid", ssid_sta),  "Unable to nvs_set_str(handle, \"ssid\", ssid_sta)");
            ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: ssid:%s", ssid_sta);
            change = true;
        }

        sz = sizeof(tmp_password);
        ret = nvs_get_str(handle, "password", tmp_password, &sz);
        if ((ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_password, (char *)password_sta) != 0)
        {
            /* different password or password does not exist in flash: save new password */
            GOTO_ERROR_ON_ERROR(nvs_set_str(handle, "password", password_sta), "Unable to nvs_set_str(handle, \"password\", password_sta)");
            ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password:%s", password_sta);
            change = true;
        }
        if (change)
        {
            ret = nvs_commit(handle);
        }
        else
        {
            ESP_LOGI(TAG, "Wifi config was not saved to flash because no change has been detected.");
        }
    error:
        nvs_close(handle);
        xSemaphoreGive(nvs_sync_mutex);
        return ret;
    }

    bool wifi_manager_fetch_wifi_sta_config()
    {
        nvs_handle handle;
        esp_err_t ret=ESP_OK;
        char buff[64];
        size_t sz;
        RETURN_FAIL_ON_FALSE(xSemaphoreTake(nvs_sync_mutex, portMAX_DELAY), "Unable to aquire lock");
        GOTO_ERROR_ON_ERROR(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle), "Unable to open nvs partition");
        sz = sizeof(ssid_sta);
        ret =  nvs_get_str(handle, "ssid", buff, &sz);                                                                
        if (ret != ESP_OK) {                                                      
            ESP_LOGI(TAG, "Unable to read ssid");                                                     
            goto error;                                                                      
        } 
        
        memcpy(ssid_sta, buff, sz);
        sz = sizeof(password_sta);
        ret = nvs_get_str(handle, "password", buff, &sz);
        if (ret != ESP_OK) {                                                      
            ESP_LOGI(TAG, "Unable to read password");
            goto error;                                                                      
        } 
        memcpy(password_sta, buff, sz);
        ESP_LOGI(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s", ssid_sta, password_sta);
        ret = (ssid_sta[0] == '\0')?ESP_FAIL:ESP_OK;
    error:
        nvs_close(handle);
        xSemaphoreGive(nvs_sync_mutex);
        return ret;
    }

    extern const char wifimanager_html_gz_start[] asm("_binary_wifimanager_html_gz_start");
    extern const size_t wifimanager_html_gz_size asm("wifimanager_html_gz_length");

    esp_err_t handle_get_wifimanager(httpd_req_t *req)
    {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, wifimanager_html_gz_start, wifimanager_html_gz_size); // -1 = use strlen()
        return ESP_OK;
    }

    esp_err_t handle_delete_wifimanager(httpd_req_t *req)
    {
        wifi_manager_send_message(message_code_t::WM_ORDER_DISCONNECT_STA, NULL);
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_sendstr(req, "Connection deleted successfully");
        return ESP_OK;
    }

    
    esp_err_t handle_post_wifimanager(httpd_req_t *req)
    {
        esp_err_t ret{ESP_OK};
        RETURN_FAIL_ON_FALSE(req->content_len < http_buffer_len, "req->content_len(%d) < http_buffer_len(%d)", req->content_len, http_buffer_len);
            
        char *buf = (char*)http_buffer;
        int received = httpd_req_recv(req, buf, http_buffer_len);
        RETURN_FAIL_ON_FALSE(received == req->content_len, "received (%d) == req->content_len (%d)", received, req->content_len);
            
        buf[req->content_len] = '\0';
        char delimiter[] = {UNIT_SEPARATOR, '\0'};
        char *ptr;
        size_t len;
        size_t pos{0};
        if (req->content_len == 0)
        {
            ESP_LOGD(TAG, "In handle_post_wifimanager(): req->content_len==0");
            goto response;
        }
        ptr = strtok(buf, delimiter);
        ESP_GOTO_ON_FALSE(ptr, ESP_FAIL, response, TAG, "In handle_post_wifimanager(): no ssid found");
            
        len = strlen(ptr);
        ESP_GOTO_ON_FALSE(len <= MAX_SSID_LEN, ESP_FAIL, response, TAG, "SSID too long");
       
        strncpy(ssid_sta, ptr, len);

        ptr = strtok(NULL, delimiter);
        ESP_GOTO_ON_FALSE(ptr, ESP_FAIL, response, TAG, "In handle_post_wifimanager(): no password found");

        len = strlen(ptr);

        ESP_GOTO_ON_FALSE(len <= MAX_PASSPHRASE_LEN, ESP_FAIL, response, TAG, "PASSPHRASE too long");
       
        strncpy(password_sta, ptr, len);
        ESP_LOGI(TAG, "Got a new SSID %s and PASSWORD %s from browser.", ssid_sta, password_sta);
        wifi_manager_send_message(message_code_t::WM_ORDER_CONNECT_STA_USER, nullptr);
    response:
        RETURN_FAIL_ON_FALSE(xSemaphoreTake(wifi_manager_aplist_mutex, portMAX_DELAY), "Cannot get aplist_mutex");

        esp_netif_ip_info_t ip_info = {};
        esp_netif_get_ip_info(wifi_netif_sta, &ip_info);
        const char* hostname;
        esp_netif_get_hostname(wifi_netif_sta, &hostname);
        wifi_ap_record_t ap;
        esp_wifi_sta_get_ap_info(&ap);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%s%c%s%c%s%c%d%c" IPSTR "%c" IPSTR "%c" IPSTR "%c%d%c", wifi_config_ap.ap.ssid, UNIT_SEPARATOR, hostname, UNIT_SEPARATOR, ssid_sta, UNIT_SEPARATOR, ap.rssi, UNIT_SEPARATOR, IP2STR(&ip_info.ip), UNIT_SEPARATOR, IP2STR(&ip_info.netmask), UNIT_SEPARATOR, IP2STR(&ip_info.gw), UNIT_SEPARATOR, (int)urc, RECORD_SEPARATOR);
        for (size_t i = 0; i < accessp_records_len; i++)
        {
            wifi_ap_record_t ap = accessp_records[i];
            pos += snprintf(buf + pos, http_buffer_len - pos, "%s%c%d%c%d%c%d%c", ap.ssid, UNIT_SEPARATOR, ap.primary, UNIT_SEPARATOR, ap.rssi, UNIT_SEPARATOR, ap.authmode, RECORD_SEPARATOR);
            if (pos >= http_buffer_len)
            {
                ESP_LOGE(TAG, "Problems in handle_put_wifimanager: Buffer too small");
                xSemaphoreGive(wifi_manager_aplist_mutex);
                return ESP_FAIL;
            }
        }
        urc=UpdateReasonCode::NO_CHANGE;
        xSemaphoreGive(wifi_manager_aplist_mutex);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, (char *)buf, pos);
        // init a scan, if there is no scan active right now
        auto uxBits = xEventGroupGetBits(wifi_manager_event_group);
        if (!(uxBits & WIFI_MANAGER_SCAN_BIT))
        {
            wifi_manager_send_message(message_code_t::WM_ORDER_START_WIFI_SCAN, NULL);
        }
        return ret;
    }

    static const httpd_uri_t get_wifimanager = {
        .uri = "/wifimanager",
        .method = HTTP_GET,
        .handler = handle_get_wifimanager,
        .user_ctx = nullptr,
    };

    static const httpd_uri_t post_wifimanager = {
        .uri = "/wifimanager",
        .method = HTTP_POST,
        .handler = handle_post_wifimanager,
        .user_ctx = nullptr,
    };

    static const httpd_uri_t delete_wifimanager = {
        .uri = "/wifimanager",
        .method = HTTP_DELETE,
        .handler = handle_delete_wifimanager,
        .user_ctx = nullptr,
    };

    void wifi_manager_task(void *pvParameters)
    {
        queue_message msg;
        BaseType_t xStatus;

        uint8_t retries = 0;
        while (true)
        {
            EventBits_t uxBits;
            xStatus = xQueueReceive(wifi_manager_queue, &msg, portMAX_DELAY);
            if (xStatus != pdPASS)
            {
                ESP_LOGE(TAG, "in wifi_manager_task: xStatus != pdPASS");
                continue;
            }
            switch (msg.code)
            {
            case message_code_t::WM_EVENT_SCAN_DONE:
            {
                wifi_event_sta_scan_done_t *evt_scan_done = (wifi_event_sta_scan_done_t *)msg.param;
                /* only check for AP if the scan is succesful */
                if (evt_scan_done->status != 0)
                {
                    break;
                }
                 if (xSemaphoreTake(wifi_manager_aplist_mutex, portMAX_DELAY) != pdTRUE)
                {
                    ESP_LOGE(TAG, "could not get access to json mutex in wifi_scan");
                    break;
                }
                // As input param, it stores max AP number ap_records can hold. As output param, it receives the actual AP number this API returns.
                // As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
                accessp_records_len = MAX_AP_NUM;
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&accessp_records_len, accessp_records));
                xSemaphoreGive(wifi_manager_aplist_mutex);
                free(evt_scan_done);
                break;
            }
            case message_code_t::WM_ORDER_START_WIFI_SCAN:
            {
                ESP_LOGD(TAG, "MESSAGE: ORDER_START_WIFI_SCAN");

                /* if a scan is already in progress this message is simply ignored thanks to the WIFI_MANAGER_SCAN_BIT uxBit */
                uxBits = xEventGroupGetBits(wifi_manager_event_group);
                if (uxBits & WIFI_MANAGER_SCAN_BIT)
                {
                    ESP_LOGW(TAG, "wifi_scan already in progress");
                    continue;
                }
                if (uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT || uxBits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT){
                    ESP_LOGW(TAG, "connection in progress scan start is not allowed!");
                    continue;
                }

                xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
                ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
                break;
            }
            case message_code_t::WM_ORDER_LOAD_AND_RESTORE_STA:
            {
                ESP_LOGI(TAG, "MESSAGE: ORDER_LOAD_AND_RESTORE_STA");
                if (wifi_manager_fetch_wifi_sta_config() == ESP_OK)
                {
                    ESP_LOGI(TAG, "Saved wifi found on startup. Will attempt to connect.");
                    wifi_manager_send_message(message_code_t::WM_ORDER_CONNECT_STA_RESTORE_CONNECTION, nullptr);
                }
                else
                {
                    /* no wifi saved: start soft AP! This is what should happen during a first run */
                    ESP_LOGI(TAG, "No saved wifi found on startup. Starting access point.");
                    wifi_manager_send_message(message_code_t::WM_ORDER_START_AP, nullptr);
                }
                break;
            }
            case message_code_t::WM_ORDER_CONNECT_STA_USER:
            case message_code_t::WM_ORDER_CONNECT_STA_RESTORE_CONNECTION:
            case message_code_t::WM_ORDER_CONNECT_STA_AUTO_RECONNECT:
            {
                ESP_LOGI(TAG, "MESSAGE: ORDER_CONNECT_STA");

                if (msg.code == message_code_t::WM_ORDER_CONNECT_STA_USER)
                {
                    xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
                }
                else if (msg.code == message_code_t::WM_ORDER_CONNECT_STA_RESTORE_CONNECTION || msg.code == message_code_t::WM_ORDER_CONNECT_STA_AUTO_RECONNECT)
                {
                    xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
                }

                uxBits = xEventGroupGetBits(wifi_manager_event_group);
                if (!(uxBits & BIT_IS_CONNECTED_TO_AP))
                {
                    strcpy((char *)wifi_config_sta.sta.ssid, ssid_sta);
                    strcpy((char *)wifi_config_sta.sta.password, password_sta);


                    /* update config to latest and attempt connection */
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));

                    /* if there is a wifi scan in progress abort it first
                       Calling esp_wifi_scan_stop will trigger a SCAN_DONE event which will reset this bit */
                    if (uxBits & WIFI_MANAGER_SCAN_BIT)
                    {
                        ESP_LOGI(TAG, "Stopping an ongoing scan");
                        esp_wifi_scan_stop();
                    }
                    ESP_LOGI(TAG, "Calling esp_wifi_connect() for WIFI_IF_STA with ssid %s and password %s.", ssid_sta, password_sta);
                    ESP_ERROR_CHECK(esp_wifi_connect());
                }
                break;
            }
            case message_code_t::WM_EVENT_STA_DISCONNECTED:
            {
                wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t *)msg.param;
                ESP_LOGI(TAG, "MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d", wifi_event_sta_disconnected->reason);

                /* this even can be posted in numerous different conditions
                 *
                 * 1. SSID password is wrong
                 * 2. Manual disconnection ordered
                 * 3. Connection lost
                 *
                 * Having clear understand as to WHY the event was posted is key to having an efficient wifi manager
                 *
                 * With wifi_manager, we determine:
                 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, We consider it's a client that requested the connection.
                 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, it's probably a password/something went wrong with the handshake.
                 *
                 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, it's a disconnection that was ASKED by the client (clicking disconnect in the app)
                 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, saved wifi is erased from the NVS memory.
                 *
                 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT and WIFI_MANAGER_REQUEST_STA_CONNECT_BIT are NOT set, it's a lost connection
                 * */

                /* if there was a timer on to stop the AP, well now it's time to cancel that since connection was lost! */
                if (xTimerIsTimerActive(wifi_manager_shutdown_ap_timer) == pdTRUE)
                {
                    xTimerStop(wifi_manager_shutdown_ap_timer, (TickType_t)0);
                }

                uxBits = xEventGroupGetBits(wifi_manager_event_group);
                if (uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT)
                {
                    /* there are no retries when it's a user requested connection by design. This avoids a user hanging too much
                     * in case they typed a wrong password for instance. Here we simply clear the request bit and move on */
                    xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
                    xSemaphoreTake(wifi_manager_aplist_mutex, portMAX_DELAY);
                    urc = UpdateReasonCode::FAILED_ATTEMPT;
                    xSemaphoreGive(wifi_manager_aplist_mutex);
                }
                else if (uxBits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT)
                {
                    /* user manually requested a disconnect so the lost connection is a normal event. Clear the flag and restart the AP */
                    xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);
                    xSemaphoreTake(wifi_manager_aplist_mutex, portMAX_DELAY);
                    ssid_sta[0] = '\0';
                    password_sta[0] = '\0';
                    urc = UpdateReasonCode::USER_DISCONNECT;
                    xSemaphoreGive(wifi_manager_aplist_mutex);
                    wifi_manager_save_sta_config();
                    wifi_manager_send_message(message_code_t::WM_ORDER_START_AP, NULL);
                }
                else
                {
                    xSemaphoreTake(wifi_manager_aplist_mutex, portMAX_DELAY);
                    urc = UpdateReasonCode::USER_DISCONNECT;
                    xSemaphoreGive(wifi_manager_aplist_mutex);

                    /* Start the timer that will try to restore the saved config */
                    xTimerStart(wifi_manager_retry_timer, (TickType_t)0);

                    /* if it was a restore attempt connection, we clear the bit */
                    xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);

                    /* if the AP is not started, we check if we have reached the threshold of failed attempt to start it */
                    if (!(uxBits & WIFI_MANAGER_AP_STARTED_BIT))
                    {

                        /* if the nunber of retries is below the threshold to start the AP, a reconnection attempt is made
                         * This way we avoid restarting the AP directly in case the connection is mementarily lost */
                        if (retries < WIFI_MANAGER_MAX_RETRY_START_AP)
                        {
                            retries++;
                        }
                        else
                        {
                            /* In this scenario the connection was lost beyond repair: kick start the AP! */
                            retries = 0;

                            /* start SoftAP */
                            wifi_manager_send_message(message_code_t::WM_ORDER_START_AP, NULL);
                        }
                    }
                }
                free(wifi_event_sta_disconnected);
                break;
            }
            case message_code_t::WM_ORDER_START_AP:
            {
                ESP_LOGI(TAG, "MESSAGE: ORDER_START_AP");

                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

                /* start DNS */
                //start_dns_server();
                break;
            }
            case message_code_t::WM_ORDER_STOP_AP:
            {
                ESP_LOGI(TAG, "MESSAGE: ORDER_STOP_AP");

                uxBits = xEventGroupGetBits(wifi_manager_event_group);

                /* before stopping the AP, we check that we are still connected. There's a chance that once the timer
                 * kicks in, for whatever reason the esp32 is already disconnected.
                 */
                if (uxBits & BIT_IS_CONNECTED_TO_AP)
                {

                    /* set to STA only */
                    esp_wifi_set_mode(WIFI_MODE_STA);

                    /* stop DNS */
                     //stop_dns_server();
                }

                break;
            }
            case message_code_t::WM_EVENT_STA_GOT_IP:
            {
                ESP_LOGI(TAG, "WM_EVENT_STA_GOT_IP");
                ip_event_got_ip_t *ip_event_got_ip = (ip_event_got_ip_t *)msg.param;
                uxBits = xEventGroupGetBits(wifi_manager_event_group);

                /* reset connection requests bits -- doesn't matter if it was set or not */
                xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

                /* save wifi config in NVS if it wasn't a restored of a connection */
                if (uxBits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT)
                {
                    xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
                }
                else
                {
                    wifi_manager_save_sta_config();
                }

                /* reset number of retries */
                retries = 0;
                xSemaphoreTake(wifi_manager_aplist_mutex, portMAX_DELAY);
                urc = UpdateReasonCode::CONNECTION_ESTABLISHED;
                xSemaphoreGive(wifi_manager_aplist_mutex);

                /* bring down DNS hijack */
                //stop_dns_server();

                /* start the timer that will eventually shutdown the access point
                 * We check first that it's actually running because in case of a boot and restore connection
                 * the AP is not even started to begin with.
                 */
                if (uxBits & WIFI_MANAGER_AP_STARTED_BIT)
                {
                    TickType_t t = pdMS_TO_TICKS(WIFI_MANAGER_SHUTDOWN_AP_TIMER);

                    /* if for whatever reason user configured the shutdown timer to be less than 1 tick, the AP is stopped straight away */
                    if (t > 0)
                    {
                        xTimerStart(wifi_manager_shutdown_ap_timer, (TickType_t)0);
                    }
                    else
                    {
                        wifi_manager_send_message(message_code_t::WM_ORDER_STOP_AP, nullptr);
                    }
                }
                free(ip_event_got_ip);
                break;
            }
            case message_code_t::WM_ORDER_DISCONNECT_STA:
            {
                ESP_LOGI(TAG, "MESSAGE: ORDER_DISCONNECT_STA, wait 2000ms...");
                vTaskDelay(pdMS_TO_TICKS(2000)); //warte 200ms, um die Beantwortung des Requests noch zu ermöglichen
                /* precise this is coming from a user request */
                xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);
                ESP_ERROR_CHECK(esp_wifi_disconnect());
                erase_sta_config();
                break;
            }
            default:
                break;
            }
        }
    }

    esp_err_t InitAndRun(bool resetStoredConnection, uint8_t* http_request_response_buffer, size_t http_request_response_buffer_len)
    {
        http_buffer=http_request_response_buffer;
        http_buffer_len=http_request_response_buffer_len;
        if (nvs_sync_mutex != nullptr || wifi_manager_aplist_mutex != nullptr)
        {
            ESP_LOGE(TAG, "wifi manager already started");
            return ESP_FAIL;
        }
        if (strlen(CONFIG_NETWORK_WIFI_AP_PASSWORD) < 8 && AP_AUTHMODE != WIFI_AUTH_OPEN)
        {
            ESP_LOGE(TAG, "Password too short for authentication. Minimal length is %d", 8);
            return ESP_FAIL;
        }
        nvs_sync_mutex = xSemaphoreCreateMutex();
        wifi_manager_aplist_mutex = xSemaphoreCreateMutex();
        wifi_manager_queue = xQueueCreate(3, sizeof(queue_message));
        wifi_manager_retry_timer = xTimerCreate("retry timer", pdMS_TO_TICKS(WIFI_MANAGER_RETRY_TIMER), pdFALSE, (void *)0, wifi_manager_timer_retry_cb);
        wifi_manager_shutdown_ap_timer = xTimerCreate("shutdown_ap_timer", pdMS_TO_TICKS(WIFI_MANAGER_SHUTDOWN_AP_TIMER), pdFALSE, (void *)0, wifi_manager_timer_shutdown_ap_cb);
        wifi_manager_event_group = xEventGroupCreate();
        if (!nvs_sync_mutex || !wifi_manager_aplist_mutex || !wifi_manager_queue || !wifi_manager_retry_timer || !wifi_manager_shutdown_ap_timer || !wifi_manager_event_group)
        {
            ESP_LOGE(TAG, "Unable to create all task sync structures");
            return ESP_FAIL;
        }

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        // Create and check netifs
        wifi_netif_sta = esp_netif_create_default_wifi_sta();
        wifi_netif_ap = esp_netif_create_default_wifi_ap();
        assert(wifi_netif_sta);
        assert(wifi_netif_ap);

        // init WIFI base
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

        // attach event handlers
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL, NULL));

        //Prepare WIFI_CONFIGs for ap mode and for sta mode and for scan
        wifi_config_sta.sta.scan_method = WIFI_FAST_SCAN;
        wifi_config_sta.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        wifi_config_sta.sta.threshold.rssi = -127;
        wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config_sta.sta.pmf_cfg.capable = true;
        wifi_config_sta.sta.pmf_cfg.required = false;
        
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char buffer[32];
        sprintf(buffer, CONFIG_NETWORK_WIFI_AP_SSID, mac[3], mac[4], mac[5]);
        
        wifi_config_ap.ap.ssid_len = 0;
        strcpy((char *)wifi_config_ap.ap.ssid, buffer);
        strcpy((char *)wifi_config_ap.ap.password, CONFIG_NETWORK_WIFI_AP_PASSWORD);
        wifi_config_ap.ap.channel = CONFIG_NETWORK_WIFI_AP_CHANNEL;
        wifi_config_ap.ap.max_connection = CONFIG_NETWORK_AP_MAX_AP_CONN;
        wifi_config_ap.ap.authmode = AP_AUTHMODE;

        scan_config.ssid = 0;
        scan_config.bssid = 0;
        scan_config.channel = 0;
        scan_config.show_hidden = true;

        sprintf(buffer, CONFIG_NETWORK_HOSTNAME_PATTERN, mac[3], mac[4], mac[5]);
        ESP_ERROR_CHECK(esp_netif_set_hostname(wifi_netif_sta, buffer));
        ESP_ERROR_CHECK(esp_netif_set_hostname(wifi_netif_ap, buffer));

        /* DHCP AP configuration */
        esp_netif_ip_info_t ap_ip_info = {};
        IP4_ADDR(&ap_ip_info.ip, 192, 168, 210, 0);
        IP4_ADDR(&ap_ip_info.netmask, 255, 255, 255, 0);
        IP4_ADDR(&ap_ip_info.gw, 192, 168, 210, 0);
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(wifi_netif_ap));
        ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_netif_ap, &ap_ip_info));
        ESP_ERROR_CHECK(esp_netif_dhcps_start(wifi_netif_ap));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
        //ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, wifi_settings.ap_bandwidth));
	    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        /* by default the mode is STA because wifi_manager will not start the access point unless it has to! */
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        if(resetStoredConnection){
            erase_sta_config();
            wifi_manager_send_message(message_code_t::WM_ORDER_START_AP, nullptr);
        }
        else{
            wifi_manager_send_message(message_code_t::WM_ORDER_LOAD_AND_RESTORE_STA, nullptr);
        }
        
        xTaskCreate(&wifi_manager_task, "wifi_manager", 4096, NULL, WIFI_MANAGER_TASK_PRIORITY, &wifi_manager_task_handle);
        ESP_LOGI(TAG, "Network Manager successfully started");
        return ESP_OK;
    }

    void RegisterHTTPDHandlers(httpd_handle_t httpd_handle)
    {
        wifi_manager_httpd_handle = httpd_handle;
        ESP_ERROR_CHECK(httpd_register_uri_handler(wifi_manager_httpd_handle, &get_wifimanager));
        ESP_ERROR_CHECK(httpd_register_uri_handler(wifi_manager_httpd_handle, &post_wifimanager));
        ESP_ERROR_CHECK(httpd_register_uri_handler(wifi_manager_httpd_handle, &delete_wifimanager));
    }

}
#undef TAG