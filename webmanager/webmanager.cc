#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <hal/efuse_hal.h>
#include <common-esp32.hh>
#include <algorithm>
#define TAG "OTA"

namespace webmanager
{

    extern const char webmanager_html_gz_start[] asm("_binary_webmanager_html_gz_start");
    extern const size_t webmanager_html_gz_length asm("webmanager_html_gz_length");
    constexpr char UNIT_SEPARATOR{0x1F};
    constexpr char RECORD_SEPARATOR{0x1E};
    constexpr char GROUP_SEPARATOR{0x1D};
    constexpr char FILE_SEPARATOR{0x1C};
   
    uint8_t *http_buffer{nullptr};
    size_t http_buffer_len{0};

    esp_err_t handle_manager_get(httpd_req_t *req)
    {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, webmanager_html_gz_start, webmanager_html_gz_length);
        return ESP_OK;
    }

    esp_err_t handle_restart_get(httpd_req_t *req)
    {
        esp_restart();
        return ESP_OK;
    }

    esp_err_t handle_systeminfo_get(httpd_req_t *req)
    {
        char *buf = (char *)http_buffer;
        size_t pos{0};
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_partition_iterator_t it= esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
        while(it){
            const esp_partition_t * p = esp_partition_get(it);
            esp_ota_img_states_t ota_state;
            esp_ota_get_state_partition(p, &ota_state);
           
            esp_app_desc_t app_info={};
            esp_ota_get_partition_description(p, &app_info);

            ESP_LOGI(TAG, "Partition %s;%d;%d;%lu;%d;%d;%s;%s;%s;%s;", p->label, (uint8_t)p->type, (uint8_t)p->subtype, p->size,(uint8_t)ota_state, p==running, app_info.project_name, app_info.version, app_info.date, app_info.time);
            pos += snprintf(buf + pos, http_buffer_len - pos, "%s%c%d%c%d%c%lu%c%d%c%d%c%s%c%s%c%s%c%s%c", p->label, UNIT_SEPARATOR, (uint8_t)p->type, UNIT_SEPARATOR, (uint8_t)p->subtype, UNIT_SEPARATOR, p->size, UNIT_SEPARATOR, (uint8_t)ota_state, UNIT_SEPARATOR, p==running, UNIT_SEPARATOR, app_info.project_name, UNIT_SEPARATOR, app_info.version, UNIT_SEPARATOR, app_info.date, UNIT_SEPARATOR, app_info.time, RECORD_SEPARATOR);
            it= esp_partition_next(it);
        }
        pos += snprintf(buf + pos, http_buffer_len - pos, "%c", GROUP_SEPARATOR);
        struct timeval tv_now;
        gettimeofday(&tv_now, nullptr);
        time_t  epoch_seconds= tv_now.tv_sec;
        uint32_t uptime_seconds = esp_timer_get_time()/1000;
        uint32_t free_heap_size = esp_get_free_heap_size();

        
        pos += snprintf(buf + pos, http_buffer_len - pos, "%llu%c", epoch_seconds,  RECORD_SEPARATOR);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%lu%c",  uptime_seconds, RECORD_SEPARATOR);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%lu%c",  free_heap_size, RECORD_SEPARATOR);

        uint8_t mac_address_buffer[6];
        esp_read_mac(mac_address_buffer, ESP_MAC_WIFI_STA);
        pos += snprintf(buf + pos, http_buffer_len - pos, MACSTR"%c",  MAC2STR(mac_address_buffer),  RECORD_SEPARATOR);
        esp_read_mac(mac_address_buffer, ESP_MAC_WIFI_SOFTAP);
        pos += snprintf(buf + pos, http_buffer_len - pos, MACSTR"%c",  MAC2STR(mac_address_buffer),  RECORD_SEPARATOR);
        esp_read_mac(mac_address_buffer, ESP_MAC_BT);
        pos += snprintf(buf + pos, http_buffer_len - pos, MACSTR"%c",  MAC2STR(mac_address_buffer),  RECORD_SEPARATOR);
        esp_read_mac(mac_address_buffer, ESP_MAC_ETH);
        pos += snprintf(buf + pos, http_buffer_len - pos, MACSTR"%c",  MAC2STR(mac_address_buffer),  RECORD_SEPARATOR);
        esp_read_mac(mac_address_buffer, ESP_MAC_IEEE802154);
        pos += snprintf(buf + pos, http_buffer_len - pos, MACSTR"%c",  MAC2STR(mac_address_buffer),  RECORD_SEPARATOR);

        esp_chip_info_t chip_info={};
        esp_chip_info(&chip_info);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%d%c",  chip_info.model,  RECORD_SEPARATOR);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%lu%c", chip_info.features,  RECORD_SEPARATOR);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%d%c",  chip_info.revision,  RECORD_SEPARATOR);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%d%c",  chip_info.cores,  RECORD_SEPARATOR);
        pos += snprintf(buf + pos, http_buffer_len - pos, "%c", GROUP_SEPARATOR);
        
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, (char *)buf, pos);
        return ESP_OK;
    }

    esp_err_t handle_ota_post(httpd_req_t *req)
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

    constexpr httpd_uri_t manager_get = {"/manager", HTTP_GET, handle_manager_get, nullptr};
    constexpr httpd_uri_t ota_post = {"/ota", HTTP_POST, handle_ota_post, nullptr};
    constexpr httpd_uri_t systeminfo_get = {"/systeminfo", HTTP_GET, handle_systeminfo_get, nullptr};
    constexpr httpd_uri_t restart_get = {"/restart", HTTP_GET, handle_restart_get, nullptr};


    esp_err_t Init(httpd_handle_t http_server, uint8_t *http_buffer_, size_t http_buffer_len_)
    {
        http_buffer=http_buffer_;
        http_buffer_len=http_buffer_len_;
        RETURN_ON_ERROR(httpd_register_uri_handler(http_server, &manager_get));
        RETURN_ON_ERROR(httpd_register_uri_handler(http_server, &ota_post));
        RETURN_ON_ERROR(httpd_register_uri_handler(http_server, &systeminfo_get));
        RETURN_ON_ERROR(httpd_register_uri_handler(http_server, &restart_get));
        return ESP_OK;
    }

    esp_err_t CallMeAfterInitializationToMarkCurrentPartitionAsValid(){
        /* Mark current app as valid */
	    const esp_partition_t *partition = esp_ota_get_running_partition();
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK) {
            if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
                esp_ota_mark_app_valid_cancel_rollback();
            }
        }
        return ESP_OK;
    }

};
