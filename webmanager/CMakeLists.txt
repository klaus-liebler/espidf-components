idf_component_register(
                    SRCS "webmanager.cc"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "dist_webui/compressed/app.html.gz"
                    REQUIRES main flatbuffers-23.5.26 esp_http_server app_update common esp_partition esp_timer)
