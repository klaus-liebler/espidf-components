#pragma once
namespace webmanager{
    esp_err_t Init(httpd_handle_t http_server, uint8_t *http_buffer, size_t http_buffer_len);
    esp_err_t CallMeAfterInitializationToMarkCurrentPartitionAsValid();
}
