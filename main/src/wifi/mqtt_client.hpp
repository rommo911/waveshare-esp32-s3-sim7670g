/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * PPPoS Client Example
 */
#pragma once

#include <string>
#include <memory>
#include "mqtt_client.h"
#include "esp_event.h"

struct MqttClientHandle;

/**
 * @brief Simple MQTT client wrapper
 */
class MqttClient
{
public:
    MqttClient();
    ~MqttClient();
    esp_err_t begin();

    /**
     * @brief Start the mqtt-client
     */
    esp_err_t connect();

    void disconnect();

    bool isConnected();
    /**
     * @brief Publish to topic
     * @param topic Topic to publish
     * @param data Data to publish
     * @param qos QoS (0 by default)
     * @return message id
     */
    int publish(const std::string &topic, const std::string &data, int qos = 0);

    /**
     * @brief Subscribe to a topic
     * @param topic Topic to subscribe
     * @param qos QoS (0 by default)
     * @return message id
     */
    int subscribe(const std::string &topic, int qos = 0);

    /**
     * @brief Get topic from event data
     * @return String topic
     */
    std::string get_topic(void *);

    /**
     * @brief Get published data from event
     * @return String representation of the data
     */
    std::string get_data(void *);

    /**
     * @brief Register MQTT event
     * @param id Event id
     * @param event_handler Event handler
     * @param event_handler_arg Event handler parameters
     */
    void register_handler(esp_mqtt_event_id_t id, esp_event_handler_t event_handler);

protected:
    bool connected = false;
    static inline const char *TAG = {"MqttClient"};

private:
    std::unique_ptr<MqttClientHandle> h;
    static void handle_event(void *arg, esp_event_base_t base, int32_t event, void *data);
};
