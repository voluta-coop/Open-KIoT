#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "DHT.h"

#define DHTPIN 14

#define DHTTYPE DHT11   // DHT 11


// Wifi
#define WIFI_SSID "XXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

// Telegram
#define BOT_TOKEN "XXXXXXXXXXXXXXXX"
#define CHAT_ID "XXXXXXXXXXX"

WiFiClientSecure secured_client;

UniversalTelegramBot bot(BOT_TOKEN, secured_client);

DHT dht(DHTPIN, DHTTYPE);

const unsigned long BOT_MTBS = 1000; // mean time between scan messages
unsigned long bot_lasttime; // last time messages' scan has been done

float temperatura;
float humedad;

void setup()
{
  Serial.begin(9600);
  Serial.println(F("DHT11 test!"));
  
  dht.begin();
  
  // attempt to connect to Wifi network:
  Serial.print("Conectando al Wifi ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
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
  humedad = dht.readHumidity();
  temperatura = dht.readTemperature();
  
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    
    bot_lasttime = millis();
  }
}
void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID )
    {
      bot.sendMessage(chat_id, "Usuario no autorizado", "");
    }
    else
    {
      String text = bot.messages[i].text;
      String from_name = bot.messages[i].from_name;
      if (from_name == "")
        from_name = "Guest";
      if (text == "/temp")
      {   String msg = "Temperatura ";
          msg += msg.concat(temperatura);
          msg += " ºC";
          bot.sendMessage(chat_id,msg, ""); 
      }
      if (text == "/hum")
      {  
          String msg = "Humedad ";
          msg += msg.concat(humedad);
          msg += " %"; 
          bot.sendMessage(chat_id,msg, ""); 
      }
      if (text == "/start")
      {
        String welcome = "Lecturas del sensor DHT11.\n";
        welcome += "/temp : Temperatura en ºC \n";
        welcome += "/hum : Humedad relativa\n";
        bot.sendMessage(chat_id, welcome, "Markdown");
      }
    }
  }
}
