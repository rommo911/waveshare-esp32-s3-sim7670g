/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * PPPoS Client Example
 */

#include "mqtt_client.hpp"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_log.h"
#include "Preferences.h"
#include "nvs_flash.h"

/**
 * Reference to the MQTT event base
 */
ESP_EVENT_DECLARE_BASE(MQTT_EVENTS);

String esp_mqtt_server = "mqtts://ramipi92.duckdns.org";
uint32_t esp_mqtt_port = 8433;
String esp_mqtt_user = "rami";
String esp_mqtt_pass = "Rr0033141500!";
bool check_nvs_storage()
{
    bool ret = true;
    Preferences pref;
    ret = pref.begin("mqtt", true);
    if (ret)
    {
        esp_mqtt_server = pref.getString("mqtt_server", esp_mqtt_server);
       esp_mqtt_port = pref.getUInt("mqtt_port", esp_mqtt_port);
        esp_mqtt_user = pref.getString("mqtt_user", esp_mqtt_user);
        esp_mqtt_pass = pref.getString("mqtt_pass", esp_mqtt_pass);
    }
    return ret;
}
/**
 * Thin wrapper around C mqtt_client
 */
struct MqttClientHandle
{
    MqttClientHandle()
    {
        check_nvs_storage();
        esp_mqtt_client_config_t config = {};
        config.broker.address.uri = esp_mqtt_server.c_str(),
        config.broker.address.port = esp_mqtt_port,
        config.credentials.username = esp_mqtt_user.c_str(),
        config.credentials.authentication.password = esp_mqtt_pass.c_str(),
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
    esp_mqtt_client_register_event(h->client, MQTT_EVENT_ANY, MqttClient::handle_event, this);
}

void MqttClient::connect()
{
    if (h->client != nullptr)
        esp_mqtt_client_start(h->client);
}


bool MqttClient::isConnected()
{
    return this->connected;
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

MqttClient::~MqttClient()
{
    if (h->client != nullptr)
    {
        esp_mqtt_client_stop(h->client);
        esp_mqtt_client_disconnect(h->client);
        esp_mqtt_client_unregister_event(h->client, MQTT_EVENT_ANY, MqttClient::handle_event);
        esp_mqtt_client_destroy(h->client);
    }
}

void MqttClient::register_handler(esp_mqtt_event_id_t id, esp_event_handler_t event_handler)
{
    if (h->client == nullptr)
        return;
    esp_mqtt_client_register_event(h->client, id, event_handler, this);
}

void MqttClient::handle_event(void *arg, esp_event_base_t base, int32_t event, void *data)
{
    auto *mqtt_client = static_cast<MqttClient *>(arg);
    if (base != MQTT_EVENTS)
        return;
    if (event == MQTT_EVENT_CONNECTED)
    {
        mqtt_client->connected = true;
        return;
    }
    if (event == MQTT_EVENT_DISCONNECTED)
    {
        mqtt_client->connected = false;
        return;
    }
    if (event == MQTT_EVENT_SUBSCRIBED)
    {
        return;
    }
    if (event == MQTT_EVENT_UNSUBSCRIBED)
    {
        return;
    }

    if (event == MQTT_EVENT_DATA)
    {
        ESP_LOGI(TAG, " TOPIC: %s", mqtt_client->get_topic(data).c_str());
        ESP_LOGI(TAG, " DATA: %s", mqtt_client->get_data(data).c_str());
    }
}
