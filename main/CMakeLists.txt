idf_component_register(SRC_DIRS 
                                "./src/led"
                                "./src/imu6500"
                                "./src/rtc"
                                "./src/sdcard"
                                "./src/web_server"
                                "./src/imu6500/interface"
                                "./src/wifi"
                                "./src/power"
                                "./src/carBattery"
                                "./src/idf_modem"
                                "./src"
                        INCLUDE_DIRS 
                                "./src/power"
                                "./src/carBattery"
                                "./src/led"
                                "./src/imu6500"
                                "./src/imu6500/interface"
                                "./src/rtc"
                                "./src/sdcard"
                                "./src/web_server"
                                "./src/wifi"
                                "./src/idf_modem"
                                "./src"
                    PRIV_REQUIRES esp_wifi esp_http_client app_update nvs_flash esp_driver_gpio esp_netif SIM7670_gnss
                    REQUIRES arduino-esp32 mqtt mbedtls
                    EMBED_TXTFILES src/ssl/isrgrootx1.pem
                    )
                   
                    
#littlefs_create_partition_image(storage ../data FLASH_IN_PROJECT)