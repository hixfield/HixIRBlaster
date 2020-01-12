#include "HixConfig.h"
#include "HixMQTT.h"
#include "HixWebServer.h"
#include "secret.h"
#include <ArduinoOTA.h>
#include <FS.h>
#include <HixPinDigitalInput.h>
#include <HixPinDigitalOutput.h>
#include <HixTimeout.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRtext.h>
#include <IRutils.h>
// runtime global variables
HixConfig           g_config;
HixWebServer        g_webServer(g_config);
IRrecv              g_irIn(D5);
HixPinDigitalOutput g_irOut(D4);


HixMQTT g_mqtt(Secret::WIFI_SSID,
               Secret::WIFI_PWD,
               g_config.getMQTTServer(),
               g_config.getDeviceType(),
               g_config.getDeviceVersion(),
               g_config.getRoom(),
               g_config.getDeviceTag());


//////////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////////

void resetWithMessage(const char * szMessage) {
    Serial.println(szMessage);
    delay(2000);
    ESP.reset();
}

void configureOTA() {
    Serial.println("Configuring OTA, my hostname:");
    Serial.println(g_mqtt.getMqttClientName());
    ArduinoOTA.setHostname(g_mqtt.getMqttClientName());
    ArduinoOTA.setPort(8266);
    //setup handlers
    ArduinoOTA.onStart([]() {
        Serial.println("OTA -> Start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("OTA -> End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA -> Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA -> Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("OTA -> Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("OTA -> Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("OTA -> Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("OTA -> Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("OTA -> End Failed");
    });
    ArduinoOTA.begin();
}

//////////////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////////////

void setup() {
    Serial.begin(115200);
    Serial.print(F("Startup "));
    Serial.print(g_config.getDeviceType());
    Serial.print(F(" "));
    Serial.println(g_config.getDeviceVersion());
    //disconnect WiFi -> seams to help for bug that after upload wifi does not want to connect again...
    Serial.println(F("Disconnecting WIFI"));
    WiFi.disconnect();
    //setup pins
    Serial.println(F("Setting up IR receiver"));
    g_irIn.enableIRIn();
    Serial.println(F("Setting up IR transmitter"));
    g_irOut.begin();
    // configure MQTT
    Serial.println(F("Setting up MQTT"));
    if (!g_mqtt.begin()) resetWithMessage("MQTT allocation failed, resetting");
    //setup SPIFFS
    Serial.println(F("Setting up SPIFFS"));
    if (!SPIFFS.begin()) resetWithMessage("SPIFFS initialization failed, resetting");
    //setup the server
    Serial.println(F("Setting up web server"));
    g_webServer.begin();
    // all done
    Serial.println(F("Setup complete"));
}

//////////////////////////////////////////////////////////////////////////////////
// Loop
//////////////////////////////////////////////////////////////////////////////////

void loop() {
    //other loop functions
    g_mqtt.loop();
    g_webServer.handleClient();
    ArduinoOTA.handle();
    //toggle output to test...
    //g_irOut.toggle();
    //delay(25);
    decode_results results;
    if (g_irIn.decode(&results)) {
        unsigned long val = results.value;
        Serial.println(val, HEX);
        g_irIn.resume();
    }
    delay(100);
}

//////////////////////////////////////////////////////////////////////////////////
// Required by the MQTT library
//////////////////////////////////////////////////////////////////////////////////

void onConnectionEstablished() {
    //setup OTA
    if (g_config.getOTAEnabled()) {
        configureOTA();
    } else {
        Serial.println("OTA is disabled");
    }
    //plushing values
    g_mqtt.publishDeviceValues();
}
