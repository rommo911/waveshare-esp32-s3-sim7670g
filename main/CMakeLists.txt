idf_component_register(SRC_DIRS 
                                "./src/led"
                                "./src/imu6500"
                                "./src/modem"
                                "./src/rtc"
                                "./src/sdcard"
                                "./src/web_server"
                                "./src/imu6500/interface"
                                "./src/wifi"
                                "./src/power"
                                "./src"
                        INCLUDE_DIRS 
                                "./src/power"
                                "./src/led"
                                "./src/imu6500"
                                "./src/imu6500/interface"
                                "./src/modem"
                                "./src/rtc"
                                "./src/sdcard"
                                "./src/web_server"
                                "./src/wifi"
                                "./src"
                    PRIV_REQUIRES esp_wifi esp_http_client app_update nvs_flash esp_driver_gpio esp_netif SIM7670_gnss
                    REQUIRES arduino-esp32 mqtt  )
                    
                    
littlefs_create_partition_image(storage ../data FLASH_IN_PROJECT)