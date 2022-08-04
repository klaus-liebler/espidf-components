idf_component_register(
    INCLUDE_DIRS "include"
    EMBED_FILES ../wifimanager_webui/dist_compressed/wifimanager.html.gz
    REQUIRES "common" "esp_http_server" "nvs_flash"
	)
