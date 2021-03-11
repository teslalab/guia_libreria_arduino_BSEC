//Libreria para usar las propiedades de la pantalla OLED
#include <Adafruit_FeatherOLED.h>
Adafruit_FeatherOLED oled = Adafruit_FeatherOLED();
// Libreria para usar las propiedades de conexión WiFi
#include <WiFi.h>
#include <PubSubClient.h> //Libreria para publicación y recepción de datos.
#include <Wire.h>         //Conexión de dispositivos I2C
//#include "bsec.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

// Variables para conexion NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Credenciales para poder conectarnos a la red-WiFi sustituya dentro de las comillas
const char *ssid = "Rigby.";          // Nombre del SSID
const char *password = "PanConPollo"; // Contraseña
                                      //Modificar al nombre que se asigne en el dashboard.
#define TEAM_NAME "ioht/gabriel/001"  //  proyecto/usuario/no.estacion
/*
  ioht/ugal/001
  ioht/oscar/001
  ioht/isidro/001
  ioht/gabriel/001
  */
// Credenciales para GALioT
#define USERNAME "aquality"
#define PASSWORD "$Air333"

const char *clientID = "ioht_gabriel_1";
const char *user = "aquality";
const char *passwd = "$Air333";

//Nombre del Servidor MQTT
const char *mqtt_server = "galiot.galileo.edu";

// Definiciones para el uso de la libreria BSEC de Bosch
#include "bsec.h"
void checkIaqSensorStatus(void);
void errLeds(void);
Bsec bme;
String output, output2;

//Mediciones disponibles para BME680
double temp, hume, pres = 0;
double aqi, sAQI, AQIa = 0;
double CO2e, VOCe, gas, rssi = 0;

char msg[50];
char msg1[50];
char cnt[50];
char msg_r[50];
char topic_name[250];

// network variables
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void setup()
{
  Serial.begin(115200);

  //Setup para BME680
  Wire.begin(21, 22); // Pines I2C del ESP32
  bme.begin(0x77, Wire);
  Serial.println("Conectando a WiFi");
  setupWiFi();
  Serial.println("Activando BME");
  delay(1000);
  bmeSetup();
  //Setup para MQTT
  Serial.println("Activando MQTT");
  setupMQTT();
  timeClient.begin();
  timeClient.setTimeOffset(-21600);
  oled.init();
  oled.clearDisplay();
}

boolean publicar_flag = true;

void loop()
{
  oled.clearDisplay();

  //Realiza mediciones de datos para mejorar sAQI Accuracy
  preHeatSensor();

  /*
  if (!mqtt_client.connected())
  {
    reconnect();
    delay(2000);
  }
  */
  mqtt_client.loop();
  timeClient.update(); // Hace update en la hora

  if (timeClient.getSeconds() == 15)
  {
    if (mqtt_client.connect(clientID, user, passwd))
    {
      Serial.println("Client connected to mqtt");
    }
    String str69 = "Estacion en línea";
    str69.toCharArray(msg, 50);
    mqtt_client.publish(getTopic("Online"), msg);
  }

  if ((timeClient.getMinutes() % 15 == 00) && (timeClient.getSeconds() == 00) && publicar_flag)
  {
    publicarDatos();
    Serial.print("Datos publicados en MQTT Server: ");
    publicar_flag = false;
  }
  else if (timeClient.getMinutes() % 15 != 00)
  {
    publicar_flag = true;
  }

  /*
  if(((timeClient.getMinutes() > 20) && (timeClient.getMinutes() < 31)) || ((timeClient.getMinutes() > 44) )) {
    preHeatSensor();
  }  //Se debe encender el sensor 10 min antes de la toma
*/
  if (timeClient.getSeconds() == 01)
  {
    datosA1();
  }
  if (timeClient.getSeconds() == 21)
  {
    datosA2();
  }
  if (timeClient.getSeconds() == 41)
  {
    datosA3();
  }
  if (timeClient.getSeconds() == 51)
  {
    datosA4();
  }

  delay(200);
}

void datosA1()
{
  bmeData();
  oled.setCursor(50, 0);
  oled.clearDisplay();
  oled.setTextSize(1);
  String hora = String(timeClient.getHours());
  String minutos = String(timeClient.getFormattedTime().substring(2, 5));

  oled.println(hora + minutos);
  oled.setCursor(0, 9);
  oled.print("Temperatura: ");
  oled.println(bme.temperature);

  oled.print("Humedad: ");
  oled.println(bme.humidity);

  oled.print("sAQI: ");
  oled.println(bme.staticIaq);

  oled.display();
}

void datosA2()
{
  bmeData();
  oled.setCursor(50, 0);
  oled.clearDisplay();
  oled.setTextSize(1);
  String hora = String(timeClient.getHours());
  String minutos = String(timeClient.getFormattedTime().substring(2, 5));

  oled.println(hora + minutos);
  oled.setCursor(0, 9);

  oled.print("CO2e: ");
  oled.println(bme.co2Equivalent);

  oled.print("VOCe: ");
  oled.println(bme.breathVocEquivalent);

  oled.print("GAS: ");
  oled.println(bme.gasResistance);

  oled.display();
}

void datosA3()
{
  bmeData();
  oled.setCursor(50, 0);
  oled.clearDisplay();
  oled.setTextSize(1);
  String hora = String(timeClient.getHours());
  String minutos = String(timeClient.getFormattedTime().substring(2, 5));
  oled.println(hora + minutos);
  oled.setCursor(0, 9);

  oled.print("AQI: ");
  oled.println(bme.iaq);

  oled.print("Accuracy: ");
  oled.println(bme.iaqAccuracy);

  oled.print("Presion: ");
  oled.println(bme.pressure);
  oled.display();
}

void datosA4()
{
  //bmeData();
  oled.setCursor(50, 0);
  oled.clearDisplay();
  oled.setTextSize(1);
  String hora = String(timeClient.getHours());
  String minutos = String(timeClient.getFormattedTime().substring(2, 5));
  oled.println(hora + minutos);
  oled.setCursor(0, 9);
  oled.println("WiFi Status");
  if (WiFi.status() == WL_CONNECTED)
  {
    oled.println("WiFi connected to: ");
    oled.println(ssid);
  }
  else
  {
    oled.println("WiFi not connected");
  }
  oled.display();
}

void bmeData()
{
  if (bme.run())
  { // If new data is available
    output += ", " + String(bme.rawTemperature);
    output += ", " + String(bme.pressure);
    output += ", " + String(bme.rawHumidity);
    output += ", " + String(bme.gasResistance);
    output += ", " + String(bme.iaq);
    output += ", " + String(bme.iaqAccuracy);
    output += ", " + String(bme.temperature);
    output += ", " + String(bme.humidity);
    output += ", " + String(bme.staticIaq);
    output += ", " + String(bme.co2Equivalent);
    output += ", " + String(bme.breathVocEquivalent); //Serial.println(output);
  }
  else
  {
    checkIaqSensorStatus();
  }
}

/* Esta funcion nos muestra el estado
del sensor BME680*/
void checkIaqSensorStatus(void)
{
  if (bme.status != BSEC_OK)
  {
    if (bme.status < BSEC_OK)
    {
      output = "BSEC error code : " + String(bme.status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    }
    else
    {
      output = "BSEC warning code : " + String(bme.status);
      Serial.println(output);
    }
  }

  if (bme.bme680Status != BME680_OK)
  {
    if (bme.bme680Status < BME680_OK)
    {
      output = "BME680 error code : " + String(bme.bme680Status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    }
    else
    {
      output = "BME680 warning code : " + String(bme.bme680Status);
      Serial.println(output);
    }
  }
}

/* Esta funcion nos muestra posibles 
errores en el sensor BME680 NO MODIFICAR,
CONFIGURADA PARA IoHT*/

void errLeds(void)
{ //Función para indicar que hay error en sensor con neopixeles
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  delay(100);
  digitalWrite(2, LOW);
  delay(100);
}

void bmeSetup()
{

  output = "\n Versión de la libreria BSEC" + String(bme.version.major) + "." + String(bme.version.minor) + "." + String(bme.version.major_bugfix) + "." + String(bme.version.minor_bugfix);
  Serial.println(output); // Informacion del sensor y la libreria
  checkIaqSensorStatus();
  bsec_virtual_sensor_t sensorList[10] = {
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_STATIC_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  bme.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  // Print the header
  output = "Timestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], Static IAQ, CO2 equivalent, breath VOC equivalent";
  Serial.println(output);
}

void setupWiFi()
{
  //Inicializamos WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  //Fin de la inicialización WiFi
}

void setupMQTT()
{
  delay(10);
  // Iniciamos la conexion WiFi con la Red que colocamos
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    msg_r[i] = (char)payload[i];
  }
  msg_r[length] = 0;
  Serial.print("'");
  Serial.print(msg_r);
  Serial.println("'");
}
void reconnect()
{
  // Loop until we're reconnected
  while (!mqtt_client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect

    if (mqtt_client.connect(clientID, user, passwd))
    {
      Serial.println("Client connected in reconnection"); //Testing
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publish(char *topic, char *payload)
{
  mqtt_client.publish(topic, payload);
}

char *getTopic(char *topic)
{
  sprintf(topic_name, "%s/%s", TEAM_NAME, topic);
  Serial.println(topic_name); //Revisando el topic que se está mandando
  return topic_name;
}

void preHeatSensor()
{
  unsigned long time_trigger = millis();
  if (bme.run())
  { // If new data is available
    output2 = String(time_trigger);

    output2 = String(time_trigger);
    output2 += ", " + String(bme.rawTemperature);
    output2 += ", " + String(bme.pressure);
    output2 += ", " + String(bme.rawHumidity);
    output2 += ", " + String(bme.gasResistance);
    output2 += ", " + String(bme.iaq);
    output2 += ", " + String(bme.iaqAccuracy);
    output2 += ", " + String(bme.temperature);
    output2 += ", " + String(bme.humidity);
    output2 += ", " + String(bme.staticIaq);
    output2 += ", " + String(bme.co2Equivalent);
    output2 += ", " + String(bme.breathVocEquivalent);
    Serial.println(output2);
  }
  else
  {
    checkIaqSensorStatus();
  }
}

void publicarDatos()
{
  String faltantes = "Datos no enviados\n: \n";
  String enviados = "";
  if (mqtt_client.connect(clientID, user, passwd))
  {
    Serial.println("Cliente conectado a MQTT Server");
    //************ Posteamos la temperatura ************
    temp = bme.temperature;
    Serial.println("Temperatura : " + String(temp));
    String(temp).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("temp"), msg)) ? enviados += "Temperatura enviada\n" : faltantes += "Temperatura\n";

    //************ Posteamos la humedad ************
    hume = bme.humidity;
    Serial.println("Humedad : " + String(hume));
    String(hume).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("hume"), msg)) ? enviados += "Humedad enviada\n" : faltantes += "Humedad\n";

    //************ Posteamos la Presion Atmosferica ************
    pres = bme.pressure;
    Serial.println("Presion Atmosferica : " + String(pres));
    String(pres).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("pres"), msg)) ? enviados += "Presion Atmosferica  enviada\n" : faltantes += "Presion Atmosferica \n";

    //************ Posteamos el Index Air Quality ************
    aqi = bme.iaq;
    Serial.println("Air Quality Index: " + String(aqi));
    String(aqi).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("aqi"), msg)) ? enviados += "Air Quality Index enviado\n" : faltantes += "Air Quality Index\n";

    //************ Posteamos el Static Index Air Quality ************
    sAQI = bme.staticIaq;
    Serial.println("Static Air Quality Index: " + String(sAQI));
    String(sAQI).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("sAQI"), msg)) ? enviados += "Static Air Quality Index enviado\n" : faltantes += "Index Air Quality\n";

    //************ Posteamos el Index Air Quality Accurary ************
    AQIa = bme.iaqAccuracy;
    Serial.println("Index Air Quality Accuracy : " + String(AQIa));
    String(AQIa).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("AQIa"), msg)) ? enviados += "Index Air Quality Accuracy enviado\n" : faltantes += "Index Air Quality Accuracy\n";

    //************ Posteamos el Gas Resistence ************
    gas = (bme.gasResistance) / 1000;
    Serial.println("Gas Resistance kOhms: " + String(gas));
    String(gas).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("gas"), msg)) ? enviados += "Gas Resistance kOhms enviado\n" : faltantes += "Gas Resistance kOhms\n";

    //************ Posteamos el CO2 Equivalente ************
    CO2e = bme.co2Equivalent;
    Serial.println("CO2 Equivalente : " + String(CO2e));
    String(CO2e).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("CO2e"), msg)) ? enviados += "CO2 Equivalente enviado\n" : faltantes += "CO2 Equivalente\n";

    //************ Posteamos el VOC Equivalente ************
    VOCe = bme.breathVocEquivalent;
    Serial.println("VOC Equivalente : " + String(VOCe));
    String(VOCe).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("VOCe"), msg)) ? enviados += "VOC Equivalente enviado\n" : faltantes += "VOC Equivalente \n";

    //************ Posteamos la intensidad de señal ************
    rssi = WiFi.RSSI();
    Serial.println("Intensidad de Señal : " + String(rssi));
    String(rssi).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("rssi"), msg)) ? enviados += "Intensidad de Señal enviado\n" : faltantes += "Intensidad de Señal\n";
    
    
    Serial.println(enviados);

    if (faltantes != "Datos no enviados\n: \n")
    {
      Serial.println(faltantes);
      Serial.println("Estado de la conexión MQTT Server: ");
      Serial.println(mqtt_client.state());
    }
    
  }
  else
  {
    Serial.println("Cliente NO conectado a MQTT Server");
    Serial.print("Estado del error de conexión: ");
    Serial.println(mqtt_client.state());
  }
}