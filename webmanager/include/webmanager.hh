#pragma once
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_wifi.h>

#include <hal/efuse_hal.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>
#include <lwip/ip4_addr.h>
#include <driver/gpio.h>
#include <driver/temperature_sensor.h>
#include <nvs.h>
#include <spi_flash_mmap.h>
#include <esp_sntp.h>
#include <time.h>
#include <common-esp32.hh>
#include <esp_log.h>
#include <sys/time.h>
#include "../dist_cpp_header/app_generated.h"

#define TAG "WMAN"
#define WLOG(mc, md) webmanager::M::GetSingleton()->Log(mc, md)

namespace messagecodes
{
#define DEF(thecode, thestring) thecode,
#define DEF_(thecode) thecode,
    enum class C
    {
        DEF_(NONE)
            DEF_(SW)
                DEF_(PANIC)
                    DEF_(BROWNOUT)
                        DEF_(WATCHDOG)
                            DEF_(SNTP)
#if __has_include(<messages.inc>)
#include <messages.inc>
#endif
    };
#undef DEF
#undef DEF_
#define DEF(thecode, thestring) thestring,
#define DEF_(thecode) #thecode,
    constexpr const char *N[] = {
        DEF_(SW)
            DEF_(PANIC)
                DEF_(BROWNOUT)
                    DEF_(WATCHDOG)
                        DEF_(SNTP)
#if __has_include(<messages.inc>)
#include <messages.inc>
#endif

    };
#undef DEF
#undef DEF_
}

namespace webmanager
{
    extern const char webmanager_html_gz_start[] asm("_binary_app_html_gz_start");
    extern const size_t webmanager_html_gz_length asm("app_html_gz_length");

    constexpr size_t MAX_AP_NUM = 15;
    constexpr int ATTEMPTS_TO_RECONNECT{5};

    constexpr char UNIT_SEPARATOR{0x1F};
    constexpr char RECORD_SEPARATOR{0x1E};
    constexpr char GROUP_SEPARATOR{0x1D};
    constexpr char FILE_SEPARATOR{0x1C};
    constexpr size_t STORAGE_LENGTH{16};

    constexpr time_t WIFI_MANAGER_RETRY_TIMER = 5000;
    constexpr time_t WIFI_MANAGER_SHUTDOWN_AP_TIMER = 60000;

    constexpr wifi_auth_mode_t AP_AUTHMODE{wifi_auth_mode_t::WIFI_AUTH_WPA2_PSK};

    constexpr char wifi_manager_nvs_namespace[]{"wifimgr"};

    enum class WifiStationState
    {
        NO_CONNECTION,
        SHOULD_CONNECT,   // Daten sind verfügbar, die passen könnten. Es soll beim nächsten Retry-Tick ein Verbindungsversuch gestartet werden. Gerade im Moment wurde aber noch kein Verbindungsversiuch gestartet. -->Scan möglich
        ABOUT_TO_CONNECT, // es wurde gerade ein Verbindungsaufbau gestartet, es ist aber noch nicht klar, ob der erfolgreich war -->Scan nicht möglich
        CONNECTED,
    };

    enum class UPDATE_REASON_CODE
    {
        NO_CHANGE = 0,
        CONNECTION_ESTABLISHED = 1,
        FAILED_ATTEMPT = 2,
        USER_DISCONNECT = 3,
        LOST_CONNECTION = 4
    };

    struct MessageLogEntry
    {
        uint32_t messageCode;
        uint32_t lastMessageData;
        uint32_t messageCount;
        time_t lastMessageTimestamp;

        MessageLogEntry(uint32_t messageCode, uint32_t lastMessageData, uint32_t messageCount, time_t lastMessageTimestamp) : messageCode(messageCode),
                                                                                                                              lastMessageData(lastMessageData),
                                                                                                                              messageCount(messageCount),
                                                                                                                              lastMessageTimestamp(lastMessageTimestamp)
        {
        }
        MessageLogEntry() : messageCode(0),
                            lastMessageData(0),
                            messageCount(0),
                            lastMessageTimestamp(0)
        {
        }

        bool operator<(const MessageLogEntry &str) const
        {
            return (lastMessageTimestamp < str.lastMessageTimestamp);
        }
    };

    class M;
    class M
    {
    private:
        static M *singleton;
        static __NOINIT_ATTR std::array<MessageLogEntry, STORAGE_LENGTH> BUFFER;
        bool hasRealtime{false};
        uint8_t *http_buffer{nullptr};
        size_t http_buffer_len{0};
        WifiStationState staState{WifiStationState::NO_CONNECTION};
        // bool accessPointIsActive{false}; // nein, diese Information kann über esp_wifi_get_mode() immer herausgefunden werden
        bool scanIsActive{false};
        int remainingAttempsToConnectAsSTA{0};

        esp_netif_t *wifi_netif_sta{nullptr};
        esp_netif_t *wifi_netif_ap{nullptr};

        wifi_config_t wifi_config_sta = {}; // 132byte
        wifi_config_t wifi_config_ap = {};  // 132byte
        wifi_scan_config_t scan_config;     // 28byte

        wifi_ap_record_t accessp_records[MAX_AP_NUM]; // 80byte*15=1200byte
        uint16_t accessp_records_len{0};

        SemaphoreHandle_t webmanager_semaphore{nullptr};

        TimerHandle_t wifi_manager_retry_timer{nullptr};
        TimerHandle_t wifi_manager_shutdown_ap_timer{nullptr};

        temperature_sensor_handle_t temp_handle{nullptr};

        UPDATE_REASON_CODE urc{UPDATE_REASON_CODE::NO_CHANGE};
        bool apAvailable{false};

        M()
        {

            struct timeval tv_now;
            gettimeofday(&tv_now, nullptr);
            time_t seconds_epoch = tv_now.tv_sec;
            if (seconds_epoch > 1684412222)
            { // epoch time when this code has been written
                ESP_LOGI(TAG, "As seconds_epoch = %llu, we expect having realtime", seconds_epoch);
                hasRealtime = true;
            }
        }

        void connectAsSTA()
        {
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
            esp_wifi_scan_stop(); // for sanity
            ESP_LOGI(TAG, "Calling esp_wifi_connect() for WIFI_IF_STA with ssid %s and password %s.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
            ESP_ERROR_CHECK(esp_wifi_connect());
            staState = WifiStationState::ABOUT_TO_CONNECT;
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

        esp_err_t read_sta_config()
        {
            nvs_handle handle;
            esp_err_t ret = ESP_OK;
            size_t sz;
            GOTO_ERROR_ON_ERROR(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle), "Unable to open nvs partition");
            sz = sizeof(wifi_config_sta.sta.ssid);
            ret = nvs_get_str(handle, "ssid", (char *)wifi_config_sta.sta.ssid, &sz);
            if (ret != ESP_OK)
            {
                ESP_LOGI(TAG, "Unable to read ssid");
                goto error;
            }

            sz = sizeof(wifi_config_sta.sta.password);
            ret = nvs_get_str(handle, "password", (char *)wifi_config_sta.sta.password, &sz);
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

        static void webmanager_timer_retry_cb_static(TimerHandle_t xTimer)
        {
            webmanager::M *myself = webmanager::M::GetSingleton();
            myself->webmanager_timer_retry_cb(xTimer);
        }
        void webmanager_timer_retry_cb(TimerHandle_t xTimer)
        {
            ESP_LOGI(TAG, "Retry Timer Tick!");
            xTimerStop(xTimer, (TickType_t)0);
            xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
            if (staState == WifiStationState::SHOULD_CONNECT)
            {
                connectAsSTA();
            }
            xSemaphoreGive(webmanager_semaphore);
        }

        static void webmanager_timer_shutdown_ap_cb_static(TimerHandle_t xTimer)
        {
            webmanager::M *myself = webmanager::M::GetSingleton();
            myself->webmanager_timer_shutdown_ap_cb(xTimer);
        }
        void webmanager_timer_shutdown_ap_cb(TimerHandle_t xTimer)
        {
            ESP_LOGI(TAG, "ShutdownAP Timer Tick!");
            xTimerStop(xTimer, (TickType_t)0);
            xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
            if (staState == WifiStationState::CONNECTED)
            {
                esp_wifi_set_mode(WIFI_MODE_STA);
            }
            xSemaphoreGive(webmanager_semaphore);
        }

        static void wifi_event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
        {
            webmanager::M *myself = static_cast<webmanager::M *>(arg);
            myself->wifi_event_handler(event_base, event_id, event_data);
        }
        void wifi_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
        {
            switch (event_id)
            {
            case WIFI_EVENT_SCAN_DONE:
            {
                ESP_LOGD(TAG, "WIFI_EVENT_SCAN_DONE");
                scanIsActive = false;
                wifi_event_sta_scan_done_t *event_sta_scan_done = (wifi_event_sta_scan_done_t *)event_data;
                if (event_sta_scan_done->status == 1)
                    break; // break, falls der Scan nicht erfolgreich abgeschlossen wurde (wegen Abbruch oder einem voreiligen Neustart)
                xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
                // As input param, it stores max AP number ap_records can hold. As output param, it receives the actual AP number this API returns.
                // As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
                accessp_records_len = MAX_AP_NUM;
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&accessp_records_len, accessp_records));
                xSemaphoreGive(webmanager_semaphore);
                break;
            }
            case WIFI_EVENT_STA_DISCONNECTED:
            {
                wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
                xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
                scanIsActive = false; // if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan to never work again. For this reason SCAN_BIT is cleared too
                // if there was a timer on to stop the AP, well now it's time to cancel that since connection was lost! */
                xTimerStop(wifi_manager_shutdown_ap_timer, portMAX_DELAY);
                switch (staState)
                {
                case WifiStationState::NO_CONNECTION:
                    ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED, when STA_STATE::NO_CONNECTION --> disconnection was requested by user. Start Access Point");
                    urc = UPDATE_REASON_CODE::USER_DISCONNECT;
                    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
                    break;
                case WifiStationState::CONNECTED:
                    ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED, when STA_STATE::CONNECTED --> unexpected disconnection. Try to reconnect %d times.", ATTEMPTS_TO_RECONNECT);
                    // Die Verbindung bestand zuvor und wurde jetzt getrennt. Versuche, erneut zu verbinden
                    urc = UPDATE_REASON_CODE::LOST_CONNECTION;
                    staState = WifiStationState::SHOULD_CONNECT;
                    remainingAttempsToConnectAsSTA = ATTEMPTS_TO_RECONNECT;
                    xTimerStart(wifi_manager_retry_timer, portMAX_DELAY);
                    break;
                case WifiStationState::ABOUT_TO_CONNECT:
                    remainingAttempsToConnectAsSTA--;
                    // Die Verbindung war bereits getrennt und es wurd über den Retry Timer versucht, diese neu aufzubauen. Das schlug fehl
                    urc = UPDATE_REASON_CODE::LOST_CONNECTION;
                    if (remainingAttempsToConnectAsSTA <= 0)
                    {
                        ESP_LOGW(TAG, "After (several?) attemps it was not possible to establish connection as STA with ssid %s and password %s (Reason %d). Start AccessPoint mode with ssid %s and password %s.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password, wifi_event_sta_disconnected->reason, wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
                        staState = WifiStationState::NO_CONNECTION;
                        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
                    }
                    else
                    {
                        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED, when STA_STATE::ABOUT_TO_CONNECT --> disconnection occured earlier and we tried to establish it again...which was not successful (Reason %d). Still %d attempt(s) to go.", wifi_event_sta_disconnected->reason, remainingAttempsToConnectAsSTA);
                        staState = WifiStationState::SHOULD_CONNECT;
                        xTimerStart(wifi_manager_retry_timer, portMAX_DELAY);
                    }
                    break;
                default:
                    ESP_LOGE(TAG, "In WIFI_EVENT_STA_DISCONNECTED Event loop: Unexpected state %d", (int)staState);
                    break;
                }
                xSemaphoreGive(webmanager_semaphore);
                break;
            }
            case WIFI_EVENT_AP_START:
            {
                esp_netif_ip_info_t ip_info = {};
                esp_netif_get_ip_info(wifi_netif_ap, &ip_info);
                ESP_LOGI(TAG, "Successfully started Access Point with ssid %s and password '%s'. Website is here: http://" IPSTR " . Webmanager is here: http://" IPSTR "/webmanager", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password, IP2STR(&ip_info.ip), IP2STR(&ip_info.ip));

                apAvailable = true;
                break;
            }
            case WIFI_EVENT_AP_STOP:
            {
                ESP_LOGI(TAG, "Successfully closed Access Point.");
                apAvailable = false;
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

        static void ip_event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
        {
            webmanager::M *myself = static_cast<webmanager::M *>(arg);
            myself->ip_event_handler(event_base, event_id, event_data);
        }
        void ip_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
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
                staState = WifiStationState::CONNECTED;
                update_sta_config_lazy();
                xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
                urc = UPDATE_REASON_CODE::CONNECTION_ESTABLISHED;
                wifi_mode_t mode;
                esp_wifi_get_mode(&mode);
                if (mode == WIFI_MODE_APSTA)
                {
                    xTimerStart(wifi_manager_shutdown_ap_timer, portMAX_DELAY);
                }
                xSemaphoreGive(webmanager_semaphore);
                esp_sntp_init();
                break;
            }
            /* This event arises when the IPV4 address become invalid.
             * IP_STA_LOST_IP doesn’t arise immediately after the WiFi disconnects, instead it starts an IPV4 address lost timer,
             * if the IPV4 address is got before ip lost timer expires, IP_EVENT_STA_LOST_IP doesn’t happen. Otherwise, the event
             * arises when IPV4 address lost timer expires.
             * Generally the application don’t need to care about this event, it is just a debug event to let the application
             * know that the IPV4 address is lost. */
            case IP_EVENT_STA_LOST_IP:
            {
                ESP_LOGI(TAG, "IP_EVENT_STA_LOST_IP");
                break;
            }
            }
        }

        static int logging_vprintf(const char *fmt, va_list l)
        {
            // Convert according to format
            char *buffer;
            int buffer_len = vasprintf(&buffer, fmt, l);
            // printf("logging_vprintf buffer_len=%d\n",buffer_len);
            // printf("logging_vprintf buffer=[%.*s]\n", buffer_len, buffer);
            if (buffer_len == 0)
            {
                return 0;
            }

            // Send MessageBuffer
            // printf("logging_vprintf sended=%d\n",sended);

            // Write to stdout
            return vprintf(fmt, l);
        }

        static esp_err_t handle_webmanager_ws_static(httpd_req_t *req)
        {
            webmanager::M *myself = static_cast<webmanager::M *>(req->user_ctx);
            return myself->handle_webmanager_ws(req);
        }
        esp_err_t handle_webmanager_ws(httpd_req_t *req)
        {
            if (req->method == HTTP_GET)
            {
                ESP_LOGI(TAG, "Handshake done, the new connection was opened");
                return ESP_OK;
            }
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;

            /* Set max_len = 0 to get the frame len */
            assert(httpd_ws_recv_frame(req, &ws_pkt, 0)==ESP_OK);

            ESP_LOGI(TAG, "frame len is %d, frame type is %d", ws_pkt.len, ws_pkt.type);
            if (ws_pkt.len == 0 || ws_pkt.type != HTTPD_WS_TYPE_BINARY)
            {
                return ESP_OK;
            }
            uint8_t *buf = (uint8_t *)malloc(ws_pkt.len);
            assert(buf);
            ws_pkt.payload = buf;
            esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
                free(buf);
                return ret;
            }
            auto mw = GetMessageWrapper(buf);
            switch (mw->message_type())
            {
            case webmanager::Message::Message_RequestSystemData:
                sendResponseSystemData(req, &ws_pkt);
                break;

            default:
                break;
            }
            free(buf);
            return ESP_OK;
        }

        esp_err_t doRestart()
        {
            esp_restart();
            return ESP_OK;
        }

        static esp_err_t handle_webmanager_get_static(httpd_req_t *req)
        {
            httpd_resp_set_type(req, "text/html");
            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            httpd_resp_send(req, webmanager_html_gz_start, webmanager_html_gz_length);
            return ESP_OK;
        }

        static esp_err_t handle_messagelog_get_static(httpd_req_t *req)
        {
            M *myself = static_cast<M *>(req->user_ctx);
            return myself->handle_messagelog_get(req);
        }

        esp_err_t handle_messagelog_get(httpd_req_t *req)
        {
            ESP_LOGI(TAG, "Creating Data");
            xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
            char *buf = (char *)http_buffer;
            size_t pos{0};
            std::sort(BUFFER.rbegin(), BUFFER.rend());

            for (int i = 0; i < BUFFER.size(); i++)
            {
                if (BUFFER[i].messageCode == 0)
                    continue;

                pos += snprintf(buf + pos, http_buffer_len - pos, "%llu%c%lu%c%s%c%lu%c%lu%c", BUFFER[i].lastMessageTimestamp, UNIT_SEPARATOR, BUFFER[i].messageCode, UNIT_SEPARATOR, messagecodes::N[BUFFER[i].messageCode], UNIT_SEPARATOR, BUFFER[i].lastMessageData, UNIT_SEPARATOR, BUFFER[i].messageCount, RECORD_SEPARATOR);
            }
            xSemaphoreGive(webmanager_semaphore);
            pos += snprintf(buf + pos, http_buffer_len - pos, "%c", GROUP_SEPARATOR);
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
            httpd_resp_set_hdr(req, "Pragma", "no-cache");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, (char *)buf, pos);
            return ESP_OK;
        }

        esp_err_t sendResponseSystemData(httpd_req_t *req, httpd_ws_frame_t *ws_pkt)
        {
            ESP_LOGI(TAG, "Prepare to send ResponseSystemData");
            flatbuffers::FlatBufferBuilder b(1024);
            const esp_partition_t *running = esp_ota_get_running_partition();
            esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
            std::vector<flatbuffers::Offset<PartitionInfo>> partitions_vector;
            while (it)
            {
                const esp_partition_t *p = esp_partition_get(it);
                esp_ota_img_states_t ota_state;
                esp_ota_get_state_partition(p, &ota_state);
                esp_app_desc_t app_info = {};
                esp_ota_get_partition_description(p, &app_info);
                auto labelOffset = b.CreateString(p->label);
                auto appnameOffset = b.CreateString(app_info.project_name);
                auto appversionOffset = b.CreateString(app_info.version);
                auto appdateOffset = b.CreateString(app_info.date);
                auto apptimeOffset = b.CreateString(app_info.time);
                auto piOffset = CreatePartitionInfo(b, labelOffset, (uint8_t)p->type, (uint8_t)p->subtype, p->size, (uint8_t)ota_state, p == running, appnameOffset, appversionOffset, appdateOffset, apptimeOffset);

                partitions_vector.push_back(piOffset);

                it = esp_partition_next(it);
            }
            auto piVecOffset = b.CreateVector(partitions_vector);

            esp_chip_info_t chip_info = {};
            esp_chip_info(&chip_info);
            struct timeval tv_now;
            gettimeofday(&tv_now, nullptr);

            float tsens_out{0.0};
            ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &tsens_out));

            ResponseSystemDataBuilder rsdb(b);
            rsdb.add_partitions(piVecOffset);
            rsdb.add_chip_cores(chip_info.cores);
            rsdb.add_chip_features(chip_info.features);
            rsdb.add_chip_model((uint32_t)chip_info.model);
            rsdb.add_chip_revision(chip_info.revision);
            rsdb.add_chip_temperature(tsens_out);
            rsdb.add_free_heap(esp_get_free_heap_size());
            rsdb.add_seconds_epoch(tv_now.tv_sec);
            rsdb.add_seconds_uptime(esp_timer_get_time() / 1000);

            uint8_t mac_buffer[6];
            esp_read_mac(mac_buffer, ESP_MAC_BT);
            auto bt = webmanager::Mac6(mac_buffer);
            esp_read_mac(mac_buffer, ESP_MAC_ETH);
            auto eth = webmanager::Mac6(mac_buffer);
            esp_read_mac(mac_buffer, ESP_MAC_IEEE802154);
            auto ieee = webmanager::Mac6(mac_buffer);
            esp_read_mac(mac_buffer, ESP_MAC_WIFI_SOFTAP);
            auto softap = webmanager::Mac6(mac_buffer);
            esp_read_mac(mac_buffer, ESP_MAC_WIFI_STA);
            auto sta = webmanager::Mac6(mac_buffer);
            rsdb.add_mac_address_bt(&bt);
            rsdb.add_mac_address_eth(&eth);
            rsdb.add_mac_address_ieee802154(&ieee);
            rsdb.add_mac_address_wifi_softap(&softap);
            rsdb.add_mac_address_wifi_sta(&sta);
            flatbuffers::Offset<ResponseSystemData> rsd = rsdb.Finish();
            auto mw= CreateMessageWrapper(b, Message::Message_ResponseSystemData, rsd.Union());
            b.Finish(mw);
            uint8_t* bp=b.GetBufferPointer();
            size_t l = b.GetSize();
            ws_pkt->payload=bp;
            ws_pkt->len=l;
            ESP_LOGI(TAG, "Created a package of size %d with first bytes %02X %02X %02X %02X and last bytes %02X %02X %02X %02X. Sending it back now", ws_pkt->len, bp[0], bp[1], bp[2], bp[3], bp[l-4], bp[l-3], bp[l-2], bp[l-1]);
            return httpd_ws_send_frame(req, ws_pkt);
        }

        static esp_err_t handle_ota_post_static(httpd_req_t *req)
        {
            M *myself = static_cast<M *>(req->user_ctx);
            return myself->handle_ota_post_(req);
        }

        esp_err_t handle_ota_post_(httpd_req_t *req)
        {
            char buf[1024];
            esp_ota_handle_t ota_handle;
            size_t remaining = req->content_len;

            const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
            ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

            while (remaining > 0)
            {
                int recv_len = httpd_req_recv(req, buf, std::min(remaining, sizeof(buf)));
                if (recv_len <= 0)
                {
                    // Serious Error: Abort OTA
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
                    return ESP_FAIL;
                }
                if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
                {
                    // Timeout Error: Just retry
                    continue;
                }
                if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK)
                {
                    // Successful Upload: Flash firmware chunk
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
                    return ESP_FAIL;
                }

                remaining -= recv_len;
            }

            // Validate and switch to new OTA image and reboot
            if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK)
            {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
                return ESP_FAIL;
            }

            httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_restart();

            return ESP_OK;
        }

    public:
        static M *GetSingleton()
        {
            if (!singleton)
            {
                singleton = new M();
            }
            return singleton;
        }

        static void time_sync_notification_cb(struct timeval *tv)
        {
            time_t now;
            char strftime_buf[64];
            struct tm timeinfo;

            time(&now);
            localtime_r(&now, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            ESP_LOGI(TAG, "Notification of a time synchronization. The current date/time in Berlin is: %s", strftime_buf);
            webmanager::M::GetSingleton()->Log(messagecodes::C::SNTP, esp_timer_get_time() / 1000);
        }

        void RegisterHTTPDHandlers(httpd_handle_t httpd_handle)
        {
            httpd_uri_t webmanager_get = {"/webmanager", HTTP_GET, handle_webmanager_get_static, this, false, false, nullptr};
            httpd_uri_t ota_post = {"/ota", HTTP_POST, handle_ota_post_static, this, false, false, nullptr};
            httpd_uri_t webmanager_ws = {"/webmanager_ws", HTTP_GET, handle_webmanager_ws_static, this, true, false, nullptr};
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &webmanager_get));
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &ota_post));
            ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &webmanager_ws));
        }

        esp_err_t Init(const char *accessPointSsid, const char *accessPointPassword, const char *hostnamePattern, uint8_t *http_buffer, size_t http_buffer_len, bool resetStoredWifiConnection, bool init_netif_and_create_event_loop = true)
        {
            if (strlen(accessPointPassword) < 8 && AP_AUTHMODE != WIFI_AUTH_OPEN)
            {
                ESP_LOGE(TAG, "Password too short for authentication. Minimal length is 8.");
                return ESP_FAIL;
            }

            if (webmanager_semaphore != nullptr)
            {
                ESP_LOGE(TAG, "webmanager already started");
                return ESP_FAIL;
            }
            webmanager_semaphore = xSemaphoreCreateBinary();
            xSemaphoreGive(webmanager_semaphore);

            this->http_buffer = http_buffer;
            this->http_buffer_len = http_buffer_len;

            if (init_netif_and_create_event_loop)
            {
                ESP_ERROR_CHECK(esp_netif_init());
                ESP_ERROR_CHECK(esp_event_loop_create_default());
            }

            ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler_static, this, nullptr));

            esp_reset_reason_t reason = esp_reset_reason();
            switch (reason)
            {
            case ESP_RST_POWERON:
                ESP_LOGI(TAG, "Reset Reason is RESET %d. MessageLog Memory is reset", reason);
                BUFFER.fill({0, 0, 0, 0});
                break;
            case ESP_RST_EXT:
            case ESP_RST_SW:
                ESP_LOGI(TAG, "Reset Reason is EXT/SW %d. MessageLogEntry is added", reason);
                this->Log(messagecodes::C::SW, 0);
                break;
            case ESP_RST_PANIC:
                ESP_LOGI(TAG, "Reset Reason is PANIC %d. MessageLogEntry is added", reason);
                this->Log(messagecodes::C::PANIC, 0);
                break;
            case ESP_RST_BROWNOUT:
                ESP_LOGI(TAG, "Reset Reason is BROWNOUT %d. MessageLogEntry is added", reason);
                this->Log(messagecodes::C::BROWNOUT, 0);
                break;
            case ESP_RST_TASK_WDT:
            case ESP_RST_INT_WDT:
                ESP_LOGI(TAG, "Reset Reason is WATCHDOG %d. MessageLogEntry is added", reason);
                this->Log(messagecodes::C::WATCHDOG, 0);
                break;
            default:
                ESP_LOGI(TAG, "Reset Reason is %d. MessageLog Memory is reset", reason);
                BUFFER.fill({0, 0, 0, 0});
                break;
            }

            esp_log_set_vprintf(M::logging_vprintf);

            wifi_manager_retry_timer = xTimerCreate("retry timer", pdMS_TO_TICKS(WIFI_MANAGER_RETRY_TIMER), pdFALSE, (void *)0, M::webmanager_timer_retry_cb_static);
            wifi_manager_shutdown_ap_timer = xTimerCreate("shutdown_ap_timer", pdMS_TO_TICKS(WIFI_MANAGER_SHUTDOWN_AP_TIMER), pdFALSE, (void *)0, M::webmanager_timer_shutdown_ap_cb_static);

            // Create and check netifs
            wifi_netif_sta = esp_netif_create_default_wifi_sta();
            wifi_netif_ap = esp_netif_create_default_wifi_ap();
            assert(wifi_netif_sta);
            assert(wifi_netif_ap);

            // attach event handler for wifi
            ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_static, this, nullptr));

            // init WIFI base
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

            // Prepare WIFI_CONFIGs for ap mode and for sta mode and for scan
            wifi_config_sta.sta.scan_method = WIFI_FAST_SCAN;
            wifi_config_sta.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
            wifi_config_sta.sta.threshold.rssi = -127;
            wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config_sta.sta.pmf_cfg.capable = true;
            wifi_config_sta.sta.pmf_cfg.required = false;

            // make ssid unique --> append Mac Adress of wifi station to ssid
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
            wifi_config_ap.ap.ssid_len = sprintf((char *)wifi_config_ap.ap.ssid, "%s%02x%02x%02x", accessPointSsid, mac[3], mac[4], mac[5]);
            strcpy((char *)wifi_config_ap.ap.password, accessPointPassword);

            wifi_config_ap.ap.channel = CONFIG_NETWORK_WIFI_AP_CHANNEL;
            wifi_config_ap.ap.max_connection = CONFIG_NETWORK_AP_MAX_AP_CONN;
            wifi_config_ap.ap.authmode = AP_AUTHMODE;

            scan_config.ssid = 0;
            scan_config.bssid = 0;
            scan_config.channel = 0;
            scan_config.show_hidden = true;

            char hostname[32] = {0};
            sprintf(hostname, hostnamePattern, mac[3], mac[4], mac[5]);
            ESP_ERROR_CHECK(esp_netif_set_hostname(wifi_netif_sta, hostname));
            ESP_ERROR_CHECK(esp_netif_set_hostname(wifi_netif_ap, hostname));

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

            if (resetStoredWifiConnection)
            {
                ESP_LOGI(TAG, "Forced to delete saved wifi configuration. Starting access point and do an initial scan.");
                delete_sta_config();
                ESP_ERROR_CHECK(esp_wifi_start());
                esp_wifi_scan_start(&scan_config, false);
                scanIsActive = true;
            }
            else if (read_sta_config() != ESP_OK)
            {
                ESP_LOGI(TAG, "No saved wifi configuration found on startup. Starting access point and do an initial scan.");
                ESP_ERROR_CHECK(esp_wifi_start());
                esp_wifi_scan_start(&scan_config, false);
                scanIsActive = true;
            }
            else
            {
                ESP_LOGI(TAG, "Saved wifi found on startup. Will attempt to do an initial scan and then connect to ssid %s with password %s.", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
                ESP_ERROR_CHECK(esp_wifi_start());
                esp_wifi_scan_start(&scan_config, true);
                remainingAttempsToConnectAsSTA = ATTEMPTS_TO_RECONNECT;
                ESP_ERROR_CHECK(esp_wifi_connect());
                staState = WifiStationState::ABOUT_TO_CONNECT;
            }

            
            temperature_sensor_config_t temp_sensor = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
            ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));
            ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));

            return ESP_OK;
        }

        esp_err_t CallMeAfterInitializationToMarkCurrentPartitionAsValid()
        {
            /* Mark current app as valid */
            const esp_partition_t *partition = esp_ota_get_running_partition();
            esp_ota_img_states_t ota_state;
            if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK)
            {
                if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
                {
                    esp_ota_mark_app_valid_cancel_rollback();
                }
            }
            return ESP_OK;
        }

        void Log(messagecodes::C messageCode, uint32_t messageData)
        {
            bool entryFound{false};
            struct timeval tv_now;
            gettimeofday(&tv_now, nullptr);
            xSemaphoreTake(webmanager_semaphore, portMAX_DELAY);
            for (int i = 0; i < BUFFER.size(); i++)
            {
                if (BUFFER[i].messageCode == 0)
                {
                    ESP_LOGD(TAG, "Found an empty logging slot on pos %d for messageCode %lu", i, (uint32_t)messageCode);
                    BUFFER[i].messageCode = (uint32_t)messageCode;
                    BUFFER[i].lastMessageData = messageData;
                    BUFFER[i].lastMessageTimestamp = tv_now.tv_sec;
                    BUFFER[i].messageCount = 1;
                    entryFound = true;
                    break;
                }
                else if (BUFFER[i].messageCode == (uint32_t)messageCode)
                {
                    ESP_LOGD(TAG, "Found an updateable logging slot on pos %d for messageCode %lu", i, (uint32_t)messageCode);
                    BUFFER[i].lastMessageData = messageData;
                    BUFFER[i].lastMessageTimestamp = tv_now.tv_sec;
                    BUFFER[i].messageCount++;
                    entryFound = true;
                    break;
                }
            }
            if (!entryFound)
            {
                // search oldest and overwrite
                time_t oldestTimestamp = BUFFER[0].lastMessageTimestamp;
                size_t oldestIndex{0};
                for (size_t i = 1; i < BUFFER.size(); i++)
                {
                    if (BUFFER[i].lastMessageTimestamp < oldestTimestamp)
                    {
                        oldestTimestamp = BUFFER[i].lastMessageTimestamp;
                        oldestIndex = i;
                    }
                }
                ESP_LOGD(TAG, "Found the oldest logging slot on pos %d for messageCode %lu", oldestIndex, (uint32_t)messageCode);
                BUFFER[oldestIndex].messageCode = (uint32_t)messageCode;
                BUFFER[oldestIndex].lastMessageData = messageData;
                BUFFER[oldestIndex].lastMessageTimestamp = tv_now.tv_sec;
                BUFFER[oldestIndex].messageCount = 1;
            }
            xSemaphoreGive(webmanager_semaphore);
            return;
        }
    };

}
#undef TAG
