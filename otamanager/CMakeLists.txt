idf_component_register(
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "sciebo.cer" "netcase.hs-osnabrueck.de.crt"
    REQUIRES nvs_flash esp_https_ota esp_http_client app_update esp_timer driver common
    )
