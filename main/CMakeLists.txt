idf_component_register(SRC_DIRS 
                                "./src/led"
                                "./src/imu6500"
                                "./src/modem"
                                "./src/rtc"
                                "./src/sdcard"
                                "./src/ssl"
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
                                "./src/ssl"
                                "./src/web_server"
                                "./src/wifi"
                                "./src"
                    PRIV_REQUIRES esp_wifi nvs_flash
                    
                    REQUIRES arduino-esp32 )
