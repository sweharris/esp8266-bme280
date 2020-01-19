// This code uses a BME280 in I2C mode to read the current values and
// publish them to a MQTT server.  It's pretty simple.
//
// The MQTT channels are based off the word "bme280" and the last 6 digits
// of the MAC, followed by "temp", "pressure", "humidity"
//
// eg bme280/123456/temp (in celcius)
//    bme280/123456/pressure (in hPa)
//    bme280/123456/humidity (in %)
//
// Uses PubSubClient library on top of the ESP8266WiFi one
//
// On my NodeMCU
//    BME280   ESP8266
//    ======   =======
//       VIN---3.3V or 5V
//       GND---GND
//       SCL---D4 (GPIO2)
//       SDA---D3 (GPIO0)
//
// GPL2.0 or later.  See GPL LICENSE file for details.
// Original code by Stephen Harris, Sep 2019

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// This include file needs to define the following
//   const char* ssid       = "your-ssid";
//   const char* password   = "your-password";
//   const char* mqttServer = "your-mqtt-server";
//   const int   mqttPort   = 1883;
// I did it this way so I can ignore the file from git

#include "network_conn.h"

#define _mqttBase    "bme280"

// How many seconds between sampling
#define sample_time 30

char mqttTempC[30];     // Enough for Base + "/" + 6 hex digits + "/" + channel
char mqttTempF[30];
char mqttPressure[30];
char mqttPressureHg[30];
char mqttHumidity[30];
char mqttDebug[30];

WiFiClient espClient;
PubSubClient client(espClient);

Adafruit_BME280 bme; // I2C

void log_msg(String msg)
{
  time_t now = time(nullptr);
  String tm = ctime(&now);
  tm.trim();
  tm = tm + ": " + msg;
  Serial.println(tm);
  client.publish(mqttDebug,tm.c_str());
}

void setup() {
  Serial.begin(115200);

  unsigned status;
    
  Wire.begin(D3,D4);
  status = bme.begin();  

  // This test taken almost verbatin from the bme280 example code
  if (!status)
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1);
  }
    
  // Set up the BME280 to reduce oversampling to "4" (it defaults to 16)
  // and apply a small smoothing curve.
  // We go into forced mode.
  // This should reduce power draw, so reduce thermal induced
  // error, and make numbers more stable

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X4,  // temperature
                  Adafruit_BME280::SAMPLING_X4, // pressure
                  Adafruit_BME280::SAMPLING_X4,  // humidity
                  Adafruit_BME280::FILTER_X16,
                  Adafruit_BME280::STANDBY_MS_1000 );

  // Let's create the channel names based on the MAC address
  unsigned char mac[6];
  char macstr[7];
  WiFi.macAddress(mac);
  sprintf(macstr, "%02X%02X%02X", mac[3], mac[4], mac[5]);

  sprintf(mqttTempC,      "%s/%s/tempc",      _mqttBase, macstr);
  sprintf(mqttTempF,      "%s/%s/tempf",      _mqttBase, macstr);
  sprintf(mqttPressure,   "%s/%s/pressure",   _mqttBase, macstr);
  sprintf(mqttPressureHg, "%s/%s/pressurehg", _mqttBase, macstr);
  sprintf(mqttHumidity,   "%s/%s/humidity",   _mqttBase, macstr);
  sprintf(mqttDebug   ,   "%s/%s/debug",      _mqttBase, macstr);

  delay(500);

  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  WiFi.mode(WIFI_STA);  // Ensure we're in station mode and never start AP
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(": ");

  // Wait for the Wi-Fi to connect
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }

  Serial.println();
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  // Get the current time.  Initial log lines may be wrong 'cos
  // it's not instant.  Timezone handling... eh, this is just for
  // debuging, so I don't care.  GMT is always good :-)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  delay(1000);

 // Now we're on the network, setup the MQTT client
  client.setServer(mqttServer, mqttPort);

#ifdef NETWORK_UPDATE
    __setup_updater();
#endif
}


int sample_loop=0; // this will cause no data to be sent for "sample_time"
                   // which will give the sensor time to settle after a reset

void loop() { 

 // Try to reconnect to MQTT each time around the loop, in case we disconnect
  while (!client.connected())
  {
    log_msg("Connecting to MQTT Server " + String(mqttServer));

    // Generate a random ID each time
    String clientId = "ESP8266Client-bme280-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      log_msg("MQTT connected.  I am " + WiFi.localIP().toString());
    } else {
      log_msg("failed with state " + client.state());
      delay(2000);
    }
  }

  // Keep MQTT alive
  client.loop();

  if (sample_loop++ >= sample_time)
  {
    read_and_send_data();
    sample_loop=0;
  }

#ifdef NETWORK_UPDATE
  __netupdateServer.handleClient();
#endif

  delay(1000);
}

float do_round(float f)
{
  return 0.1*int(f*10.0);
}

float do_round3(float f)
{
  return 0.001*int(f*1000.0);
}

char buf[12];
char *ftoa(float f,int dp)
{
  dtostrf(f,10,dp,buf);
  return buf;
}

void do_publish(char *channel, float newval, int dp)
{
  // Sanity check to ensure values aren't toooo out of range
  // tempC and tempF may go negative, but not _too_ negative.
  // pressure is over 1000, but never 2000
  if (newval > -100 && newval < 2000)
  {
    client.publish(channel, ftoa(newval,dp), true);
  }
  else
  {
    log_msg("Skipping bad value for " + String(channel) + " " + String(ftoa(newval,dp)));
  }
}

void read_and_send_data()
{
  // Force the BME to take a reading
  bme.takeForcedMeasurement();

  // Get current values to 0.1
  float c=bme.readTemperature();
  float f=do_round(c*1.8+32.0);
  c=do_round(c);

  float p=bme.readPressure()/100.0;
  float phg=do_round3(p*0.029529983071445);
  p=do_round(p);

  float h=do_round(bme.readHumidity());

  do_publish(mqttTempC, c, 1);
  do_publish(mqttTempF, f, 1);
  do_publish(mqttPressure, p, 1);
  do_publish(mqttPressureHg, phg, 3);
  do_publish(mqttHumidity, h, 1);
}
