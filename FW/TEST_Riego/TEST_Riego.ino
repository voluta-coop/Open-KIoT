#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Wifi
#define WIFI_SSID "XXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

// Telegram
#define BOT_TOKEN "XXXXXXXXXXXXXXXX"
#define CHAT_ID "XXXXXXXXXXX"

// Pines

#define PIN_BOMBA 26
#define PIN_BOYA 27
#define PIN_HUM_TIERRA 35


WiFiClientSecure secured_client;

UniversalTelegramBot bot(BOT_TOKEN, secured_client);


const unsigned long BOT_MTBS = 1000; // mean time between scan messages
unsigned long bot_lasttime; // last time messages' scan has been done

bool bomba = false;
bool boya = false;
int humedad_tierra = 0;
int hum_min = 400;
int hum_max = 1500;
unsigned long tiempo = 0;

void setup()
{
  Serial.begin(9600);
  
  
  // attempt to connect to Wifi network:
  Serial.print("Conectando al Wifi ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org

  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_BOYA, INPUT);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi conectado. Dirección IP: ");
  Serial.println(WiFi.localIP());
}
void loop()
{

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  if(millis() - tiempo > 60000){
    
    boya = digitalRead(PIN_BOYA);
    humedad_tierra = analogRead(PIN_HUM_TIERRA);
  
    if(boya == false){
      bot.sendMessage(CHAT_ID,"Debes rellenar el agua del riego", "");
      bomba = false;
    }
  
    else if(boya == true && humedad_tierra < hum_min && bomba == false) {
      bot.sendMessage(CHAT_ID,"Riego encendido", "");
      bomba = true;
    }
  
    else if(boya == true && humedad_tierra > hum_max){
      bomba = false;
      bot.sendMessage(CHAT_ID,"Riego apagado", "");
    }
    
    digitalWrite(PIN_BOYA, boya);
  }
}