
#define PIN_BOMBA 26
#define PIN_BOYA 27
#define PIN_HUM_TIERRA 33

bool bomba = false;
bool boya = false;
int humedad_tierra = 0;
int humedad_tierra_100 = 0;
int hum_min = 30;
int hum_max = 80;
unsigned long tiempo = 0;

void setup()
{
  Serial.begin(9600);
  
  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_BOYA, INPUT);
  
}

void loop()
{
    
    boya = digitalRead(PIN_BOYA);
    humedad_tierra = analogRead(PIN_HUM_TIERRA);
    humedad_tierra_100 = map(humedad_tierra, 350, 3800, 0, 100);
    
    if(humedad_tierra_100 > 100) humedad_tierra_100 = 100;
    if(humedad_tierra_100 < 0) humedad_tierra_100 = 0;
    
    if(boya == false){
      bomba = false;
    }
  
    else if(boya == true && humedad_tierra_100 < hum_min && bomba == false) {
      bomba = true;
    }
  
    else if(boya == true && humedad_tierra_100 > hum_max){
      bomba = false;
    }

    Serial.print("Humedad : ");
    Serial.println(humedad_tierra);
    Serial.print("Humedad % : ");
    Serial.println(humedad_tierra_100);
    Serial.print("Boya : ");
    Serial.println(boya);
    Serial.print("Bomba : ");
    Serial.println(bomba);
    
    digitalWrite(PIN_BOMBA, bomba);
}
