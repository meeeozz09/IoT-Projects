#define BLYNK_TEMPLATE_ID "xxxxxxxxx" ///insert your template ID here
#define BLYNK_TEMPLATE_NAME "xxxxxxxxx" ///insert your template name here
#define BLYNK_AUTH_TOKEN "xxxxxxxxx" ///insert your auth token here

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

char auth[] = "xxxxxxx"; ///insert your auth token here

/// insert your wifi SSID and password here
char ssid[] = "xxxxxxx";
char pass[] = "xxxxxxxx";

int read_hujan = 0;

#define sensor_hujan 4

BlynkTimer timer;
int hold=0;
void hujanNotify()
{
   read_hujan = digitalRead(sensor_hujan);

 if (read_hujan==0 && hold==0) {
     Serial.println("Hujan");
     Blynk.logEvent("hujan","Awas Terjadi Hujan !!!");
    hold=1;
  }
  else if (read_hujan==1 && hold==1)
  {
     Serial.println("Reda");
     Blynk.logEvent("reda","hujan sudah reda");
    hold=0;
  } 
}

void setup(){
  pinMode(sensor_hujan,INPUT);
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);
  timer.setInterval(2500L, hujanNotify);
}

void loop(){

  Blynk.run();
  timer.run();

}