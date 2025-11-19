idf_component_register(SRCS "pppos_client.cpp" "SIM7670_gnss.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_modem esp_driver_gpio)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
