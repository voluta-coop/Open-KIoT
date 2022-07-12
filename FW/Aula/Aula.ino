#define PIN_DHT       33
#define PIN_MICRO     35
#define PIN_LDR       14
#define PIN_LED_RUIDO 2
#define PIN_LED_LUM   15
#define PIN_LED_TH    17
#define T_LEC_RUIDO   20

// Varialbes ruido
int lec_max = 0;
int lec_min = 0;
int lec = 0;
unsigned int t = 0;
unsigned int t_2 = 0;
int cnt_ruido = 0;
int aviso_ruido = 0;
int t_delay_ruido = 0;
bool led_ruido = 0;

// Variables luminosidad.
bool led_lum = LOW;

// Variables temperatura y humedad.
bool led_th = LOW;

void ruido(){
  t = millis();
  
  while (millis() - t < T_LEC_RUIDO){
    
    lec = analogRead(PIN_MICRO);
    
    if(lec_min == 0) lec_min = lec;
    if(lec_max < lec) lec_max = lec;
    if(lec_min > lec) lec_min = lec;

  }

  if((lec_max - lec_min) >= 27){
    cnt_ruido = cnt_ruido + (lec_max - lec_min)/5;
  }
  
  else if((lec_max - lec_min) < 27 && aviso_ruido != 3){
    cnt_ruido = cnt_ruido - (lec_max - lec_min)/4;
  }

  if(cnt_ruido <= 0) cnt_ruido = 0;

  if(cnt_ruido < 200) aviso_ruido = 0;
  if(cnt_ruido >= 200) aviso_ruido = 1;
  if(cnt_ruido >= 700) aviso_ruido = 2;
  if(cnt_ruido >= 1600) aviso_ruido = 3;

  Serial.print(lec_max - lec_min);
  Serial.print(" ");
  Serial.println(cnt_ruido);
  
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

void luminosidad(){
  
}

void temp_hum(){
  
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(PIN_LED_RUIDO, OUTPUT);
  pinMode(PIN_LED_LUM, OUTPUT);
  pinMode(PIN_LED_TH, OUTPUT);
}

void loop() {

  ruido();

  luminosidad();

  temp_hum();
  
  
  digitalWrite(PIN_LED_RUIDO, led_ruido);
  digitalWrite(PIN_LED_LUM, led_lum);
  digitalWrite(PIN_LED_TH, led_th);
}
