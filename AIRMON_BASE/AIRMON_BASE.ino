/*
 *Programa conexión WiFi y envio de datos a la plataforma GALIoT con la tarjeta de MONAIR  
 *Conexión y envio de información 
 *Descripción: En este programa usaremos la tarjeta de MONAIR, conectaremos a la plataforma GALIoT
 *con ello podremos visualizar información de nuestras variables ambientales en los dashboard de MONAIR 
 *cada minuto se publicaran nuestros datos hacia el mqtt broker
 *Programa realizado por Angel Isidro & Gabriel Monzón - 27 julio 2021 - Versión 1 
 */
// Libreria para usar las propiedades de conexión WiFi
#include <WiFi.h>
#include <PubSubClient.h> //Libreria para publicación y recepción de datos.
#include <Wire.h>         //Conexión de dispositivos I2C
#include "bsec.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
// Para DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
// Variables para conexion NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Credenciales para poder conectarnos a la red-WiFi sustituya dentro de las comillas

const char *ssid = "galileo"; //Sustituya NOMBRE_WiFi por el nombre de su red WiFi-
const char *password = "";    //Sustituya Contraseña_WiFi por su contraseña de red WiFi

//credenciales para publicar hacia el mqtt broker
const char *user = "aquality";
const char *passwd = "$Air333";
const char *clientID = "airmon_2";

//Nombre del Servidor MQTT
const char *mqtt_server = "galiot.galileo.edu";
//Modificar al nombre que se asigne en el dashboard.
#define TEAM_NAME "/airmon/S002"

//Declaración de funciones de ayuda
void checkIaqSensorStatus(void);
Bsec iaqSensor;
String output, output2;
// data variables
int lastReadingTime, lastReadingTime2 = 0;
double temp, ds18, hume, pres = 0;
double aqi, sAQI, AQIa = 0;
double CO2e, VOCe, gas, rssi, pm10, pm25 = 0;
char msg[50];
char msg1[50];
char cnt[50];
char msg_r[50];
char topic_name[250];
//************** Funciones para el Sensor DS28B20 **************
const int pinDatosDQ = 18;
OneWire oneWireObjeto(pinDatosDQ);
DallasTemperature sensorDS18B20(&oneWireObjeto);
// network variables
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
//************** Funciones para el Sensor PMSA0031 **************
#include "Adafruit_PM25AQI.h"
Adafruit_PM25AQI particulas = Adafruit_PM25AQI();
PM25_AQI_Data data;

void setup()
{
  Serial.begin(115200);
  //Inicializamos WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //mientras no se logra conectar al wifi se queda esperando conexión
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(5000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  //Fin de la inicialización WiFi
  //Setup para MQTT
  setupMQTT();
  timeClient.begin();
  timeClient.setTimeOffset(-21600);

  //Setup para el BME680
  Wire.begin(21, 22);
  iaqSensor.begin(0x77, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
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

  iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  // Print the header
  output = "Timestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], Static IAQ, CO2 equivalent, breath VOC equivalent";
  Serial.println(output);
  delay(3000);
  /******************* Setup para el sensor PMSA0031 ***********************/
  if (!particulas.begin_I2C())
  { // connect to the sensor over I2C

    Serial.println("Could not find PM 2.5 sensor!");
    ESP.restart();
    while (1)
      delay(10);
  }
  else
  {
    Serial.println("PM25 found!");
  }
}

/*flag que nos permite llevar el control de publicar una sola vez por minuto
-se va a publica solo cuando el flag este en TRUE
-una vez publicado el flag pasa a estar en FALSE
-al pasar el minuto o tiempo establecido se restaura el valor del flag a TRUE para que sea posible volver a publicar
*/
boolean publicar_flag = true;
void loop()
{

  if (!mqtt_client.connected())
  {
    reconnect();
  }
  mqtt_client.loop();

  timeClient.update(); // Hace update en la hora
  //Serial.println(timeClient.getFormattedTime()); // Imprime en el monitor serial  la hora

  if (timeClient.getSeconds() == 15)
  {
    if (mqtt_client.connect(clientID, user, passwd))
    {
      //Serial.println("Client connected to mqtt");
    }
    else
    {
      setupWiFi();
      reconnect();
    }
    String str69 = "Estacion en línea";
    str69.toCharArray(msg, 50);
    mqtt_client.publish(getTopic("Online"), msg);
  }

  //if ((timeClient.getMinutes() % 2 == 00) && (timeClient.getSeconds() == 00) && publicar_flag)
  if ((timeClient.getSeconds() == 00) && publicar_flag)
  {
    Serial.print("2 minutos publicando");
    publicarDatos();
    Serial.print("Datos publicados en MQTT Server: ");
    publicar_flag = false;
  }
  else if (timeClient.getSeconds() != 00)
  {
    publicar_flag = true;
  }

  preHeatSensor();
}

//funcion que se manda a llamar si se pierde la conexion
//si no se logra reconectar luego de cierto numero de intentos el esp hace un soft reset
void setupWiFi()
{
  //Inicializamos WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  delay(2000);
  int cont = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    delay(2000);
    Serial.print(".");
    cont += 1;
    if (cont > 150)
    {
      ESP.restart();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  //Fin de la inicialización WiFi
}

// Helper function definitions
void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK)
  {
    if (iaqSensor.status < BSEC_OK)
    {
      output = "BSEC error code : " + String(iaqSensor.status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    }
    else
    {
      output = "BSEC warning code : " + String(iaqSensor.status);
      Serial.println(output);
    }
  }

  if (iaqSensor.bme680Status != BME680_OK)
  {
    if (iaqSensor.bme680Status < BME680_OK)
    {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    }
    else
    {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
  }
}

void errLeds(void)
{
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  delay(100);
  digitalWrite(2, LOW);
  delay(100);
}

void publish(char *topic, char *payload)
{
  Serial.println(topic_name);
  mqtt_client.publish(topic_name, payload);
}

//por ejemplo: TEAM_NAME/hume
char *getTopic(char *topic)
{
  sprintf(topic_name, "%s/%s", TEAM_NAME, topic);
  return topic_name;
}

void reconnect()
{
  // Loop until we're reconnected
  int cont = 1;
  while (!mqtt_client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    Serial.println("connected");

    if (mqtt_client.connect(TEAM_NAME))
    {
      //mqtt_client.subscribe(getTopic("rgb"));
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      cont += 1;
      if (cont > 150)
      {
        ESP.restart();
      }
      delay(5000);
    }
  }
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

void setupMQTT()
{
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);
}

void publicarDatos()
{
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);
  delay(100);
  String faltantes = "Datos no enviados\n: \n";
  String enviados = "";
  if (mqtt_client.connect(clientID, user, passwd))
  {
    Serial.println("Cliente conectado a MQTT Server");
    //************ Posteamos la temperatura ************
    temp = iaqSensor.temperature;
    //Serial.println("Temperatura : " + String(temp));
    String(temp).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("temp"), msg)) ? enviados += "Temperatura enviada\n" : faltantes += "Temperatura\n";

    //************ Posteamos la humedad ************
    hume = iaqSensor.humidity;
    //Serial.println("Humedad : " + String(hume));
    String(hume).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("hume"), msg)) ? enviados += "Humedad enviada\n" : faltantes += "Humedad\n";

    //************ Posteamos la Presion Atmosferica ************
    pres = iaqSensor.pressure;
    //Serial.println("Presion Atmosferica : " + String(pres));
    String(pres).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("pres"), msg)) ? enviados += "Presion Atmosferica  enviada\n" : faltantes += "Presion Atmosferica \n";

    //************ Posteamos el Index Air Quality ************
    aqi = iaqSensor.iaq;
    //Serial.println("Air Quality Index: " + String(aqi));
    String(aqi).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("aqi"), msg)) ? enviados += "Air Quality Index enviado\n" : faltantes += "Air Quality Index\n";

    //************ Posteamos el Static Index Air Quality ************
    sAQI = iaqSensor.staticIaq;
    //Serial.println("Static Air Quality Index: " + String(sAQI));
    String(sAQI).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("sAQI"), msg)) ? enviados += "Static Air Quality Index enviado\n" : faltantes += "Index Air Quality\n";

    //************ Posteamos el Index Air Quality Accurary ************
    AQIa = iaqSensor.iaqAccuracy;
    //Serial.println("Index Air Quality Accuracy : " + String(AQIa));
    String(AQIa).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("AQIa"), msg)) ? enviados += "Index Air Quality Accuracy enviado\n" : faltantes += "Index Air Quality Accuracy\n";

    //************ Posteamos el Gas Resistence ************
    gas = (iaqSensor.gasResistance) / 1000;
    //Serial.println("Gas Resistance kOhms: " + String(gas));
    String(gas).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("gas"), msg)) ? enviados += "Gas Resistance kOhms enviado\n" : faltantes += "Gas Resistance kOhms\n";

    //************ Posteamos el CO2 Equivalente ************
    CO2e = iaqSensor.co2Equivalent;
    //Serial.println("CO2 Equivalente : " + String(CO2e));
    String(CO2e).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("CO2e"), msg)) ? enviados += "CO2 Equivalente enviado\n" : faltantes += "CO2 Equivalente\n";

    //************ Posteamos el VOC Equivalente ************
    VOCe = iaqSensor.breathVocEquivalent;
    //Serial.println("VOC Equivalente : " + String(VOCe));
    String(VOCe).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("VOCe"), msg)) ? enviados += "VOC Equivalente enviado\n" : faltantes += "VOC Equivalente \n";

    //************ Posteamos la intensidad de señal ************
    rssi = WiFi.RSSI();
    //Serial.println("Intensidad de Señal : " + String(rssi));
    String(rssi).toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("rssi"), msg)) ? enviados += "Intensidad de Señal enviado\n" : faltantes += "Intensidad de Señal\n";

    /*Posteamos las mediciones de las particulas*/
    if (!particulas.read(&data))
    {
      Serial.println("Could not read from AQI");
      delay(50); // try again in a bit!
      return;
    }
    else
    {
      pm25 = data.pm25_env;
      String str11(pm25);
      str11.toCharArray(msg, 50);
      (mqtt_client.publish(getTopic("pm25"), msg)) ? enviados += "PM25 enviado\n" : faltantes += "PM25\n";

      pm10 = data.pm100_env;
      String str12(pm10);
      str12.toCharArray(msg, 50);
      (mqtt_client.publish(getTopic("pm10"), msg)) ? enviados += "PM10 enviado\n" : faltantes += "PM10\n";
    }

    sensorDS18B20.requestTemperatures();
    Serial.print("Temperatura sensor 1: ");
    ds18 = sensorDS18B20.getTempCByIndex(0);
    Serial.print(ds18);
    Serial.println(" C");
    String str13(ds18);
    str13.toCharArray(msg, 50);
    (mqtt_client.publish(getTopic("ds18"), msg)) ? enviados += "DS18 enviado\n" : faltantes += "DS18\n";

    Serial.println(enviados);

    if (faltantes != "Datos no enviados\n: \n")
    {
      Serial.println(faltantes);
      Serial.println("Estado de la conexión MQTT Server: ");
      Serial.println(mqtt_client.state());
      String(faltantes).toCharArray(msg, 2000);
      (mqtt_client.publish(getTopic("info"), msg)) ? Serial.println("Log enviado.") : Serial.println("Fallo en envio de log");
    }
    else
    {
      String("Todos enviados.").toCharArray(msg, 100);
      (mqtt_client.publish(getTopic("info"), msg)) ? Serial.println("Log enviado.") : Serial.println("Fallo en envio de log");
    }
  }
  else
  {
    Serial.println("Cliente NO conectado a MQTT Server");
    Serial.print("Estado del error de conexión: ");
    Serial.println(mqtt_client.state());
    reconnect();
  }
}

void preHeatSensor()
{
  unsigned long time_trigger = millis();
  if (iaqSensor.run())
  { // If new data is available
    output2 = String(time_trigger);

    output2 = String(time_trigger);
    output2 += ", " + String(iaqSensor.rawTemperature);
    output2 += ", " + String(iaqSensor.pressure);
    output2 += ", " + String(iaqSensor.rawHumidity);
    output2 += ", " + String(iaqSensor.gasResistance);
    output2 += ", " + String(iaqSensor.iaq);
    output2 += ", " + String(iaqSensor.iaqAccuracy);
    output2 += ", " + String(iaqSensor.temperature);
    output2 += ", " + String(iaqSensor.humidity);
    output2 += ", " + String(iaqSensor.staticIaq);
    output2 += ", " + String(iaqSensor.co2Equivalent);
    output2 += ", " + String(iaqSensor.breathVocEquivalent);
    //Serial.println(output2);
  }
  else
  {
    checkIaqSensorStatus();
  }
}