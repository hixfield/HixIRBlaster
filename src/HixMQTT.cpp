#include "HixMQTT.h"
#include <Arduino.h>
#include <ArduinoJson.h>

HixMQTT::HixMQTT(HixConfig &  config,
                 const char * szWifi_SSID,
                 const char * szWiFi_Password,
                 const char * szMQTT_Server,
                 const char * szDeviceType,
                 const char * szDeviceVersion,
                 const char * szRoom,
                 const char * szDeviceTag) : HixMQTTBase(szWifi_SSID, szWiFi_Password, szMQTT_Server, szDeviceType, szDeviceVersion, szRoom, szDeviceTag),
                                             m_config(config) {
}

bool HixMQTT::publishDeviceValues(void) {
    if (!HixMQTTBase::publishDeviceValues()) {
        return false;
    }
    //my custom implementation
    if (isConnected()) {
        publish(topicForPath("device/ac_temperature"), m_config.getACTemperature(), true);
        //return non error
        return true;
    }
    //not connected, return error
    return false;
}

bool HixMQTT::publishStatusValues(bool   bACEnabled,
                                  char * szReceivedIR) {
    //call base implementation
    if (!HixMQTTBase::publishStatusValues()) {
        return false;
    }
    //my custom implementation
    if (isConnected()) {
        //dynamic values are not published with a retain = default value = false
        publish(topicForPath("status/ac_enabled"), bACEnabled);
        if (szReceivedIR) {
            publish(topicForPath("status/ir_received"), szReceivedIR);
        }
        //publish to influxdb topic
        publish(topicForPath("influxdb"), influxDBJson(bACEnabled, szReceivedIR));
        //return non error
        return true;
    }
    //not connected, return error
    return false;
}

String HixMQTT::influxDBJson(bool   bACEnabled,
                             char * szReceivedIr) {
    DynamicJsonDocument doc(500);

    //create the measurements => fields
    JsonObject doc_0     = doc.createNestedObject();
    doc_0["ac_enabled"]  = bACEnabled;
    doc_0["ir_received"] = szReceivedIr;

    //the device props => tags
    JsonObject doc_1        = doc.createNestedObject();
    doc_1["device_type"]    = m_deviceType;
    doc_1["device_version"] = m_deviceVersion;
    doc_1["device_tag"]     = m_deviceTag;
    doc_1["room"]           = m_room;
    doc_1["wifi_ssid"]      = WiFi.SSID();
    doc_1["ac_temperature"] = m_config.getACTemperature();

    //to string
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}
