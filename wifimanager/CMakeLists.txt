idf_component_register(SRCS "dns_server.c"
                    INCLUDE_DIRS "include"
                    EMBED_FILES ../wifimanager_webui/dist_compressed/wifimanager.html.gz
                    REQUIRES esp_hw_support driver)
