#include <Arduino.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRtext.h>
#include <IRutils.h>
#include <ir_Samsung.h>

IRSamsungAc g_IRTransmitter(D3);
IRrecv      g_IRReciever(D5, 1024, 40, true);

decode_results results;

void printACState() {
    Serial.println("Samsung A/C remote is in the following state:");
    Serial.printf("  %s\n", g_IRTransmitter.toString().c_str());
}



void setup() {
    Serial.begin(115200);
    //configure ir transmitter
    Serial.println("Setting up IR Transmitter");
    g_IRTransmitter.begin();
    //configure receiver
    Serial.println("Setting up IR Receiver");
    g_IRReciever.setUnknownThreshold(12);
    g_IRReciever.enableIRIn();
    //all done
    Serial.println("Setup complete");
}

void setWarming(int nTemperature) {
    g_IRTransmitter.on();
    g_IRTransmitter.setFan(kSamsungAcFanAuto);
    g_IRTransmitter.setMode(kSamsungAcCool);
    g_IRTransmitter.setTemp(nTemperature);
    g_IRTransmitter.setSwing(true);
    g_IRTransmitter.send();
}

void setCooling(void) {
}

void setAuto(void) {
}

void turnOff(void) {
    g_IRTransmitter.off();
    g_IRTransmitter.send();
}

bool checkIR(void) {
    // Check if the IR code has been received.
    if (g_IRReciever.decode(&results)) {
        // Check if we got an IR message that was to big for our capture buffer.
        if (results.overflow) Serial.println("Error IR capture buffer overflow");
        // Display the basic output of what we found.
        Serial.print("IR Received: ");
        Serial.println(resultToHexidecimal(&results));
        //did find something!
        return true;
    }
    //return not found
    return false;
}

void loop() {
    if (checkIR()) {
        switch (results.value) {
        case 0x4DE74847:
            Serial.println("AC Heating");
            g_IRReciever.disableIRIn();
            setWarming(28);
            g_IRReciever.enableIRIn();
            break;
        case 0xB8781EF:
            Serial.println("AC Off");
            g_IRReciever.disableIRIn();
            turnOff();
            g_IRReciever.enableIRIn();
            break;
        }
    }
    /*
    setWarming(28);
    delay(5000);
    turnOff();
    delay(5000);
    */
}
