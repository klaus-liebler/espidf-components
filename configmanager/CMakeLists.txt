idf_component_register(
    INCLUDE_DIRS "include"
    EMBED_FILES ../configmanager_webui/dist_compressed/configmanager.html.gz
    REQUIRES esp_http_server nvs_flash driver
    )