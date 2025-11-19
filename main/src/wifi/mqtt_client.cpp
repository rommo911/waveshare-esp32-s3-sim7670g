/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * PPPoS Client Example
 */

#include <memory>
#include "mqtt_client.h"
#include "mqtt_client.hpp"
#include "esp_log.h"
/**
 * Reference to the MQTT event base
 */
ESP_EVENT_DECLARE_BASE(MQTT_EVENTS);

/**
 * Thin wrapper around C mqtt_client
 */
struct MqttClientHandle
{
    MqttClientHandle()
    {
        esp_mqtt_client_config_t config = {};
        config.broker.address.uri = "mqtts://ramipi92.duckdns.org",
        config.broker.address.port = 8433,
        config.credentials.username = "rami",
        config.credentials.authentication.password = "Rr0033141500!",
        config.broker.verification.skip_cert_common_name_check = true,
        client = esp_mqtt_client_init(&config);
        if (!client)
        {
            ESP_LOGE("mqttcpp", "Failed to initialize MQTT client");
        }
    }

    ~MqttClientHandle()
    {
        esp_mqtt_client_destroy(client);
    }

    esp_mqtt_client_handle_t client;
};

/**
 * @brief Definitions of MqttClient methods
 */
MqttClient::MqttClient() : h(std::unique_ptr<MqttClientHandle>(new MqttClientHandle()))
{
}

void MqttClient::connect()
{
    if (h->client != nullptr)
        esp_mqtt_client_start(h->client);
}

int32_t MqttClient::get_event(MqttClient::Event ev)
{
    switch (ev)
    {
    case Event::CONNECT:
    {
        return MQTT_EVENT_CONNECTED;
    }
    case Event::DATA:
        return MQTT_EVENT_DATA;
    }
    return -1;
}

int MqttClient::publish(const std::string &topic, const std::string &data, int qos)
{
    if (h->client != nullptr)
        return esp_mqtt_client_publish(h->client, topic.c_str(), data.c_str(), 0, qos, 0);
    return -1;
}

int MqttClient::subscribe(const std::string &topic, int qos)
{
    if (h->client != nullptr)
        return esp_mqtt_client_subscribe(h->client, topic.c_str(), qos);
    return -1;
}

std::string MqttClient::get_topic(void *event_data)
{
    auto event = (esp_mqtt_event_handle_t)event_data;
    if (event == nullptr || event->client != h->client)
        return {};

    return std::string(event->topic, event->topic_len);
}

std::string MqttClient::get_data(void *event_data)
{
    auto event = (esp_mqtt_event_handle_t)event_data;
    if (event == nullptr || event->client != h->client)
        return {};

    return std::string(event->data, event->data_len);
}

void MqttClient::register_handler(int32_t event_id, esp_event_handler_t event_handler, void *arg)
{
    if (h->client != nullptr)
        ESP_ERROR_CHECK(esp_mqtt_client_register_event(h->client, MQTT_EVENT_ANY, event_handler, arg));
}

MqttClient::~MqttClient() 
{
    if (h->client != nullptr)
        esp_mqtt_client_destroy(h->client);
}
