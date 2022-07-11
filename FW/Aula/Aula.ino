#define T_LEC 20

int lec_max = 0;
int lec_min = 0;
int lec = 0;
unsigned int t = 0;
unsigned int t_2 = 0;
int cnt_ruido = 0;
int aviso_ruido = 0;
int t_delay_ruido = 0;
bool led_ruido = 0;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(2,OUTPUT);
}

void loop() {

  t = millis();
  
  while (millis() - t < T_LEC){
    
    lec = analogRead(34);

    if(lec_min == 0) lec_min = lec;
    if(lec_max < lec) lec_max = lec;
    if(lec_min > lec) lec_min = lec;
  }

  if((lec_max - lec_min) >= 25) cnt_ruido++;
  else if((lec_max - lec_min) < 25 && (aviso_ruido == 1 || aviso_ruido == 2)) cnt_ruido--;

  if(cnt_ruido <= 0) cnt_ruido = 0;

  if(cnt_ruido < 200) aviso_ruido = 0;
  if(cnt_ruido >= 200) aviso_ruido = 1;
  if(cnt_ruido >= 600) aviso_ruido = 2;
  if(cnt_ruido >= 900) aviso_ruido = 3;
  
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

    Serial.println(t_2);
    
    if(millis() - t_2 >= t_delay_ruido || t == 0){
      t_2 = millis();
      led_ruido = !led_ruido;
    }
  }

  digitalWrite(2, led_ruido);

  lec_max = 0;
  lec_min = 0;
}
