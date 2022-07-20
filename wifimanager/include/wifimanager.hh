#pragma once
#include <string.h>
#include <common.hh>
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
#include <esp_spi_flash.h>

#define TAG "WIFIMGR"

namespace WIFIMGR
{
    extern const char wifimanager_html_gz_start[] asm("_binary_wifimanager_html_gz_start");
    extern const size_t wifimanager_html_gz_size asm("wifimanager_html_gz_length");

    constexpr size_t MAX_AP_NUM = 15;
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

    constexpr wifi_auth_mode_t AP_AUTHMODE{wifi_auth_mode_t::WIFI_AUTH_WPA2_PSK};

    constexpr int ATTEMPTS_TO_RECONNECT {5};

    constexpr char UNIT_SEPARATOR{0x1F};
    constexpr char RECORD_SEPARATOR{0x1E};
    constexpr char GROUP_SEPARATOR{0x1D};
    constexpr char FILE_SEPARATOR{0x1C};
    constexpr size_t ONE_AP_LEN{120};

    constexpr char wifi_manager_nvs_namespace[]{"wifimgr"};

    enum class STA_STATE
    {
        NO_CONNECTION,
        SHOULD_CONNECT,      // Daten sind verfügbar, die passen könnten. Es soll beim nächsten Retry-Tick ein Verbindungsversuch gestartet werden. Gerade im Moment wurde aber noch kein Verbindungsversiuch gestartet. -->Scan möglich
        ABOUT_TO_CONNECT, // es wurde gerade ein Verbindungsaufbau gestartet, es ist aber noch nicht klar, ob der erfolgreich war -->Scan nicht möglich
        CONNECTED,
    };

    /**
     * @brief simplified reason codes for a lost connection.
     *
     * esp-idf maintains a big list of reason codes which in practice are useless for most typical application.
     */
    enum class UPDATE_REASON_CODE
    {
        NO_CHANGE = 0,
        CONNECTION_ESTABLISHED = 1,
        FAILED_ATTEMPT = 2,
        USER_DISCONNECT = 3,
        LOST_CONNECTION = 4
    };

    STA_STATE staState{STA_STATE::NO_CONNECTION};
    //bool accessPointIsActive{false}; // nein, diese Information kann über esp_wifi_get_mode() immer herausgefunden werden
    bool scanIsActive{false};
    int remainingAttempsToConnectAsSTA{0};

    esp_netif_t *wifi_netif_sta = nullptr;
    esp_netif_t *wifi_netif_ap = nullptr;

    wifi_config_t wifi_config_sta = {}; // 132byte
    wifi_config_t wifi_config_ap = {};  // 132byte
    wifi_scan_config_t scan_config;     // 28byte

    TimerHandle_t wifi_manager_retry_timer = nullptr;
    TimerHandle_t wifi_manager_shutdown_ap_timer = nullptr;
    SemaphoreHandle_t wifi_manager_mutex = nullptr;

    httpd_handle_t wifi_manager_httpd_handle = nullptr;

    wifi_ap_record_t accessp_records[MAX_AP_NUM]; // 80byte*15=1200byte
    uint16_t accessp_records_len;

    uint8_t *http_buffer{nullptr};
    size_t http_buffer_len{0};

    UPDATE_REASON_CODE urc{UPDATE_REASON_CODE::NO_CHANGE};

    bool apAvailable{false};

    void connectAsSTA()
    {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
        esp_wifi_scan_stop(); // for sanity
        ESP_LOGI(TAG, "Calling esp_wifi_connect() for WIFI_IF_STA with ssid %s and password %s.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
        ESP_ERROR_CHECK(esp_wifi_connect());
        staState=STA_STATE::ABOUT_TO_CONNECT;
    }

    esp_err_t delete_sta_config()
    {
        nvs_handle handle;
        esp_err_t ret = ESP_OK;
        ESP_LOGI(TAG, "About to erase config in flash!!");
        GOTO_ERROR_ON_ERROR(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle), "Unable to open nvs partition");
        GOTO_ERROR_ON_ERROR(nvs_erase_all(handle), "Unable to to erase all keys");
        ret = nvs_commit(handle);
    error:
        nvs_close(handle);
        return ret;
    }

    esp_err_t update_sta_config_lazy()
    {
        nvs_handle handle;
        esp_err_t ret = ESP_OK;
        char tmp_ssid[33];     /**< SSID of target AP. */
        char tmp_password[64]; /**< Password of target AP. */
        bool change{false};
        size_t sz{0};

        ESP_LOGI(TAG, "About to save config to flash!!");
        GOTO_ERROR_ON_ERROR(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle), "Unable to open nvs partition");
        sz = sizeof(tmp_ssid);
        ret = nvs_get_str(handle, "ssid", tmp_ssid, &sz);
        if ((ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_ssid, (char *)wifi_config_sta.sta.ssid) != 0)
        {
            /* different ssid or ssid does not exist in flash: save new ssid */
            GOTO_ERROR_ON_ERROR(nvs_set_str(handle, "ssid", (const char *)wifi_config_sta.sta.ssid), "Unable to nvs_set_str(handle, \"ssid\", ssid_sta)");
            ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: ssid:%s", wifi_config_sta.sta.ssid);
            change = true;
        }

        sz = sizeof(tmp_password);
        ret = nvs_get_str(handle, "password", tmp_password, &sz);
        if ((ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_password, (char *)wifi_config_sta.sta.password) != 0)
        {
            /* different password or password does not exist in flash: save new password */
            GOTO_ERROR_ON_ERROR(nvs_set_str(handle, "password", (const char *)wifi_config_sta.sta.password), "Unable to nvs_set_str(handle, \"password\", password_sta)");
            ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password:%s", wifi_config_sta.sta.password);
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
        return ret;
    }

    bool read_sta_config()
    {
        nvs_handle handle;
        esp_err_t ret = ESP_OK;
        size_t sz;
        GOTO_ERROR_ON_ERROR(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle), "Unable to open nvs partition");
        sz = sizeof(wifi_config_sta.sta.ssid);
        ret = nvs_get_str(handle, "ssid", (char*)wifi_config_sta.sta.ssid, &sz);
        if (ret != ESP_OK)
        {
            ESP_LOGI(TAG, "Unable to read ssid");
            goto error;
        }

        sz = sizeof(wifi_config_sta.sta.password);
        ret = nvs_get_str(handle, "password", (char*)wifi_config_sta.sta.password, &sz);
        if (ret != ESP_OK)
        {
            ESP_LOGI(TAG, "Unable to read password");
            goto error;
        }
        ESP_LOGI(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
        ret = (wifi_config_sta.sta.ssid[0] == '\0') ? ESP_FAIL : ESP_OK;
    error:
        nvs_close(handle);
        return ret;
    }
  
    void wifi_manager_timer_retry_cb(TimerHandle_t xTimer)
    {
        ESP_LOGI(TAG, "Retry Timer Tick!");
        xTimerStop(xTimer, (TickType_t)0);
        xSemaphoreTake(wifi_manager_mutex, portMAX_DELAY);
        if (staState == STA_STATE::SHOULD_CONNECT)
        {
            connectAsSTA();
        }
        xSemaphoreGive(wifi_manager_mutex);
    }

    void wifi_manager_timer_shutdown_ap_cb(TimerHandle_t xTimer)
    {
        ESP_LOGI(TAG, "ShutdownAP Timer Tick!");
        xTimerStop(xTimer, (TickType_t)0);
        xSemaphoreTake(wifi_manager_mutex, portMAX_DELAY);
        if (staState == STA_STATE::CONNECTED)
        {
            esp_wifi_set_mode(WIFI_MODE_STA);
        }
        xSemaphoreGive(wifi_manager_mutex);
    }

    static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
        case WIFI_EVENT_SCAN_DONE:
        {
            ESP_LOGD(TAG, "WIFI_EVENT_SCAN_DONE");
            scanIsActive=false;
            wifi_event_sta_scan_done_t *event_sta_scan_done = (wifi_event_sta_scan_done_t *)event_data;
            if (event_sta_scan_done->status == 1)
                break; // break, falls der Scan nicht erfolgreich abgeschlossen wurde (wegen Abbruch oder einem voreiligen Neustart)
            xSemaphoreTake(wifi_manager_mutex, portMAX_DELAY);
            // As input param, it stores max AP number ap_records can hold. As output param, it receives the actual AP number this API returns.
            // As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
            accessp_records_len = MAX_AP_NUM;
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&accessp_records_len, accessp_records));
            xSemaphoreGive(wifi_manager_mutex);
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            //wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
            xSemaphoreTake(wifi_manager_mutex, portMAX_DELAY);
            scanIsActive = false; // if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan to never work again. For this reason SCAN_BIT is cleared too
            // if there was a timer on to stop the AP, well now it's time to cancel that since connection was lost! */
            xTimerStop(wifi_manager_shutdown_ap_timer, portMAX_DELAY);
            switch (staState)
            {
            case STA_STATE::NO_CONNECTION:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED, when STA_STATE::NO_CONNECTION --> disconnection was requested by user. Start Access Point"); 
                urc = UPDATE_REASON_CODE::USER_DISCONNECT;
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
                break;
            case STA_STATE::CONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED, when STA_STATE::CONNECTED --> unexpected disconnection. Try to reconnect %d times.", ATTEMPTS_TO_RECONNECT); 
                // Die Verbindung bestand zuvor und wurde jetzt getrennt. Versuche, erneut zu verbinden
                urc = UPDATE_REASON_CODE::LOST_CONNECTION;
                staState = STA_STATE::SHOULD_CONNECT;
                remainingAttempsToConnectAsSTA = ATTEMPTS_TO_RECONNECT;
                xTimerStart(wifi_manager_retry_timer, portMAX_DELAY);
                break;
            case STA_STATE::ABOUT_TO_CONNECT:
                remainingAttempsToConnectAsSTA--;
                //Die Verbindung war bereits getrennt und es wurd über den Retry Timer versucht, diese neu aufzubauen. Das schlug fehl
                urc = UPDATE_REASON_CODE::LOST_CONNECTION;
                if(remainingAttempsToConnectAsSTA<=0){
                    ESP_LOGW(TAG, "After (several?) attemps it was not possible to establish connection as STA with ssid %s and password %s. Start AccessPoint mode with ssid %s and password %s.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password, wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
                    staState = STA_STATE::NO_CONNECTION;
                    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
                }
                else{
                    ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED, when STA_STATE::ABOUT_TO_CONNECT --> disconnection occured earlier and we tried to establish it again...which was not successful. Still %d attempt(s) to go.", remainingAttempsToConnectAsSTA); 
                    staState = STA_STATE::SHOULD_CONNECT;
                    xTimerStart(wifi_manager_retry_timer, portMAX_DELAY);
                }
                break;
            default:
                ESP_LOGE(TAG, "In WIFI_EVENT_STA_DISCONNECTED Event loop: Unexpected state %d", (int)staState);
                break;
            }
            xSemaphoreGive(wifi_manager_mutex);
            break;
        }
        case WIFI_EVENT_AP_START:
        {
            ESP_LOGI(TAG, "Successfully started Access Point with ssid %s and password %s.", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
            apAvailable=true;   
            break;
        }
        case WIFI_EVENT_AP_STOP:
        {
            ESP_LOGI(TAG, "Successfully closed Access Point.");    
            apAvailable=false;
            break;
        }
        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station " MACSTR " joined this AccessPoint, AID=%d", MAC2STR(event->mac), event->aid);
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Station " MACSTR " leaved this AccessPoint, AID=%d", MAC2STR(event->mac), event->aid);
            break;
        }
        }
    }

    static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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
            ip_event_got_ip_t *ip_event_got_ip = (ip_event_got_ip_t *)malloc(sizeof(ip_event_got_ip_t));
            *ip_event_got_ip = *((ip_event_got_ip_t *)event_data);
            staState=STA_STATE::CONNECTED;
            update_sta_config_lazy();
            xSemaphoreTake(wifi_manager_mutex, portMAX_DELAY);
            urc = UPDATE_REASON_CODE::CONNECTION_ESTABLISHED;
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            if(mode==WIFI_MODE_APSTA){
                 xTimerStart(wifi_manager_shutdown_ap_timer, portMAX_DELAY);
            }
            xSemaphoreGive(wifi_manager_mutex);
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

    esp_err_t handle_get_wifimanager(httpd_req_t *req)
    {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, wifimanager_html_gz_start, wifimanager_html_gz_size); // -1 = use strlen()
        return ESP_OK;
    }

    esp_err_t handle_delete_wifimanager(httpd_req_t *req)
    {
        ESP_LOGI(TAG, "Received HTTP request to delete wifi credentials. Wait 2000ms till actually disconnect...");     
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_sendstr(req, "Connection deleted successfully");
        vTaskDelay(pdMS_TO_TICKS(2000)); // warte 200ms, um die Beantwortung des Requests noch zu ermöglichen
        xSemaphoreTake(wifi_manager_mutex, portMAX_DELAY);
        staState=STA_STATE::NO_CONNECTION;
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        delete_sta_config();
        ESP_LOGI(TAG, "Disconnected as STA from ssid %s.", wifi_config_sta.sta.ssid);             
        xSemaphoreGive(wifi_manager_mutex);
        return ESP_OK;
    }

    esp_err_t handle_post_wifimanager(httpd_req_t *req)
    {
        esp_err_t ret{ESP_OK};
        RETURN_FAIL_ON_FALSE(req->content_len < http_buffer_len, "req->content_len(%d) < http_buffer_len(%d)", req->content_len, http_buffer_len);

        char *buf = (char *)http_buffer;
        int received = httpd_req_recv(req, buf, http_buffer_len);
        RETURN_FAIL_ON_FALSE(received == req->content_len, "received (%d) == req->content_len (%d)", received, req->content_len);

        buf[req->content_len] = '\0';
        char delimiter[] = {UNIT_SEPARATOR, '\0'};
        char *ptr;
        size_t len;
        size_t pos{0};
        xSemaphoreTake(wifi_manager_mutex, portMAX_DELAY);
        if (req->content_len == 0)
        {
            ESP_LOGD(TAG, "In handle_post_wifimanager(): req->content_len==0");
            goto response;
        }
        ptr = strtok(buf, delimiter);
        ESP_GOTO_ON_FALSE(ptr, ESP_FAIL, response, TAG, "In handle_post_wifimanager(): no ssid found");
        len = strlen(ptr);
        ESP_GOTO_ON_FALSE(len <= MAX_SSID_LEN, ESP_FAIL, response, TAG, "SSID too long");
        strncpy((char *)wifi_config_sta.sta.ssid, ptr, len);
        ptr = strtok(NULL, delimiter);
        ESP_GOTO_ON_FALSE(ptr, ESP_FAIL, response, TAG, "In handle_post_wifimanager(): no password found");
        len = strlen(ptr);
        ESP_GOTO_ON_FALSE(len <= MAX_PASSPHRASE_LEN, ESP_FAIL, response, TAG, "PASSPHRASE too long");    
        strncpy((char *)wifi_config_sta.sta.password, ptr, len);
        ESP_LOGI(TAG, "Got a new SSID %s and PASSWORD %s from browser. wifi_config_sta is marked dirty.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
        remainingAttempsToConnectAsSTA=1;
        connectAsSTA();
    response:
        esp_netif_ip_info_t ip_info = {};
        esp_netif_get_ip_info(wifi_netif_sta, &ip_info);
        const char *hostname;
        esp_netif_get_hostname(wifi_netif_sta, &hostname);
        wifi_ap_record_t ap;
        esp_wifi_sta_get_ap_info(&ap);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%s%c%s%c%s%c%d%c" IPSTR "%c" IPSTR "%c" IPSTR "%c%d%c", wifi_config_ap.ap.ssid, UNIT_SEPARATOR, hostname, UNIT_SEPARATOR, wifi_config_sta.sta.ssid, UNIT_SEPARATOR, ap.rssi, UNIT_SEPARATOR, IP2STR(&ip_info.ip), UNIT_SEPARATOR, IP2STR(&ip_info.netmask), UNIT_SEPARATOR, IP2STR(&ip_info.gw), UNIT_SEPARATOR, (int)urc, RECORD_SEPARATOR);
        for (size_t i = 0; i < accessp_records_len; i++)
        {
            wifi_ap_record_t ap = accessp_records[i];
            pos += snprintf(buf + pos, http_buffer_len - pos, "%s%c%d%c%d%c%d%c", ap.ssid, UNIT_SEPARATOR, ap.primary, UNIT_SEPARATOR, ap.rssi, UNIT_SEPARATOR, ap.authmode, RECORD_SEPARATOR);
            if (pos >= http_buffer_len)
            {
                ESP_LOGE(TAG, "Problems in handle_put_wifimanager: Buffer too small");
                xSemaphoreGive(wifi_manager_mutex);
                return ESP_FAIL;
            }
        }
        urc = UPDATE_REASON_CODE::NO_CHANGE;
        // init a scan, if there is no scan active right now and the system is not establishing a connection right now
        
        if (!scanIsActive && staState!=STA_STATE::ABOUT_TO_CONNECT)
        {
            esp_wifi_scan_start(&scan_config, false);
            scanIsActive=true;
        }
        xSemaphoreGive(wifi_manager_mutex);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, (char *)buf, pos);
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

    esp_err_t InitAndRun(bool resetStoredConnection, uint8_t *http_request_response_buffer, size_t http_request_response_buffer_len)
    {
        http_buffer = http_request_response_buffer;
        http_buffer_len = http_request_response_buffer_len;
        if (wifi_manager_mutex != nullptr)
        {
            ESP_LOGE(TAG, "wifi manager already started");
            return ESP_FAIL;
        }
        if (strlen(CONFIG_NETWORK_WIFI_AP_PASSWORD) < 8 && AP_AUTHMODE != WIFI_AUTH_OPEN)
        {
            ESP_LOGE(TAG, "Password too short for authentication. Minimal length is %d", 8);
            return ESP_FAIL;
        }

        wifi_manager_mutex = xSemaphoreCreateMutex();

        wifi_manager_retry_timer = xTimerCreate("retry timer", pdMS_TO_TICKS(WIFI_MANAGER_RETRY_TIMER), pdFALSE, (void *)0, wifi_manager_timer_retry_cb);
        wifi_manager_shutdown_ap_timer = xTimerCreate("shutdown_ap_timer", pdMS_TO_TICKS(WIFI_MANAGER_SHUTDOWN_AP_TIMER), pdFALSE, (void *)0, wifi_manager_timer_shutdown_ap_cb);
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
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, NULL));

        // Prepare WIFI_CONFIGs for ap mode and for sta mode and for scan
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
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        if (resetStoredConnection)
        {
            ESP_LOGI(TAG, "Forced to delete saved wifi configuration.");
            delete_sta_config();  
        }
        if(resetStoredConnection || read_sta_config() != ESP_OK)
        {
            ESP_LOGI(TAG, "No saved wifi configuration found on startup. Starting access point and do an initial scan.");
            ESP_ERROR_CHECK(esp_wifi_start());
            esp_wifi_scan_start(&scan_config, false);
            scanIsActive=true;
        }
        else
        {
            ESP_LOGI(TAG, "Saved wifi found on startup. Will attempt to do an initial scan and then connect to ssid %s with password %s.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
            ESP_ERROR_CHECK(esp_wifi_start());
            esp_wifi_scan_start(&scan_config, true);
            remainingAttempsToConnectAsSTA=ATTEMPTS_TO_RECONNECT;
            ESP_ERROR_CHECK(esp_wifi_connect());
            staState=STA_STATE::ABOUT_TO_CONNECT;
        }
        
        ESP_LOGI(TAG, "Network Manager successfully started");
        return ESP_OK;
    }

    enum class SimpleState{
        OFFLINE,
        AP_AVAILABLE,
        STA_CONNECTED,
    };

    SimpleState GetState(){
        if(staState==STA_STATE::CONNECTED) return SimpleState::STA_CONNECTED;
        if(apAvailable) return SimpleState::AP_AVAILABLE;
        return SimpleState::OFFLINE;
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