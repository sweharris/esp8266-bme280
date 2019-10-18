# esp8266-bme280
Use an esp8266 with a bme280 connected to report on temp/press/humidity via
MQTT

You will need to make a file `network_conn.h` with contents similar to

    const char* ssid       = "your-ssid";
    const char* password   = "your-wpa-passwd";
    const char* mqttServer = "your-MQTT-server";
    const int mqttPort     = 1883;

in order to provide details of your network and MQTT server

The BME280 is connected via I2C connectivity.  So, typically should
be wired as

BME280   ESP8266
======   =======
   VIN---3.3V
   GND---GND
   SCL---D1 (GPIO5)
   SDA---D2 (GPIO6)

The D1/D2 ports on the ESP8266 are hardware managed I2C, so work faster

The MQTT channels are based off the word "bme280" and the last 6 digits of the MAC
   
     e.g  bme280/123456/temp      (in Celcius)
     e.g  bme280/123456/pressure  (in Pa)
     e.g  bme280/123456/humidity  (in %)
   
Uses PubSubClient and IRremoteESP8266 libraries on top of the ESP8266WiFi one
   
GPL2.0 or later.  See GPL LICENSE file for details.
Original code by Stephen Harris, Sep 2019
