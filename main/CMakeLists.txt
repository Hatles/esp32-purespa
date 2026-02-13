set(requires esp-tls nvs_flash esp_netif esp_http_server json driver esp_timer)
idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs protocol_examples_common)
else()
    list(APPEND requires esp_wifi esp_eth)
endif()

idf_component_register(SRCS "main.cpp" "wifi_manager.cpp" "dns_server.cpp" "captive_portal.cpp" "web_server.cpp" "purespa/PureSpaIO.cpp" "purespa/PureSpaService.cpp"
                    INCLUDE_DIRS "." "purespa"
                    PRIV_REQUIRES ${requires})

target_add_binary_data(${COMPONENT_LIB} "index.html" TEXT)
