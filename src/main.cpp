#include "HixConfig.h"
#include "HixMQTT.h"
#include "HixWebServer.h"
#include "secret.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRtext.h>
#include <IRutils.h>
#include <ir_Samsung.h>

// runtime global variables
HixConfig    g_config;
HixWebServer g_webServer(g_config);
HixMQTT      g_mqtt(g_config,
               Secret::WIFI_SSID,
               Secret::WIFI_PWD,
               g_config.getMQTTServer(),
               g_config.getDeviceType(),
               g_config.getDeviceVersion(),
               g_config.getRoom(),
               g_config.getDeviceTag());
//hardware related
IRSamsungAc g_IRTransmitter(D3);
IRrecv      g_IRReciever(D5, 1024, 40, true);
//software global vars
bool g_bACIsOn = false;

//////////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////////

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

void resetWithMessage(const char * szMessage) {
    Serial.println(szMessage);
    delay(2000);
    ESP.reset();
}

void printACState() {
    Serial.println("Samsung A/C remote is in the following state:");
    Serial.printf("  %s\n", g_IRTransmitter.toString().c_str());
}

void AC_On(int nTemperature) {
    Serial.print("AC ON to ");
    Serial.print(nTemperature);
    Serial.println(" C");
    //to avoid detecting our own transmitted signal, switch off detector first
    g_IRReciever.disableIRIn();
    //transmit our commands
    g_IRTransmitter.on();
    g_IRTransmitter.setFan(kSamsungAcFanAuto);
    g_IRTransmitter.setMode(kSamsungAcAuto);
    g_IRTransmitter.setTemp(nTemperature);
    g_IRTransmitter.setSwing(true);
    g_IRTransmitter.send();
    //re-enable our IR detector
    g_IRReciever.enableIRIn();
    //keep our state
    g_bACIsOn = true;
}


void AC_Off(void) {
    Serial.print("AC OFF");
    //to avoid detecting our own transmitted signal, switch off detector first
    g_IRReciever.disableIRIn();
    //transmit our commands
    g_IRTransmitter.off();
    g_IRTransmitter.send();
    //re-enable our IR detector
    g_IRReciever.enableIRIn();
    //keep our state
    g_bACIsOn = false;
}

void AC_toggle(int nTemperature) {
    if (g_bACIsOn)
        AC_Off();
    else
        AC_On(nTemperature);
}

bool handleIRCommand(decode_results results) {
    switch (results.value) {
        //Telenet IR : play button
    case 0x48C6EAFF:
        AC_On(28);
        return true;
        //Telenet IR : pause button
    case 0xAA33049:
        AC_Off();
        return true;
    default:
        Serial.print("Unrecognized code received ");
        Serial.println(resultToHexidecimal(&results));
    }
    //nothing handled
    return false;
}

bool checkIR(void) {
    decode_results results;
    // Check if the IR code has been received.
    if (g_IRReciever.decode(&results)) {
        // Check if we got an IR message that was to big for our capture buffer.
        if (results.overflow) Serial.println("Error IR capture buffer overflow");
        // Display the basic output of what we found.
        Serial.print("IR Received: ");
        Serial.println(resultToHexidecimal(&results));
        //did find something!
        return handleIRCommand(results);
    }
    //return not found
    return false;
}


//////////////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////////////

void setup() {
    //print startup config
    Serial.begin(115200);
    Serial.print(F("Startup "));
    Serial.print(g_config.getDeviceType());
    Serial.print(F(" "));
    Serial.println(g_config.getDeviceVersion());
    //disconnect WiFi -> seams to help for bug that after upload wifi does not want to connect again...
    Serial.println(F("Disconnecting WIFI"));
    WiFi.disconnect();
    // configure MQTT
    Serial.println(F("Setting up MQTT"));
    if (!g_mqtt.begin()) resetWithMessage("MQTT allocation failed, resetting");
    //setup SPIFFS
    Serial.println(F("Setting up SPIFFS"));
    if (!SPIFFS.begin()) resetWithMessage("SPIFFS initialization failed, resetting");
    //setup the server
    Serial.println(F("Setting up web server"));
    g_webServer.begin(); //configure ir transmitter
    Serial.println("Setting up IR Transmitter");
    g_IRTransmitter.begin();
    //configure receiver
    Serial.println("Setting up IR Receiver");
    g_IRReciever.setUnknownThreshold(12);
    g_IRReciever.enableIRIn();
    //all done
    Serial.println("Setup complete");
    //set to known state
    AC_Off();
}

//////////////////////////////////////////////////////////////////////////////////
// Loop
//////////////////////////////////////////////////////////////////////////////////


void loop() {
    //other loop functions
    g_mqtt.loop();
    g_webServer.handleClient();
    ArduinoOTA.handle();
    //my own processing
    checkIR();
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

    //publish values
    g_mqtt.publishDeviceValues();
    g_mqtt.publishStatusValues(false, NULL);

    //register for ac temperature
    g_mqtt.subscribe(g_mqtt.topicForPath("subscribe/ac_temperature"), [](const String & payload) {
        g_config.setACTemperature(payload.toInt());
        g_config.commitToEEPROM();
        g_mqtt.publishDeviceValues();
        if (g_bACIsOn) {
            AC_On(g_config.getACTemperature());
        }
    });

    //ACC on
    g_mqtt.subscribe(g_mqtt.topicForPath("subscribe/ac_on"), [](const String & payload) {
        AC_On(g_config.getACTemperature());
        g_mqtt.publishStatusValues(g_bACIsOn, NULL);
    });

    //ACC off
    g_mqtt.subscribe(g_mqtt.topicForPath("subscribe/ac_off"), [](const String & payload) {
        AC_Off();
        g_mqtt.publishStatusValues(g_bACIsOn, NULL);
    });

    //ACC toggle
    g_mqtt.subscribe(g_mqtt.topicForPath("subscribe/ac_toggle"), [](const String & payload) {
        AC_toggle(g_config.getACTemperature());
        g_mqtt.publishStatusValues(g_bACIsOn, NULL);
    });

    //RAW IR send
    g_mqtt.subscribe(g_mqtt.topicForPath("subscribe/send_raw_ir"), [](const String & payload) {
        Serial.println("Not implemtned yet send_raw_ir");
    });
}