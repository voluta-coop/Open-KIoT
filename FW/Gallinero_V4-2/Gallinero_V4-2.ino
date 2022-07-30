#include <ETH.h>
#include <WiFiSTA.h>
#include <WiFiGeneric.h>
#include <WiFiType.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <WiFiScan.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <WiFiMulti.h>
#include <TimeLib.h>
#include <sunset.h>
#include <ctype.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include "BluetoothSerial.h"


#define LATITUDE        39.9875
#define LONGITUDE       -0.0392
#define DST_OFFSET      2
#define MPM_OFFSET      0  // Offset en minutos para adelantar o retrasar la apertura y el cierre.


// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "XXXXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" 
#define CHAT_ID "XXXXXXXXXXX"

#define PIN_LED_1 2
#define PIN_LED_2 15
#define PIN_PULSADOR_1 17
#define PIN_PULSADOR_2 16
#define PIN_AGUA 27
#define PIN_PIENSO 14
#define PIN_SERVO 12
#define PIN_CUBIERTA 26
#define PIN_FOTO 25
#define PIN_FLASH 33

//Estados de configuración
#define CONFIG_SET_UP 0
#define CONFIG_DOOR_DOWN 1
#define CONFIG_DOOR_UP 2
#define CONFIG_DOOR_TEST_DOWN 3
#define CONFIG_DOOR_TEST_UP 4
#define CONFIG_DOOR_OK 5
#define CONFIG_OK 6
//Constantes Servo

#define MANTENER 90
#define ABRIR 180
#define CERRAR 0

#define ABIERTA 0
#define CERRADA 1
#define MOVIENDO 2

// Constantes LEDs
#define OFF 1000
#define ON 100
#define F_1HZ 1
#define F_2HZ 2
#define F_5HZ 5
#define F_10HZ 10

Servo myservo;

Preferences preferences;

String ssids_array[50];
String network_string;
String connected_string;

const char* pref_ssid = "";
const char* pref_pass = "";
String client_wifi_ssid;
String client_wifi_password;

const char* bluetooth_name = "eGalliner 1";

long start_wifi_millis;
long wifi_timeout = 10000;
bool bluetooth_disconnect = false;

enum wifi_setup_stages { NONE, SCAN_START, SCAN_COMPLETE, SSID_ENTERED, WAIT_PASS, PASS_ENTERED, WAIT_CONNECT, LOGIN_FAILED };
enum wifi_setup_stages wifi_stage = NONE;

BluetoothSerial SerialBT;

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

unsigned long bot_lasttime; // last time messages' scan has been done

int ledStatus = 0;
int estado = ABIERTA;
int estado_ant = ABIERTA;

// Tiempo

const uint8_t _usDSTStart[22] = { 8,14,13,12,10, 9, 8,14,12,11,10, 9,14,13,12,11, 9};
const uint8_t _usDSTEnd[22]   = { 1, 7, 6, 5, 3, 2, 1, 7, 5, 4, 3, 2, 7, 6, 5, 4, 2};
static const char ntpServerName[] = "us.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
SunSet sun;
int mpm;
int sunrise;
int sunset;
  
//---------------------

long int crono_puerta = 0;
long int crono_puerta_auto = 0;

int huevos;

bool agua = 0;
bool pienso = 0;
bool cajon = 0;
bool mensaje = 0;
bool cajon_huevos = 0;
bool rst = 0;
bool msg_cajon = false;
bool manual = false;

// Variables de los LEDs
bool state_led_1 = LOW;
bool state_led_2 = LOW;
bool state_led_1_last = LOW;
bool state_led_2_last = LOW;

//Estado entradas digitales
bool state_p_1 = false;
bool state_p_2 = false;
bool state_p_1_last = false;
bool state_p_2_last = false;
bool state_cajon = true;
bool state_agua = false;
bool state_pienso = false;
bool state_cajon_last = true;
bool state_agua_last = false;
bool state_pienso_last = false;
bool puerta_autom = false;
bool puerta_autom_ant = false;

// Variables puerta
int servo = MANTENER;
int servo_ant = MANTENER;
bool abrir = false;
bool cerrar = false;
bool abierto = false;
bool cerrado = false;
bool movimiento = false;
bool enviar = false;

// Variables de tiempo generales
unsigned int  previousMillis_1 = 0;
unsigned int  previousMillis_2 = 0;
unsigned int  t_p_1 = 0;
unsigned int  t_p_2 = 0;
unsigned int  t_servo_down;
unsigned int  t_servo_up;
unsigned int  t_servo_crono = 0;
unsigned int  t_mantener = 0;
unsigned int  t_cajon = 0;

int state = CONFIG_SET_UP;

time_t getNtpTime()
{
    IPAddress ntpServerIP; // NTP server's ip address

    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    Serial.println("Transmit NTP Request");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            Serial.println("Receive NTP Response");
            Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            return secsSince1900 - 2208988800UL + (DST_OFFSET * SECS_PER_HOUR);
        }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(address, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

// utility function for digital clock display: prints leading 0
String twoDigits(int digits)
{
    if(digits < 10) {
        String i = '0'+String(digits);
        return i;
    }
    else {
        return String(digits);
    }
}

void disconnect_bluetooth(){
  delay(1000);
  Serial.println("BT stopping");
  SerialBT.println("Desconectant el Bluetooth...");
  delay(1000);
  SerialBT.flush();
  SerialBT.disconnect();
  SerialBT.end();
  Serial.println("BT stopped");
  delay(1000);
  bluetooth_disconnect = false;
}

void wifi() {

  if (bluetooth_disconnect)
  {
    disconnect_bluetooth();
  }

  switch (wifi_stage)
  {
    case SCAN_START:
      SerialBT.println("Escanetjant xarxes WiFi");
      Serial.println("Scanning Wi-Fi networks");
      scan_wifi_networks();
      SerialBT.println("Si us plau, introdueix el nombre de la teva xarxa Wifi (1, 2, 3...)");
      wifi_stage = SCAN_COMPLETE;
      break;

    case SSID_ENTERED:
      SerialBT.println("Si us plau, introdueix la contrasenya de la xarxa Wifi seleccionada.");
      Serial.println("Please enter your Wi-Fi password");
      wifi_stage = WAIT_PASS;
      break;

    case PASS_ENTERED:
      SerialBT.println("Si us plau, espera a la connexió WiFi...");
      Serial.println("Please wait for Wi_Fi connection...");
      wifi_stage = WAIT_CONNECT;
      preferences.putString("pref_ssid", client_wifi_ssid);
      preferences.putString("pref_pass", client_wifi_password);
      if (init_wifi()) { // Connected to WiFi
        connected_string = "ESP32 IP: ";
        connected_string = connected_string + WiFi.localIP().toString();
        SerialBT.println(connected_string);
        Serial.println(connected_string);
        bluetooth_disconnect = true;
      } else { // try again
        wifi_stage = LOGIN_FAILED;
      }
      break;

    case LOGIN_FAILED:
      SerialBT.println("La connexió WiFi ha fallat.");
      Serial.println("Wi-Fi connection failed");
      delay(2000);
      wifi_stage = SCAN_START;
      break;
  }
}

bool init_wifi(){

  wifi();
  
  String temp_pref_ssid = preferences.getString("pref_ssid");
  String temp_pref_pass = preferences.getString("pref_pass");
  pref_ssid = temp_pref_ssid.c_str();
  pref_pass = temp_pref_pass.c_str();

  Serial.println(pref_ssid);
  Serial.println(pref_pass);

  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

  start_wifi_millis = millis();
  WiFi.begin(pref_ssid, pref_pass);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  
  Serial.print(F("Connecting to Wifi..."));
  
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    
    if(WiFi.status() == WL_CONNECTED){
      Serial.print(F("\nWiFi connected. IP address: "));
      Serial.println(WiFi.localIP());

    }
    
    if (millis() - start_wifi_millis > wifi_timeout) {
      WiFi.disconnect(true, true);
      return false;
    }
  }
      
  return true;
}

void scan_wifi_networks(){
  WiFi.mode(WIFI_STA);
  // WiFi.scanNetworks will return the number of networks found
  int n =  WiFi.scanNetworks();
  if (n == 0) {
    SerialBT.println("No s'han trobat xarxes WiFi");
  } else {
    SerialBT.println();
    SerialBT.print(n);
    SerialBT.println(" xarxes WiFi trobades.");
    delay(1000);
    for (int i = 0; i < n; ++i) {
      ssids_array[i + 1] = WiFi.SSID(i);
      Serial.print(i + 1);
      Serial.print(F(": "));
      Serial.println(ssids_array[i + 1]);
      network_string = i + 1;
      network_string = network_string + ": " + WiFi.SSID(i) + " (dB's: " + WiFi.RSSI(i) + ")";
      SerialBT.println(network_string);
    }
    wifi_stage = SCAN_COMPLETE;
  }
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){

  
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    wifi_stage = SCAN_START;
  }

  if (event == ESP_SPP_DATA_IND_EVT && wifi_stage == SCAN_COMPLETE) { // data from phone is SSID
    int client_wifi_ssid_id = SerialBT.readString().toInt();
    client_wifi_ssid = ssids_array[client_wifi_ssid_id];
    wifi_stage = SSID_ENTERED;
  }

  if (event == ESP_SPP_DATA_IND_EVT && wifi_stage == WAIT_PASS) { // data from phone is password
    client_wifi_password = SerialBT.readString();
    client_wifi_password.trim();
    wifi_stage = PASS_ENTERED;
  }

}

void callback_show_ip(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    SerialBT.print(F("ESP32 IP: "));
    SerialBT.println(WiFi.localIP());
    bluetooth_disconnect = true;
  }
}

void IRAM_ATTR alarma_cajon() {
  cajon = 1;
}

void puerta(int accion) {
  
  if (accion == ABRIR && !movimiento) {
    Serial.println("Abriendo...");
    crono_puerta = millis();
    movimiento = true;
    myservo.write(ABRIR);
  }
  
  else if (accion == CERRAR && !movimiento) {
    Serial.println("Cerrando...");
    crono_puerta = millis();
    movimiento = true;
    myservo.write(CERRAR);
  }
  
  else if (accion == MANTENER && movimiento) {
    Serial.println(F("Voy a mantener el estado actual"));
    myservo.write(MANTENER);
    movimiento = false;
    enviar = true;
  }
}

void handleNewMessages(int numNewMessages){
  
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  digitalWrite(PIN_FOTO, LOW);
  digitalWrite(PIN_FLASH, LOW);
  
  for (int i = 0; i < numNewMessages; i++)
  {

    String text = bot.messages[i].text;
    
    text.remove(0,1);
    
    Serial.print(text);
    Serial.println(" Recibido");
    
    if (text == "Foto")
    {
      Serial.println("Foto");
      digitalWrite(PIN_FOTO, HIGH);
    }

    else if (text == "Flash")
    {
      digitalWrite(PIN_FLASH, HIGH);
      Serial.println("Flash");
    }
    
    else if (cajon_huevos) {

      huevos = preferences.getInt("huevos", 0);
      huevos += text.toInt();
      preferences.putInt("huevos", huevos);
      cajon_huevos = 0;
      Serial.print(F("Huevos: ")); Serial.println(huevos);
      bot.sendMessage(CHAT_ID, "Porteu " + String(huevos) + " ous recollits.", "");
    }
  }
}

void outputs() {


  digitalWrite(PIN_LED_1, state_led_1);
  digitalWrite(PIN_LED_2, state_led_2);
  
  //if (state < CONFIG_OK) puerta(servo);

  puerta(servo);
}

void leds(int freq_1, int freq_2) { // Esta función enciende el led que se indica y a la frecuencia que se le indica en Hz
  
  int per_1 = 500 / freq_1;
  int per_2 = 500 / freq_2;


  if ((millis() - previousMillis_1) >= per_1 && freq_1 != OFF && freq_1 != ON) {
    state_led_1 = !state_led_1;
    previousMillis_1 = millis();
  }
  else if (freq_1 == OFF) state_led_1 = LOW;
  else if (freq_1 == ON) state_led_1 = HIGH;

  if ((millis() - previousMillis_2) >= per_2 && freq_2 != OFF && freq_2 != ON) {
    state_led_2 = !state_led_2;
    previousMillis_2 = millis();
  }
  else if (freq_2 == OFF) state_led_2 = LOW;
  else if (freq_2 == ON) state_led_2 = HIGH;
}

void inputs() {
  
  state_p_1_last = state_p_1;
  state_p_2_last = state_p_2;
  state_cajon_last = state_cajon;
  state_agua_last = state_agua;
  state_pienso_last = state_pienso;
  
  state_p_1 = digitalRead(PIN_PULSADOR_1);
  state_p_2 = digitalRead(PIN_PULSADOR_2);
  state_cajon = digitalRead(PIN_CUBIERTA);

  // Solo lee el estado del agua y el pienso si el cajón está cerrado.
  if (state_cajon) state_agua = digitalRead(PIN_AGUA);
  if (state_cajon) state_pienso = digitalRead(PIN_PIENSO);

  if(!state_cajon && state_cajon_last){
    msg_cajon = true;
    t_cajon = millis();
  }

  else if (state_cajon && state_cajon_last) t_cajon = millis();

  if(!state_cajon && !state_cajon_last && (millis() - t_cajon > 600000) && msg_cajon){
    msg_cajon = false;
    bot.sendMessage(CHAT_ID, "S'ha quedat obert el calaix del galliner.", "");
  }
  
  if((!state_p_1_last && !state_p_1) && (!state_p_2_last && state_p_2)) manual = false;

  //Serial.print(String(state) + " " + String(state_p_1)  + String(state_p_1_last) + " " + String(state_p_2) + String(state_p_2_last) + " " + String(manual));
  
  if((!state_p_1_last && state_p_1) && (!state_p_2_last && !state_p_2)){

    if(estado == ABIERTA){
      servo = CERRAR;
      manual = true;
    }
  
    else if(estado == CERRADA){
      servo = ABRIR;
      manual = true;
    }
  }

  
  //Serial.print(state_cajon);Serial.print(" ");Serial.print(state_agua);Serial.print(" ");Serial.println(state_pienso);
}

void sol(){
  
  static int currentDay = 32;

  if (currentDay != day()) {
      sun.setCurrentDate(year(), month(), day());
      currentDay = day();
  }

    mpm = hour() * 60 + minute();
    sunrise = static_cast<int>(sun.calcSunrise());
    sunset = static_cast<int>(sun.calcSunset());
    //sunset = 1082;
    /*
    Serial.print(F("Sunrise at "));
    Serial.print(sunrise/60);
    Serial.print(F(":"));
    Serial.print(twoDigits(sunrise%60));
    Serial.print(F("am, Sunset at "));
    Serial.print(sunset/60);
    Serial.print(F(":"));
    Serial.print(twoDigits(sunset%60));
    Serial.println(F("pm"));*/
    
    if (estado == CERRADA && mpm + MPM_OFFSET>= sunrise && mpm < sunset){
      servo = ABRIR;
      Serial.println("Es hora de abrir");
    }
    else if(estado == ABIERTA && (mpm - MPM_OFFSET >= sunset || mpm - MPM_OFFSET < sunrise)){
      Serial.println("Es hora de cerrar");
      servo = CERRAR;
    }
}

void config_puerta() {
  
  while (state != CONFIG_OK) {

    //Serial.print(state); Serial.print(F(" ")); Serial.println(servo);

    switch (state) {

      case CONFIG_SET_UP:
      
        t_servo_up = preferences.getUInt("t_up",0);
        t_servo_down = preferences.getUInt("t_down",0);
        
        while (state == CONFIG_SET_UP) {

          Serial.println(F("CONFIG_SET_UP"));
          inputs();

          if ((state_p_2 && state_p_2_last)) {
            leds(F_5HZ, OFF);
            servo = ABRIR;
          }

          else {
            leds(ON, OFF);
            servo = MANTENER;
          }
          //Serial.print(state_p_1); Serial.println(state_p_1_last);
          if (state_p_1 && !state_p_1_last) state = CONFIG_DOOR_DOWN;

          outputs();
        }
        break;

      case CONFIG_DOOR_DOWN:

        while (state == CONFIG_DOOR_DOWN) {

          Serial.println("CONFIG_DOOR_DOWN");

          inputs();

          if ((state_p_2 && state_p_2_last)) {
            leds(ON, F_5HZ);
            servo = CERRAR;
          }
          else {
            leds(OFF, F_5HZ);
            servo = MANTENER;
          }

          if (state_p_2 && !state_p_2_last) t_p_2 = millis();
          if ((!state_p_2 && state_p_2_last)) t_servo_down += millis() - t_p_2;

          if (state_p_1 && !state_p_1_last) state = CONFIG_DOOR_UP;

          outputs();
        }

        preferences.putUInt("t_down", t_servo_down);

        break;

      case CONFIG_DOOR_UP:

        while (state == CONFIG_DOOR_UP) {

          Serial.println(F("CONFIG_DOOR_UP"));

          inputs();

          if ((state_p_2 && state_p_2_last)) {
            leds(F_5HZ, ON);
            servo = ABRIR;
          }
          else {
            leds(F_5HZ, OFF);
            servo = MANTENER;
          }

          if (state_p_2 && !state_p_2_last) t_p_2 = millis();
          if (!state_p_2 && state_p_2_last) t_servo_up += millis() - t_p_2;

          if (state_p_1 && !state_p_1_last) state = CONFIG_DOOR_TEST_DOWN;

          outputs();
        }

        preferences.putUInt("t_up", t_servo_up);

        break;

      case CONFIG_DOOR_TEST_DOWN:

        t_servo_crono = millis();

        while (state == CONFIG_DOOR_TEST_DOWN) {

          Serial.println(state);

          inputs();

          if (millis() - t_servo_crono < t_servo_down) {
            leds(OFF, F_2HZ);
            servo = CERRAR;
          }
          else {
            servo = MANTENER;
            leds(OFF, ON);
            if (state_p_1 && !state_p_1_last) state = CONFIG_DOOR_TEST_UP;
            if (state_p_2 && !state_p_2_last) state = CONFIG_SET_UP;
          }
          estado = CERRADA;
          outputs();
        }
        break;

      case CONFIG_DOOR_TEST_UP:

        t_servo_crono = millis();

        while (state == CONFIG_DOOR_TEST_UP) {

          Serial.println(F("CONFIG_DOOR_TEST_UP"));

          inputs();

          if (millis() - t_servo_crono < t_servo_up) {
            leds(F_2HZ, OFF);
            servo = ABRIR;
          }
          else {
            servo = MANTENER;
            leds(ON, OFF);
            abierto = true;
            if (state_p_1 && !state_p_1_last) state = CONFIG_DOOR_OK;
          }
          estado = ABIERTA;
          outputs();
        }
        break;

      case CONFIG_DOOR_OK:

        delay(100);

        while (state == CONFIG_DOOR_OK) {

          Serial.println(F("CONFIG_DOOR_OK"));

          inputs();

          leds(F_1HZ, F_1HZ);

          if (state_p_1 && !state_p_1_last) {
            leds(OFF, OFF);
            state = CONFIG_OK;
            ESP.restart();
          }

          outputs();
        }
        
        break;
    }
  }
}

void gestion_puerta() {
  
  estado_ant = estado;
  servo_ant = servo;
  
  if(servo_ant == MANTENER && servo != MANTENER){

    t_servo_up = preferences.getUInt("t_up", 0);
    t_servo_down = preferences.getUInt("t_down", 0);
  }
  
  if (estado == CERRADA && servo == ABRIR &&  millis() - crono_puerta > t_servo_up && movimiento) {

    servo = MANTENER;
    estado = ABIERTA;
  }

  else if (estado == ABIERTA && servo == CERRAR && millis() - crono_puerta > t_servo_down && movimiento) {

    servo = MANTENER;
    estado = CERRADA;
  }
}

void gestion_bot() {
  
  if (millis() - bot_lasttime > BOT_MTBS) {
        
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    //Serial.println(numNewMessages);
    
    while (numNewMessages)
    {
      Serial.println(F("got response"));
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

  if (state_pienso && !state_pienso_last) {
    bot.sendMessage(CHAT_ID, "Queda poc pinso.", "");
    Serial.println("Queda poco pienso");
  }
  else if (!state_pienso && state_pienso_last) {
    bot.sendMessage(CHAT_ID, "S'ha emplenat el pinso.", "");
    Serial.println("Se ha rellenado el pienso");
  }

  if (state_agua && !state_agua_last) {
    bot.sendMessage(CHAT_ID, "Queda poca aigua.", "");
    Serial.println("Queda poca agua");
  }

  else if (!state_agua && state_agua_last) {
    bot.sendMessage(CHAT_ID, "S'ha emplenat l'aigua", "");
    Serial.println("Se ha rellenado el agua");
  }

  if (state_cajon && !state_cajon_last) {
    cajon_huevos = 1;
    bot.sendMessage(CHAT_ID, "Has obert el calaix del galliner Quants ous has recollit?", "");
    //Añadir aviso cubierta abierta.
    //crono_cajon = millis();
    Serial.println("cajon abierto");
    cajon = 0;
  }

  //Serial.print(mensaje);Serial.print(F(" "));Serial.print(estado);Serial.print(F(" "));Serial.print(enviar);Serial.print(F(" "));Serial.println(!movimiento);
  if (mensaje == 1 && estado == ABIERTA && enviar && !movimiento) {
      Serial.println("3");
      Serial.println();
      mensaje = 0;
      enviar = false;
      bot.sendMessage(CHAT_ID, "Porta oberta.", "");
    }

    else if (mensaje == 1 && estado == CERRADA && enviar && !movimiento){
      Serial.println(F("5"));
      Serial.println();
      mensaje = false;
      enviar = false;
      bot.sendMessage(CHAT_ID, "Porta tancada.", "");
    }
}

void setup(){
  
  preferences.begin("voluta", false);
  
  SerialBT.begin(bluetooth_name);
  
  Serial.begin(115200);
  Serial.println();
  
  if (!init_wifi()) { // Connect to Wi-Fi fails
    SerialBT.register_callback(callback);
  } else {
    SerialBT.register_callback(callback_show_ip);
  }
  
  pinMode(PIN_AGUA, INPUT);
  pinMode(PIN_PIENSO, INPUT);
  pinMode(PIN_CUBIERTA, INPUT);
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  pinMode(PIN_PULSADOR_1, INPUT);
  pinMode(PIN_PULSADOR_2, INPUT);
  pinMode(PIN_FOTO, OUTPUT);
  pinMode(PIN_FLASH, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  myservo.setPeriodHertz(50);    // standard 50 hz servo
  
  myservo.attach(PIN_SERVO, 1000, 2000);

  t_servo_up = preferences.getUInt("t_up", 0);
  t_servo_down = preferences.getUInt("t_down", 0);
  huevos = preferences.getInt("huevos", 0);

  Serial.print("t_servo_up: ");Serial.println(t_servo_up);
  Serial.print("t_servo_down: ");Serial.println(t_servo_down);
  Serial.println();
  delay(1000);
  
  if (t_servo_up == 0 || t_servo_down == 0){
    state = 0;
    config_puerta();
  }
  
  bot.sendMessage(CHAT_ID, "eGalliner inicialitzat.", "");

  sun.setPosition(LATITUDE, LONGITUDE, DST_OFFSET);
  sun.setTZOffset(DST_OFFSET);
  setSyncProvider(getNtpTime);
  setSyncInterval(60*60);

  Serial.print("Fecha:");
  Serial.print(day());
  Serial.print("/");
  Serial.print(month());
  Serial.print("/");
  Serial.println(year());
  Serial.print("Hora:");
  Serial.print(hour());
  Serial.print(":");
  Serial.println(minute());

  sol();
  
  bot.sendMessage(CHAT_ID, "Apertura a les: " + String(sunrise/60) + ":" + String(twoDigits(sunrise%60)) + ". Tancament a les " + String(sunset/60) + ":" + String(twoDigits(sunset%60)) + ".", "");
}

void loop()
{

  if (WiFi.status() == WL_CONNECTED) digitalWrite(PIN_LED_2, HIGH);

  else if (WiFi.status() != WL_CONNECTED && !movimiento && servo_ant == MANTENER && servo == MANTENER ) {
    init_wifi();
    
    digitalWrite(PIN_LED_2, LOW);
  }
  
  inputs(); //if(!movimiento) inputs();

  if (state_p_1 && state_p_2 && !movimiento) {
    preferences.putInt("huevos", 0);
    preferences.putUInt("t_up", 0);
    preferences.putUInt("t_down", 0);
    state = 0;
    config_puerta();
  }
 
  if(!manual) sol();

  if(estado == ABIERTA) Serial.println("Abierta");
  else if(estado == CERRADA) Serial.println("Cerrada");
  
  gestion_puerta();
  
  gestion_bot();
  
  outputs();
}
