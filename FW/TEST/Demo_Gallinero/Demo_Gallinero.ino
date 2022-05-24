#include <ctype.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>
#include <Preferences.h>

// Wifi network station credentials
#define WIFI_SSID "XXXXXXX"
#define WIFI_PASSWORD "XXXXXXXXXXXXXX"

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "XXXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" 
#define CHAT_ID "XXXXXXXXXXXX"

#define PIN_LED_1 2
#define PIN_LED_2 15
#define PIN_PULSADOR_1 16
#define PIN_PULSADOR_2 17
#define PIN_AGUA 26
#define PIN_PIENSO 27
#define PIN_SERVO 12
#define PIN_IR_CUBIERTA 13

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

const unsigned long BOT_MTBS = 3000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

int servoPin = 14;

int ledStatus = 0;
int estado = ABIERTA;
int estado_ant = ABIERTA;

long int crono_puerta = 0;
long int crono_puerta_auto = 0;

unsigned long int t_puerta_demo = 60000;//318000;//300000; //5 min
unsigned long int crono_puerta_demo = 0;

int huevos;

bool agua = 0;
bool pienso = 0;
bool cajon = 0;
bool mensaje = 0;
bool cajon_huevos = 0;
bool rst = 0;

String chat_id_g;

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
bool state_cajon = false;
bool state_agua = false;
bool state_pienso = false;
bool state_cajon_last = false;
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

int state = CONFIG_SET_UP;

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
  
  else if (accion == CERRAR && servo_ant == MANTENER && !movimiento) {
    Serial.println("Cerrando...");
    crono_puerta = millis();
    movimiento = true;
    myservo.write(CERRAR);
  }
  
  else if (accion == MANTENER && movimiento) {
    Serial.println("Voy a mantener el estado actual");
    myservo.write(MANTENER);
    movimiento = false;
    enviar = true;
  }
}

void handleNewMessages(int numNewMessages){
  
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++){

    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    chat_id_g = chat_id;

    Serial.println(cajon_huevos);
    Serial.print("text: "); Serial.print(text); Serial.print(" -> "); Serial.print(text.toInt()); Serial.print(" -> "); Serial.print(isDigit(text.toInt()));

    if (from_name == "")
      from_name = "Guest";

    if (estado == CERRADA && text == "Abrir")
    {
      Serial.println("Abrir recibido");
      mensaje = 1;
      servo = ABRIR;
      text = "";
    }

    else if (estado == ABIERTA && text == "Cerrar")
    {
      Serial.println("Cerrar recibido");
      mensaje = 1;
      servo = CERRAR;
      text = "";
    }

    else if (cajon_huevos) {
      Serial.println("Ha entrado");
      huevos = preferences.getInt("huevos", 0);
      huevos += text.toInt();
      preferences.putInt("huevos", huevos);
      cajon_huevos = 0;
      Serial.print("Huevos: "); Serial.println(huevos);
      bot.sendMessage(String(CHAT_ID), "Llevas " + String(huevos) + " huevos recogidos.", "");
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
  state_cajon = digitalRead(PIN_IR_CUBIERTA);

  // Solo leo el estado del agua y el pienso si el cajón está cerrado.
  if (state_cajon) state_agua = digitalRead(27);
  if (state_cajon) state_pienso = !digitalRead(32);

  //Serial.print(state_cajon);Serial.print(" ");Serial.print(state_agua);Serial.print(" ");Serial.println(state_pienso);
}

void config_puerta() {
  
  while (state != CONFIG_OK) {

    Serial.print(state); Serial.print(" "); Serial.println(servo);

    switch (state) {

      case CONFIG_SET_UP:
      
        t_servo_up = preferences.getUInt("t_up",0);
        t_servo_down = preferences.getUInt("t_down",0);
        
        while (state == CONFIG_SET_UP) {

          Serial.println(state);
          inputs();

          if ((state_p_2 && state_p_2_last)) {
            leds(F_5HZ, OFF);
            servo = ABRIR;
          }

          else {
            leds(ON, OFF);
            servo = MANTENER;
          }
          Serial.print(state_p_1); Serial.println(state_p_1_last);
          if (state_p_1 && !state_p_1_last) state = CONFIG_DOOR_DOWN;

          outputs();
        }
        break;

      case CONFIG_DOOR_DOWN:

        while (state == CONFIG_DOOR_DOWN) {

          Serial.println(state);

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

          Serial.println(state);

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
          estado = CERRAR;
          outputs();
        }
        break;

      case CONFIG_DOOR_TEST_UP:

        t_servo_crono = millis();

        while (state == CONFIG_DOOR_TEST_UP) {

          Serial.println(state);

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

          Serial.println(state);

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

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

  if (state_pienso && !state_pienso_last) {
    bot.sendMessage(String(CHAT_ID), "Queda poco pienso.", "");
    Serial.println("Queda poco pienso");
  }
  else if (!state_pienso && state_pienso_last) {
    bot.sendMessage(String(CHAT_ID), "Se ha rellenado el pienso.", "");
    Serial.println("Se ha rellenado el pienso");
  }

  if (state_agua && !state_agua_last) {
    bot.sendMessage(String(CHAT_ID), "Queda poca agua.", "");
    Serial.println("Queda poca agua");
  }

  else if (!state_agua && state_agua_last) {
    bot.sendMessage(String(CHAT_ID), "Se ha rellenado el agua", "");
    Serial.println("Se ha rellenado el agua");
  }

  if (state_cajon && !state_cajon_last) {
    cajon_huevos = 1;
    bot.sendMessage(String(CHAT_ID), "Has abierto el cajón ¿Cuantos huevos has recogido?", "");
    //Añadir aviso cubierta abierta.
    //crono_cajon = millis();
    Serial.println("cajon abierto");
    cajon = 0;
  }

  Serial.print(mensaje);Serial.print(" ");Serial.print(estado);Serial.print(" ");Serial.print(enviar);Serial.print(" ");Serial.println(!movimiento);
  if (mensaje == 1 && estado == ABIERTA && enviar && !movimiento) {
      Serial.println("3");
      Serial.println();
      mensaje = 0;
      enviar = false;
      bot.sendMessage(String(CHAT_ID), "Puerta abierta", "");
    }

    else if (mensaje == 1 && estado == CERRADA && enviar && !movimiento){
      Serial.println("5");
      Serial.println();
      mensaje = false;
      enviar = false;
      bot.sendMessage(String(CHAT_ID), "Puerta cerrada", "");
    }
    
  /* Falta perfeccionar
    if(!state_cajon && state_cajon_last) crono_cajon = millis();

    if(millis()-crono_cajon > t_alarma_cajon && !state_cajon){
    bot.sendMessage(String(CHAT_ID), "Te has dejado el cajón abierto.", "");
    crono_cajon = millis();
    }
  */
}

void setup()
{
  preferences.begin("voluta", false);

  Serial.begin(115200);
  Serial.println();
  
  pinMode(26, INPUT);
  pinMode(27, INPUT);
  pinMode(32, INPUT_PULLUP);
  pinMode(2, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(PIN_PULSADOR_1, INPUT);
  pinMode(PIN_PULSADOR_2, INPUT);

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }

  Serial.println(now);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);    // standard 50 hz servo
  myservo.attach(servoPin, 1000, 2000);


 // attachInterrupt(26, alarma_cajon, RISING);

  
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
}

void loop()
{

  if (WiFi.status() == WL_CONNECTED) digitalWrite(15, HIGH);

  else if (WiFi.status() != WL_CONNECTED && !movimiento && servo_ant == MANTENER && servo == MANTENER ) {

    digitalWrite(15, LOW);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
    while (WiFi.status() != WL_CONNECTED){
      delay(200);
    }

    configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
    time_t now = time(nullptr);
    
    while (now < 24 * 3600)
    {
      delay(100);
      now = time(nullptr);
    }
  }

  if(!movimiento) inputs();

  if (state_p_1 && state_p_2 && !movimiento) {
    preferences.putInt("huevos", 0);
    preferences.putUInt("t_up", 0);
    preferences.putUInt("t_down", 0);
    state = 0;
    config_puerta();
  }
  
  gestion_puerta();
  
  if(!movimiento) gestion_bot();
  
  outputs();
}
