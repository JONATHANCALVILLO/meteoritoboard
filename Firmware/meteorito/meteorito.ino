/*******************************************************
Meteorito.ino
Ejemplo de Estacion Meteorologica con ESP8266 
para redmet.org y Meteorito
Andres Sabas @ The Inventor's House
Fecha Original de Creación: 28 de Octubre del 2017

Este ejemplo demuestra la conexion y envio de datos 
con un modulo ESP32 a la plataforma 
http://redmet.org

Entorno de Desarrollo Especifico:
  IDE: Arduino 1.8.4
  Plataforma de Hardware:
    - ESP32 WEMOS D1 Mini
    - DHT22
    - VEML6070
    - Fotoresistencia
    - BMP180
  o Tarjeta de desarrollo Meteorito por Electronic Cats

Este código es beerware; si me ves 
(o cualquier otro miembreo de The Inventors House) 
por tu localidad, y ha encontrado útil nuestro código,
¡por favor cómprenos una ronda!

Distribuido tal cual; no se otorga ninguna garantía. 
Bajo Licencia MIT
************************************************************/
//Incluir la biblioteca WiFi
#include <WiFi.h>
#include "configuracion.h"
#include  <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>

Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

sensors_event_t event;

int sensor = 16;
float temperatura;
float humedad;

/*Variables Anemometro*/
const int pinAnemometro = 27;
unsigned long tiempoAntes;
unsigned long  tiempo=0;
unsigned long sumaTiempo=0;
byte contador=0;
bool bandera=0;

/*Variables uv*/
const byte pinRayosUV = 12;         //pin Analogico

/*Variables Nubosidad*/
const byte pinNubosidad = A0;

/*Variables Direccion de Viento*/
int sumaVeleta=0;      
const byte pinDireccion = 14;       //pin Analógico 
int direccion = 0;
const int tiempoEnvio=180;

//variables manejo de proceso precipitacion
float precipitacion = 0;
const byte pinPluviometro = 13;  //pin digital
unsigned long tiempoAntesDos;
unsigned long  tiempoDos=0;
unsigned long sumaTiempoDos=0;
byte contadorDos=0;
const int capacidadTotal=10;   //capacidad combinada de ambos lados en mL


DHT dht (sensor,DHT22);

const char tipoNubosidad[6]={'C','M','N','P','D','O'};
  /* D - despejado
   * P - poco nuboso
   * N - nuboso
   * M - muy nuboso
   * C - cubierto
   * O - 
   */

//Formamos el header para enviar a la pagina
String httpHeader = "POST /api/device/metrics HTTP/1.1\r\n"
                    "Host: redmet.org\r\n" 
                    "Content-Type: application/json\r\n"
                    "Authorization: Basic "+ Token +"\r\n";

//Inicializar el WiFi cliente objeto
WiFiClient client;

/*Funcion para obtener direccion del viento */
/*
int leerDireccion(){
  suma=suma/tiempoEnvio;
  if(suma>=415 && suma< 440) return 0;
  if(suma>=440 && suma< 490) return 45;
  if(suma>=490 && suma< 510) return 90;
  if(suma>=540 && suma< 550) return 135;
  if(suma>=510 && suma< 525) return 18;
  if(suma>=525 && suma< 540) return 225;
  if(suma>=590 && suma< 615) return 270;
  if(suma>=615 && suma< 620) return 315;
}
*/
/*Funcion para obtener la luz ultravioleta*/
int leerUV(){
  int uv =map(analogRead(pinRayosUV),0,4095,0,15);
  return uv;
}

/*Funcion para obtener nubosidad*/
char nubosidad() {
  int lecturaSensor=analogRead(pinNubosidad);
  char nubosidad = tipoNubosidad[map(lecturaSensor, 0, 1023, 0, 6)];
  Serial.print("Nubosidad: "); 
  Serial.println(nubosidad); 
  return nubosidad;
}

/*Funcion para obtener presion y altura*/
void presion(){
  /* Muestra los resultados (la presión barométrica se mide en hPa) */
  if (event.pressure)
  {
    /* Display atmospheric pressue in hPa */
    Serial.print("Presion:    ");
    Serial.print(event.pressure*0.1);
    Serial.println(" hPa");
    
    /* Calculating altitude with reasonable accuracy requires pressure    *
     * sea level pressure for your position at the moment the data is     *
     * converted, as well as the ambient temperature in degress           *
     * celcius.  If you don't have these values, a 'generic' value of     *
     * 1013.25 hPa can be used (defined as SENSORS_PRESSURE_SEALEVELHPA   *
     * in sensors.h), but this isn't ideal and will give variable         *
     * results from one day to the next.                                  *
     *                                                                    *
     * You can usually find the current SLP value by looking at weather   *
     * websites or from environmental information centers near any major  *
     * airport.                                                           *
     *                                                                    *
     * For example, for Paris, France you can check the current mean      *
     * pressure and sea level at: http://bit.ly/16Au8ol                   */
     
    /* Primero obtenemos la temperatura actual del BMP085 */
    float temperature;
    bmp.getTemperature(&temperature);
    Serial.print("Temperatura BMP180: ");
    Serial.print(temperature);
    Serial.println(" C");

    /* Luego convierte la presión atmosférica, y SLP a la altitud        */
    /* Actualice esta próxima línea con el SLP actual para obtener mejores resultados     */
    float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
    Serial.print("Altitud:    "); 
    Serial.print(bmp.pressureToAltitude(seaLevelPressure,
                                        event.pressure)); 
    Serial.println(" m");
    Serial.println("");
  }
  else
  {
    Serial.println("Sensor error");
  }
}

/*
 *Función de envio de datos 
 */
static void envioDatos () {
  if (client.connect(Servidor, 80) <= 0)
  {
    Serial.println(F("Fallo conexion a servidor."));
    return;
  }
  Serial.println(F("Connected."));

  //Asignar parametros a enviar:
         /*clouds, humidity, pressure, rain, temp, uv, windDirection, windSpeed*/
  String clouds, humidity, pressure, rain, temp, indiceUV, windDirection, windSpeed;
  
  clouds = String(nubosidad());
  humidity = String(humedad);
  pressure = String(event.pressure*0.1);
  rain = String(random(0,250));
  temp = String(temperatura);
  indiceUV = String(leerUV());
  windDirection = String(random(0,360));
  windSpeed = String(random(0,360));

//cargamos una cadena con los datos
  /*El formato es "{\"data\":{\"metrica\":valor}}"*/
  
  //Ejemplo con un solo valor constante
  //String dato="{\"data\":{\"temp\":15}}";

  //Ejemplo con todos los valores posibles constantes
  //String dato="{\"data\":{\"clouds\":\"D\",\"humidity\":95,\"pressure\":145,\"rain\":245,\"temp\":15,\"uv\":13,\"windDirection\":9,\"windSpeed\":340}}";
  
  //Lectura de todos los valores posibles
  String dato="{\"data\":{\"clouds\":\""+clouds+"\",\"humidity\":\""+humidity+"\",\"pressure\":\""+pressure+"\",\"rain\":\""+rain+"\",\"temp\":\""+temp+"\",\"uv\":\""+indiceUV+"\",\"windDirection\":\""+windDirection+"\",\"windSpeed\":\""+windSpeed+"\"}}";
  
  Serial.println(F("Enviando datos!"));
  Serial.println(dato);

  client.print(httpHeader);
  client.print("Content-Length: "); 
  client.println(dato.length());
  client.println();
  client.println(dato);

  unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    // Read all the lines of the reply from server and print them to Serial
    while(client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }

/*
  // available() devolverá el número de caracteres
  // actualmente en el búfer de recepción.
  while (client.available())
    Serial.write(client.read()); // read() obtiene el carácter de FIFO

  // connected () es un valor de retorno booleano - 1 si
  // la conexión está activa, 0 si está cerrada.
  if (client.connected())
    client.stop(); // stop() cierra una conexión TCP.
    */
}

void setup () {
  Serial.begin(9600);
  
  Serial.println("Iniciando Estacion Meteorito");
  Serial.println("por Electronic Cats");
 
  dht.begin();

  /* Initializar el sensor BMP180 */
  if(!bmp.begin())
  {
    /* Hubo un problema al detectar el BMP085 ... verifique sus conexiones */
    Serial.print("Ooops, no detectado BMP085 ... Checar tu cableado o I2C Direccion!");
    while(1);
  }

//verificación del modulo WiFi y la conexión a internet
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }
//Tu estas conectado ahora
  Serial.println("Tu estas conectado a la red WiFi");
  
  printWifiStatus();

  //Iniciamos anemometro
  pinMode(pinAnemometro, INPUT);
  //attachInterrupt(digitalPinToInterrupt(pinAnemometro), interrupcionViento,RISING );
  tiempoAntes=millis();

  //Iniciamos pluviometro
   pinMode(pinPluviometro, INPUT);
   //attachInterrupt(digitalPinToInterrupt(pinPluviometro), interrupcionPrecipitacion,RISING );
   tiempoAntesDos=millis();
}

void loop () {
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();
   
  /* Obtener una nueva lectura del sensor BMP180 */ 
  bmp.getEvent(&event);
  
  presion();
  
  envioDatos();

    Serial.println("");
   //Mostrar variables
   Serial.print("temperatura: ");
   Serial.println(temperatura);
   Serial.print(" humedad: ");
   Serial.println(humedad);
   Serial.print("UV nivel luz: "); 
   Serial.println(leerUV());
   Serial.print("Direccion del viento: "); 
//   Serial.println(leerDireccion());
   delay(500);
}

/*
 * Funcion que imprime status de WiFi, 
 * nombre de red, local IP e intensidad de señal
*/
void printWifiStatus()
{
  // imprime el SSID de la red a la que está conectado
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // imprime la dirección IP de tu estacion WiFi
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // imprime la intensidad de la señal recibida
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void interrupcionViento() {
  if( millis()>(50+tiempoAntes)){
    bandera=!bandera;
    if(bandera==0){
      tiempo=(millis()-tiempoAntes);
      tiempoAntes=millis();
      sumaTiempo+=tiempo; 
      if(contador<=19){
        contador++;
        Serial.println(contador);
      }else{
        contador=0;
        float velocidad=(2*3.1416*0.05*3.6)/((sumaTiempo/1000.0)/20);
        Serial.print(velocidad);
        Serial.println("  Km/h");
        sumaTiempo=0;
      }
    }
  }
}

//interrupcion para precipitación
void interrupcionPrecipitacion() {
  if( millis()>(50+tiempoAntesDos)){
      tiempoDos=(millis()-tiempoAntesDos);
      tiempoAntesDos=millis();
      sumaTiempoDos+=tiempoDos; 
      if(contadorDos<=19){
        contadorDos++;
      }else{
        precipitacion=contadorDos*(((capacidadTotal*10)/(42.84))/(sumaTiempoDos/1000.0));
        contadorDos=0;
        sumaTiempoDos=0;
      }
  }
}
