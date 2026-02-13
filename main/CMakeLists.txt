set(requires esp-tls nvs_flash esp_netif esp_http_server)
idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs protocol_examples_common)
else()
    list(APPEND requires esp_wifi esp_eth)
endif()

idf_component_register(SRCS "main.c" "wifi_manager.c" "dns_server.c" "captive_portal.c" "web_server.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ${requires})

target_add_binary_data(${COMPONENT_LIB} "index.html" TEXT)
