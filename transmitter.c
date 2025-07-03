
// DHT
#include <DHT.h>
#include <DHT_U.h>

// NRF24
#include <nRF24L01.h>
#include <RF24_config.h>
#include <printf.h>
#include <RF24.h>
#include <SPI.h>

// DHT
#define DHTPIN 4        // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);

// NRF24
#define pinCE   8             
#define pinCSN  7             
RF24 radio(pinCE, pinCSN); 
const byte address[6] = "PIPE1";

void setup() {
  // DHT
  Serial.begin(9600);
  dht.begin();

  // NRF24 CONFIG
  radio.begin();  
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.stopListening();  
}

void loop() {

  int soil_humidity = analogRead(A1);
  //int soil_humidity = dht.readHumidity();
  float air_humidity = dht.readHumidity();
  float air_temperature = dht.readTemperature();
  

  if (isnan(air_humidity) || isnan(air_temperature)) {
    Serial.println("Failed to sensors!");
    return;
  }

  struct SensorData {
    float air_temperature;
    float air_humidity;
    int16_t soil_humidity;
  };

  SensorData dataToSend;

  dataToSend.air_temperature = air_temperature;
  dataToSend.air_humidity = air_humidity;
  dataToSend.soil_humidity = soil_humidity;

  radio.write(&dataToSend, sizeof(dataToSend));

  Serial.print("Sent: ");
  Serial.println(dataToSend.air_temperature);
  Serial.println(dataToSend.air_humidity);
  Serial.println(dataToSend.soil_humidity);


  delay(10000);
}
