#include "DHTesp.h"
#include <Ticker.h>
#include <ArduinoWebsockets.h>
#include "esp_http_server.h"
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "DHTesp.h"
#include <Ticker.h>
#include <ArduinoWebsockets.h>
#include "esp_http_server.h"
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

#define PIN_DHT       25
#define PIN_MICRO     33
#define PIN_LDR       34
#define PIN_LED_RUIDO 2
#define PIN_LED_LUM   15
#define PIN_LED_TH    14

#define T_LEC_RUIDO   100

// Config Telegram
#define CHAT_ID "XXXXXXXXXXX"
#define BOT_TOKEN "XXXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

bool msg_ruido_enviado = 0;
bool msg_th_enviado = 0;
bool msg_lum_enviado = 0;

unsigned int t_lum_msg = 0;
unsigned int t_ruido_msg = 0;
unsigned int t_th_msg = 0;

// Horarios Colegio

#define HORA_APERTURA_COLE 9*60 // 9 de la mañana
#define HORA_CIERRE_COLE 17*60 // 17 de la tade

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// Variables WiFi
String ssids_array[50];
String network_string;
String connected_string;

unsigned int t_5 = 0;

const char* pref_ssid = "";
const char* pref_pass = "";
String client_wifi_ssid;
String client_wifi_password;

const char* bluetooth_name = "Open_KIoT";

long start_wifi_millis;
long wifi_timeout = 10000;
bool bluetooth_disconnect = false;

enum wifi_setup_stages { NONE, SCAN_START, SCAN_COMPLETE, SSID_ENTERED, WAIT_PASS, PASS_ENTERED, WAIT_CONNECT, LOGIN_FAILED };
enum wifi_setup_stages wifi_stage = NONE;

using namespace websockets;
WebsocketsServer socket_server;
BluetoothSerial SerialBT;
Preferences preferences;

// Varialbes ruido
int lec_max = 0;
int lec_min = 0;
int lec = 0;
unsigned int t = 0;
unsigned int t_2 = 0;
unsigned int t_msg_ruido = 0;
int cnt_ruido = 0;
int aviso_ruido = 0;
int t_delay_ruido = 0;
bool led_ruido = 0;
int t_reset_ruido = 12000; // 2 min
bool msg_ruido = false;
const int sensitivity = -58;     // microphone sensitivity in dB
const int gain = 20;             // op-amp gain dB
double spl_db = 0.0;

// Variables luminosidad.
bool  led_lum = LOW;
unsigned int t_3 = 0;
unsigned int t_msg_lum = 0;
int aviso_lum = 0;
int t_delay_lum = 0;
int t_reset_lum = 10800000; // 3h
bool msg_lum = false;
int lum = 0;
int lux = 0;

// Variables temperatura y humedad.
bool led_th = LOW;
float temp = 0.0;
float hum = 0.0;
unsigned int t_4 = 0;
unsigned int t_msg_th = 0;
int aviso_th = 0;
int t_delay_th = 0;
int t_reset_th = 3600000; // 1h
bool msg_th = false;

DHTesp dht;

void tempTask(void *pvParameters);
bool getTemperature();
void triggerGetTemp();

/** Task handle for the light value read task */
TaskHandle_t tempTaskHandle = NULL;
/** Ticker for temperature reading */
Ticker tempTicker;
/** Comfort profile */
ComfortState cf;
/** Flag if task should run */
bool tasksEnabled = false;
/** Pin number for DHT11 data pin */
int dhtPin = PIN_DHT;

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 2;     // Central European Time
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

//Telegram
String msg_comfortStatus;

// Variables de horarios
int mpm = 0;
int mpm_apertura = HORA_APERTURA_COLE;
int mpm_cierre = HORA_CIERRE_COLE;

bool horario_lectivo = 0;

void setup() {

  Serial.begin(9600);

  pinMode(PIN_LED_RUIDO, OUTPUT);
  pinMode(PIN_LED_LUM, OUTPUT);
  pinMode(PIN_LED_TH, OUTPUT);

  preferences.begin("wifi_access", false);

  if (!init_wifi()) { // Connect to Wi-Fi fails
    SerialBT.register_callback(callback);
  } else {
    SerialBT.register_callback(callback_show_ip);
  }

  SerialBT.begin(bluetooth_name);

  socket_server.listen(82);

  initTemp();

  tasksEnabled = true;

  bot.sendMessage(CHAT_ID, "Inicialitzat el Open KIoT 1.", "");

  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

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
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
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

void tiempo() {

  mpm = hour() * 60 + minute();



  if (mpm >= mpm_apertura && mpm <= mpm_cierre && weekday() != 1 && weekday() != 7) horario_lectivo = true;
  else horario_lectivo = false;
}

bool init_wifi()
{

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
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (WiFi.status() == WL_CONNECTED) {
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

void scan_wifi_networks()
{
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
      Serial.print(": ");
      Serial.println(ssids_array[i + 1]);
      network_string = i + 1;
      network_string = network_string + ": " + WiFi.SSID(i) + " (dB's:" + WiFi.RSSI(i) + ")";
      SerialBT.println(network_string);
    }
    wifi_stage = SCAN_COMPLETE;
  }
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{

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

void callback_show_ip(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    SerialBT.print("Kit IoT Aula IP: ");
    SerialBT.println(WiFi.localIP());
    bluetooth_disconnect = true;
  }
}

void disconnect_bluetooth() {
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

bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
  dht.setup(dhtPin, DHTesp::DHT11);
  Serial.println("DHT initiated");

  // Start task to get temperature
  xTaskCreatePinnedToCore(
    tempTask,                       /* Function to implement the task */
    "tempTask ",                    /* Name of the task */
    4000,                           /* Stack size in words */
    NULL,                           /* Task input parameter */
    5,                              /* Priority of the task */
    &tempTaskHandle,                /* Task handle. */
    1);                             /* Core where the task should run */

  if (tempTaskHandle == NULL) {
    Serial.println("Failed to start task for temperature update");
    return false;
  } else {
    // Start update of environment data every 5 min
    tempTicker.attach(300, triggerGetTemp);
  }
  return true;
}

/**
   triggerGetTemp
   Sets flag dhtUpdated to true for handling in loop()
   called by Ticker getTempTimer
*/
void triggerGetTemp() {
  if (tempTaskHandle != NULL) {
    xTaskResumeFromISR(tempTaskHandle);
  }
}

/**
   Task to reads temperature from DHT11 sensor
   @param pvParameters
      pointer to task parameters
*/
void tempTask(void *pvParameters) {
  Serial.println("tempTask loop started");
  while (1) // tempTask loop
  {
    if (tasksEnabled) {
      // Get temperature values
      getTemperature();
    }
    // Got sleep again
    vTaskSuspend(NULL);
  }
}

/**
   getTemperature
   Reads temperature from DHT11 sensor
   @return bool
      true if temperature could be aquired
      false if aquisition failed
*/
bool getTemperature() {
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return false;
  }

  float heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);

  temp = newValues.temperature - 1.5;
  hum = newValues.humidity;

  switch (cf) {
    case Comfort_TooHot:
      msg_comfortStatus = "Alerta, ambient calorós.";
      msg_th = true;
      break;
    case Comfort_TooCold:
      msg_comfortStatus = "Alerta, ambient fred.";
      msg_th = true;
      break;
    case Comfort_TooDry:
      msg_comfortStatus = "Alerta, ambient sec.";
      msg_th = true;
      break;
    case Comfort_TooHumid:
      msg_comfortStatus = "Alerta, ambient humit.";
      msg_th = true;
      break;
    case Comfort_HotAndHumid:
      msg_comfortStatus = "Alerta, ambient calorós i humit.";
      msg_th = true;
      break;
    case Comfort_HotAndDry:
      msg_comfortStatus = "Alerta, ambient calorós i sec.";
      msg_th = true;
      break;
    case Comfort_ColdAndHumid:
      msg_comfortStatus = "Alerta, ambient fred i humit.";
      msg_th = true;
      break;
    case Comfort_ColdAndDry:
      msg_comfortStatus = "Alerta, ambient fred i sec.";
      msg_th = true;
      break;
  }

  Serial.println(temp);

  return true;
}

void ruido() {

  if (aviso_ruido != 3) {

    t = millis();

    while (millis() - t < T_LEC_RUIDO) {

      lec = analogRead(PIN_MICRO);

      //Serial.println(lec);

      if (lec_min == 0) lec_min = lec;
      if (lec_max < lec) lec_max = lec;
      if (lec_min > lec) lec_min = lec;
    }



    double volts = ((double(lec_max - lec_min) * 3.3) / 4096.0) * 10.0;
    double volts_db = 20.0 * log10(volts / 0.001259);
    spl_db = volts_db + 94 + sensitivity - gain;

    Serial.println(spl_db);

    if (spl_db < 65) aviso_ruido = 0;
    if (spl_db >= 65) aviso_ruido = 1;
    if (spl_db >= 70) aviso_ruido = 2;
    if (spl_db >= 78) aviso_ruido = 3;

    switch (aviso_ruido) {
      case 0:
        led_ruido = LOW;
        break;
      case 1:
        t_delay_ruido = 300;
        break;
      case 2:
        t_delay_ruido = 80;
        break;
      case 3:
        led_ruido = HIGH;
        msg_ruido = true;
        break;
    }

    if (aviso_ruido == 1 || aviso_ruido == 2) {

      if (millis() - t_2 >= t_delay_ruido || t == 0) {
        t_2 = millis();
        led_ruido = !led_ruido;
      }
    }

    lec_max = 0;
    lec_min = 0;
  }

  if (aviso_ruido == 3 && msg_ruido) t_msg_ruido = millis();

  if ( millis() - t_msg_ruido > t_reset_ruido && t_msg_ruido != 0) {
    aviso_ruido = 0;
    cnt_ruido = 0;
  }
}

int lux_converter(int raw_val) {
  // Conversion rule
  float Vout = float(raw_val) * (3.3 / float(4095));// Conversion analog to voltage
  float RLDR = (10000 * (3.3 - Vout)) / Vout; // Conversion voltage to resistance
  int lux_conv = 500 / (RLDR / 1000); // Conversion resitance to lumen
  return lux_conv;
}

void luminosidad() {

  if (aviso_lum != 3) {

    for (int j = 0; j < 150; j++) {
      lum += analogRead(PIN_LDR);
      //Serial.println(analogRead(PIN_LDR));
    }

    lum = lum / 150;

    lux = lux_converter(lum);

    Serial.println(lux);

    if (lux > 450) aviso_lum = 0;
    if (lux <= 450) aviso_lum = 1;
    if (lux <= 400) aviso_lum = 2;
    if (lux < 350) aviso_lum = 3;



    switch (aviso_lum) {
      case 0:
        led_lum = LOW;
        break;
      case 1:
        t_delay_lum = 300;
        break;
      case 2:
        t_delay_lum = 80;
        break;
      case 3:
        led_lum = HIGH;
        break;
    }

    if (aviso_lum == 1 || aviso_lum == 2) {

      if (millis() - t_3 >= t_delay_lum || t == 0) {
        t_3 = millis();
        led_lum = !led_lum;
      }
    }
    else if (aviso_lum == 3) {

      // Enviar mensaje.
      msg_lum = true;
      t_msg_lum = millis();
    }
  }

  if ( millis() - t_msg_lum > t_reset_lum) aviso_lum = false;

  lum = 0;
}

void temp_hum() {
  if (!aviso_th) {
    if (!tasksEnabled) {
      // Wait 2 seconds to let system settle down
      delay(2000);
      // Enable task that will read values from the DHT sensor
      tasksEnabled = true;
      if (tempTaskHandle != NULL) {
        vTaskResume(tempTaskHandle);
      }
    }

    yield();
  }
  if (millis() - t_msg_th > t_reset_th) aviso_th = false;
}

void telegram() {

  if (millis() - t_lum_msg >= 60000) msg_lum_enviado = false;
  if (millis() - t_ruido_msg >= 30000) msg_ruido_enviado = false;
  if (millis() - t_th_msg >= 60000) msg_th_enviado = false;
  
  if (msg_th) {
    aviso_th = true;
    msg_th = false;
    msg_th_enviado = true;
    t_th_msg = millis();
    bot.sendMessage(CHAT_ID, msg_comfortStatus, "");
    bot.sendMessage(CHAT_ID, String(temp) + "ºC " + String(hum) + "%.", "");
    Serial.println("Aviso TH enviado");
  }

  if (msg_lum) {
    aviso_lum = true;
    msg_lum = false;
    msg_lum_enviado = true;
    t_lum_msg = millis();
    bot.sendMessage(CHAT_ID, "Iluminació insuficient a l'aula, " + String(lux) + " lux. Deu estar entre 350 lux i 1000 lux", "");
    Serial.println("Aviso LUM enviado");
  }

  if (msg_ruido) {
    msg_ruido = false;
    msg_ruido_enviado = true;
    t_ruido_msg = millis();
    bot.sendMessage(CHAT_ID, "Masa soroll a l'aula, " + String(spl_db) + " dB. Deu ser menor a 55 dB.", "");
    Serial.println("Aviso RUIDO enviado");
  }
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

void loop() {

  if (WiFi.status() != WL_CONNECTED) {

    digitalWrite(PIN_LED_RUIDO, led_ruido);
    digitalWrite(PIN_LED_LUM, led_lum);
    digitalWrite(PIN_LED_TH, led_th);

    delay(80);

    led_ruido = !led_ruido;
    led_lum = !led_lum;
    led_th = !led_th;

    init_wifi();

    if (WiFi.status() == WL_CONNECTED) {
      led_ruido = LOW;
      led_lum = LOW;
      led_th = LOW;

      digitalWrite(PIN_LED_RUIDO, led_ruido);
      digitalWrite(PIN_LED_LUM, led_lum);
      digitalWrite(PIN_LED_TH, led_th);
    }
  }

  tiempo ();

  if (horario_lectivo && WiFi.status() == WL_CONNECTED) {
    
    if(!msg_ruido_enviado) ruido();

    if(!msg_lum_enviado) luminosidad();

    if(!msg_th_enviado) temp_hum();

    telegram();

    digitalWrite(PIN_LED_RUIDO, led_ruido);
    digitalWrite(PIN_LED_LUM, led_lum);
    digitalWrite(PIN_LED_TH, led_th);
  }
}

#define PIN_DHT       25
#define PIN_MICRO     33
#define PIN_LDR       32
#define PIN_LED_RUIDO 2
#define PIN_LED_LUM   15
#define PIN_LED_TH    27
#define T_LEC_RUIDO   20

// Config Telegram
#define CHAT_ID "941876134"
#define BOT_TOKEN "1843657254:AAHsMVntAsn16NQVYnEXw6Z1gZESVfsxE1U"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// Variables WiFi
String ssids_array[50];
String network_string;
String connected_string;

unsigned int t_5 = 0;

const char* pref_ssid = "";
const char* pref_pass = "";
String client_wifi_ssid;
String client_wifi_password;

const char* bluetooth_name = "Open_KIoT_1";

long start_wifi_millis;
long wifi_timeout = 10000;
bool bluetooth_disconnect = false;

enum wifi_setup_stages { NONE, SCAN_START, SCAN_COMPLETE, SSID_ENTERED, WAIT_PASS, PASS_ENTERED, WAIT_CONNECT, LOGIN_FAILED };
enum wifi_setup_stages wifi_stage = NONE;

using namespace websockets;
WebsocketsServer socket_server;
BluetoothSerial SerialBT;
Preferences preferences;

// Varialbes ruido
int lec_max = 0;
int lec_min = 0;
int lec = 0;
unsigned int t = 0;
unsigned int t_2 = 0;
unsigned int t_msg_ruido = 0;
int cnt_ruido = 0;
int aviso_ruido = 0;
int t_delay_ruido = 0;
bool led_ruido = 0;
int t_reset_ruido = 12000; // 2 min
bool msg_ruido = false;

// Variables luminosidad.
bool  led_lum = LOW;
unsigned int t_3 = 0;
unsigned int t_msg_lum = 0;
int aviso_lum = 0;
int t_delay_lum = 0;
int t_reset_lum = 10800000; // 3h
bool msg_lum = false;
int lum = 0;

// Variables temperatura y humedad.
bool led_th = LOW;
float temp = 0.0;
float hum = 0.0;
unsigned int t_4 = 0;
unsigned int t_msg_th = 0;
int aviso_th = 0;
int t_delay_th = 0;
int t_reset_th = 3600000; // 1h
bool msg_th = false;

DHTesp dht;

void tempTask(void *pvParameters);
bool getTemperature();
void triggerGetTemp();

/** Task handle for the light value read task */
TaskHandle_t tempTaskHandle = NULL;
/** Ticker for temperature reading */
Ticker tempTicker;
/** Comfort profile */
ComfortState cf;
/** Flag if task should run */
bool tasksEnabled = false;
/** Pin number for DHT11 data pin */
int dhtPin = PIN_DHT;

//Telegram
String msg_comfortStatus;

void setup() {

  Serial.begin(9600);

  pinMode(PIN_LED_RUIDO, OUTPUT);
  pinMode(PIN_LED_LUM, OUTPUT);
  pinMode(PIN_LED_TH, OUTPUT);

  preferences.begin("wifi_access", false);
  
  if (!init_wifi()) { // Connect to Wi-Fi fails
    SerialBT.register_callback(callback);
  } else {
    SerialBT.register_callback(callback_show_ip);
  }

  SerialBT.begin(bluetooth_name);

  socket_server.listen(82);
  
  initTemp();
  
  tasksEnabled = true;
  
  bot.sendMessage(CHAT_ID, "Inicializado el Open KIoT.", "");
}

bool init_wifi()
{
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
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
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

void scan_wifi_networks()
{
  WiFi.mode(WIFI_STA);
  // WiFi.scanNetworks will return the number of networks found
  int n =  WiFi.scanNetworks();
  if (n == 0) {
    SerialBT.println("no networks found");
  } else {
    SerialBT.println();
    SerialBT.print(n);
    SerialBT.println(" networks found");
    delay(1000);
    for (int i = 0; i < n; ++i) {
      ssids_array[i + 1] = WiFi.SSID(i);
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(ssids_array[i + 1]);
      network_string = i + 1;
      network_string = network_string + ": " + WiFi.SSID(i) + " (Strength:" + WiFi.RSSI(i) + ")";
      SerialBT.println(network_string);
    }
    wifi_stage = SCAN_COMPLETE;
  }
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
  
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

void callback_show_ip(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    SerialBT.print("ESP32 IP: ");
    SerialBT.println(WiFi.localIP());
    bluetooth_disconnect = true;
  }
}

void disconnect_bluetooth(){
  delay(1000);
  Serial.println("BT stopping");
  SerialBT.println("Bluetooth disconnecting...");
  delay(1000);
  SerialBT.flush();
  SerialBT.disconnect();
  SerialBT.end();
  Serial.println("BT stopped");
  delay(1000);
  bluetooth_disconnect = false;
}

bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
  dht.setup(dhtPin, DHTesp::DHT11);
  Serial.println("DHT initiated");

  // Start task to get temperature
  xTaskCreatePinnedToCore(
      tempTask,                       /* Function to implement the task */
      "tempTask ",                    /* Name of the task */
      4000,                           /* Stack size in words */
      NULL,                           /* Task input parameter */
      5,                              /* Priority of the task */
      &tempTaskHandle,                /* Task handle. */
      1);                             /* Core where the task should run */

  if (tempTaskHandle == NULL) {
    Serial.println("Failed to start task for temperature update");
    return false;
  } else {
    // Start update of environment data every 2 min
    tempTicker.attach(3600, triggerGetTemp);
  }
  return true;
}

/**
 * triggerGetTemp
 * Sets flag dhtUpdated to true for handling in loop()
 * called by Ticker getTempTimer
 */
void triggerGetTemp() {
  if (tempTaskHandle != NULL) {
     xTaskResumeFromISR(tempTaskHandle);
  }
}

/**
 * Task to reads temperature from DHT11 sensor
 * @param pvParameters
 *    pointer to task parameters
 */
void tempTask(void *pvParameters) {
  Serial.println("tempTask loop started");
  while (1) // tempTask loop
  {
    if (tasksEnabled) {
      // Get temperature values
      getTemperature();
    }
    // Got sleep again
    vTaskSuspend(NULL);
  }
}

/**
 * getTemperature
 * Reads temperature from DHT11 sensor
 * @return bool
 *    true if temperature could be aquired
 *    false if aquisition failed
*/
bool getTemperature() {
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return false;
  }

  float heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);
  
  temp = newValues.temperature;
  hum = newValues.humidity;
  
  switch(cf) {
    case Comfort_TooHot:
      msg_comfortStatus = "Alerta: exceso de calor.";
      msg_th = true;
      break;
    case Comfort_TooCold:
      msg_comfortStatus = "Alerta: exceso de frío.";
      msg_th = true;
      break;
    case Comfort_TooDry:
      msg_comfortStatus = "Alerta: falta de humedad.";
      msg_th = true;
      break;
    case Comfort_TooHumid:
      msg_comfortStatus = "Alerta: exceso humedad.";
      msg_th = true;
      break;
    case Comfort_HotAndHumid:
      msg_comfortStatus = "Alerta: exceso de calor y humedad.";
      msg_th = true;
      break;
    case Comfort_HotAndDry:
      msg_comfortStatus = "Alerta: exceso de calor y falta de humedad.";
      msg_th = true;
      break;
    case Comfort_ColdAndHumid:
      msg_comfortStatus = "Alerta: exceso de frío y humedad.";
      msg_th = true;
      break;
    case Comfort_ColdAndDry:
      msg_comfortStatus = "Alerta: exceso de frío y falta de humedad.";
      msg_th = true;
      break;
  }

  Serial.println(temp);
  
  return true;
}

void ruido(){

  if(aviso_ruido != 3){
    
    t = millis();
    
    while (millis() - t < T_LEC_RUIDO){
      
      lec = analogRead(PIN_MICRO);
      
      //Serial.println(lec);
      
      if(lec_min == 0) lec_min = lec;
      if(lec_max < lec) lec_max = lec;
      if(lec_min > lec) lec_min = lec;
    }
  
    if((lec_max - lec_min) >= 27) cnt_ruido = cnt_ruido + 3;
    
    
    else if((lec_max - lec_min) < 27 && aviso_ruido != 3) cnt_ruido= cnt_ruido - 2;
    

    if(cnt_ruido <= 0) cnt_ruido = 0;

    //Serial.println(cnt_ruido);
    
    if(cnt_ruido < 200) aviso_ruido = 0;
    if(cnt_ruido >= 200) aviso_ruido = 1;
    if(cnt_ruido >= 700) aviso_ruido = 2;
    if(cnt_ruido >= 1600) aviso_ruido = 3;

    switch (aviso_ruido) {
    case 0:
      led_ruido = LOW;
      break;
    case 1:
      t_delay_ruido = 300;
      break;
    case 2:
    t_delay_ruido = 80;
      break;
    case 3:
      led_ruido = HIGH;
      msg_ruido = true;
      break;
  }
  
    if(aviso_ruido == 1 || aviso_ruido == 2){
      
      if(millis() - t_2 >= t_delay_ruido || t == 0){
        t_2 = millis();
        led_ruido = !led_ruido;
      }
    }

    lec_max = 0;
    lec_min = 0;
  }
  
  if(aviso_ruido == 3 && msg_ruido) t_msg_ruido = millis();
    
  if( millis() - t_msg_ruido > t_reset_ruido && t_msg_ruido != 0){
    aviso_ruido = 0;
    cnt_ruido = 0;
  }
}

void luminosidad(){

  if(aviso_lum != 3){
    
    for(int j = 0; j<150; j++){ 
      lum += analogRead(PIN_LDR);
      //Serial.println(analogRead(PIN_LDR));
    }

    lum = lum/150;

    lum = map(lum, 0, 4095, 0, 100);
  
    if(lum > 65) aviso_lum = 0;
    if(lum <= 65) aviso_lum = 1;
    if(lum <= 50) aviso_lum = 2;
    if(lum <= 40) aviso_lum = 3;
  
    
    
    switch (aviso_lum) {
    case 0:
      led_lum = LOW;
      break;
    case 1:
      t_delay_lum = 300;
      break;
    case 2:
    t_delay_lum = 80;
      break;
    case 3:
      led_lum = HIGH;
      break;
  }
  
    if(aviso_lum == 1 || aviso_lum == 2){
      
      if(millis() - t_3 >= t_delay_lum || t == 0){
        t_3 = millis();
        led_lum = !led_lum;
      }
    }
    else if(aviso_lum == 3){
      
      // Enviar mensaje.
      msg_lum = true;
      t_msg_lum = millis();
    }
  }
  
  if( millis() - t_msg_lum > t_reset_lum) aviso_lum = false;

  lum = 0;
}

void temp_hum(){
  if(!aviso_th){
    if (!tasksEnabled) {
      // Wait 2 seconds to let system settle down
      delay(2000);
      // Enable task that will read values from the DHT sensor
      tasksEnabled = true;
      if (tempTaskHandle != NULL) {
        vTaskResume(tempTaskHandle);
      }
    }
    
    yield();
  }
  if(millis() - t_msg_th > t_reset_th) aviso_th = false;
}

void telegram(){

  if(msg_th){
    aviso_th = true;
    msg_th = false;
    bot.sendMessage(CHAT_ID, msg_comfortStatus, "");
    bot.sendMessage(CHAT_ID, String(temp) + "ºC " + String(hum) + "%.", "");
    Serial.println("Aviso TH enviado");
  }

  if(msg_lum){
    aviso_lum = true;
    msg_lum = false;
    bot.sendMessage(CHAT_ID, "Iluminación insuficiente en el aula.", "");
    Serial.println("Aviso LUM enviado");
  }

  if(msg_ruido && !aviso_ruido){
    msg_ruido = false;
    bot.sendMessage(CHAT_ID, "Demasiado ruido en el aula.", "");
    Serial.println("Aviso RUIDO enviado");
  }
}

void wifi(){
  
  if (bluetooth_disconnect)
  {
    disconnect_bluetooth();
  }

  switch (wifi_stage)
  {
    case SCAN_START:
      SerialBT.println("Scanning Wi-Fi networks");
      Serial.println("Scanning Wi-Fi networks");
      scan_wifi_networks();
      SerialBT.println("Please enter the number for your Wi-Fi");
      wifi_stage = SCAN_COMPLETE;
      break;

    case SSID_ENTERED:
      SerialBT.println("Please enter your Wi-Fi password");
      Serial.println("Please enter your Wi-Fi password");
      wifi_stage = WAIT_PASS;
      break;

    case PASS_ENTERED:
      SerialBT.println("Please wait for Wi-Fi connection...");
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
      SerialBT.println("Wi-Fi connection failed");
      Serial.println("Wi-Fi connection failed");
      delay(2000);
      wifi_stage = SCAN_START;
      break;
  }
}

void loop() {

  if(WiFi.status() != WL_CONNECTED) init_wifi();

  else if (WiFi.status() == WL_CONNECTED){
    ruido();
  
    luminosidad();
  
    temp_hum();
  
    telegram();
    
    digitalWrite(PIN_LED_RUIDO, led_ruido);
    digitalWrite(PIN_LED_LUM, led_lum);
    digitalWrite(PIN_LED_TH, led_th);
  }
}
