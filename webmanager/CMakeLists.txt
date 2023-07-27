idf_component_register(
                    SRCS "webmanager.cc"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "webui_builder/dist_compressed/webmanager.html.gz"
                    REQUIRES main esp_http_server app_update common esp_partition esp_timer)
