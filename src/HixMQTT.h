#ifndef HixMQTT_h
#define HixMQTT_h

#include "HixConfig.h"
#include <HixMQTTBase.h>

class HixMQTT : public HixMQTTBase {
  private:
    //link to the configuration
    HixConfig & m_config;
    //puts all values in one single json compatible with influxDB
    String influxDBJson(bool   bACEnabled,
                        char * szReceivedIr);

  public:
    HixMQTT(HixConfig &  config,
            const char * szWifi_SSID,
            const char * szWiFi_Password,
            const char * szMQTT_Server,
            const char * szDeviceType,
            const char * szDeviceVersion,
            const char * szRoom,
            const char * szDeviceTag);
    //publishes all the values for the sensor
    bool publishStatusValues(bool   bACEnabled,
                             char * szReceivedIR);
    bool publishDeviceValues(void);
};

#endif
