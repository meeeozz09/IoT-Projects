#define BLYNK_TEMPLATE_ID           "xxxxxx" ///insert your template ID here
#define BLYNK_TEMPLATE_NAME         "xxxxxx" ///insert your template name here
#define BLYNK_AUTH_TOKEN            "xxxxxx" ///insert your auth token here

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

char ssid[] = "xxxxxxx"; ///insert your wifi SSID here  
char pass[] = "xxxxxxx"; ///insert your wifi password here

#define DHTPIN 13         
#define DHTTYPE DHT11     

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

void sendSensor()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature(); 

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Blynk.virtualWrite(V1, h);
  Blynk.virtualWrite(V0, t);
}

void setup()
{
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  dht.begin();

  // Set interval to 2 seconds (2000 milliseconds)
  timer.setInterval(2000L, sendSensor);
}

void loop()
{
  Blynk.run();
  timer.run();
}
