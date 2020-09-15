// Agosto/2020
// TODO: dia 01 - Colocar limite para número de abertura e fechamento de lockers
// TODO: dia 01 - Na primeira leitura do Json de leitura da base de dados, carrega o Array
// TODO: dia 01 - Na leitura de Boolean, procurar o Locker Index no Array Ok
// TODO: dia 01 - Nas outras leituras do Json, verificar as mudanças de Confirma operação.
// TODO: dia 01 - Testar as 10 portas para fazer a placa no Fritzing Ok

#include "FirebaseESP32.h"
#include "ESP32TimerInterrupt.h"

#include <qrcode.h>
#include <SSD1306.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"

#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP_WiFiManager.h> 

//Carrega a biblioteca do sensor ultrassonico
#include <Ultrasonic.h>

//conexão dos pinos para o sensor ultrasonico
#define PIN_TRIGGER   2
#define PIN_ECHO      15
//Inicializa o sensor nos pinos definidos acima
Ultrasonic ultrasonic(PIN_TRIGGER, PIN_ECHO);

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define PIN_LED 2
#define PIN D1
#define NUM_LEDS 3

FirebaseData firebaseData;

// Pins que podem ser utilizados no ESP32
// Direito: H15,4,16,17,H5,18,19,23 (8-2High) = 6
// Esquerdo: 13,?12,H14,27,26,25,33,32 (8-1LOW-1HIGH) = 6
// HIGH durante o boot: 0.2.4.5.12.15 (nao pode ser utilizado como OUTPUT)
// LOW  durante o boot: 12 (nao deve ter pullup externo para sensor)
// Restritos INPUT: 35,34,39,36 (Nao tem Pullup e sao analogicos) = 4 - 2 = 2
// Pins nao usados
// SPI Flash: 6,7,8,9,10,11
// 2: LED, 21:i2c SDA, 3:RX, 1:TX, 22: i2c SCL
// Pins analogicos
//Interrupts

//Opções de roteamento Vr.1.0
//int relayPin[LOCKS] =  {14,15,4,16,17,5,18,19};
//int sensorPin[LOCKS] = {13,27,2 ou 12,23,32,33,25,26};
// ou Vr.1.1
//int relayPin[LOCKS] =  {14,15,4,16,17,5,35,34};
//int sensorPin[LOCKS] = {13,18,19,23,32,33,25,26};

int count = 0;
const byte LOCKS = 3;
int lockState[LOCKS] = {0,0,0};
// WRoom
int relayPin[LOCKS] =  {4,18,19};
int sensorPin[LOCKS] = {16,17,23};
const char* lockers[] = {"-MDL3rZ_I6c0JklDSCQ4", "-MDL3uym3jpNy_kHUZ9n", "-MDLrMsOnZLx7OqexYzz"};
byte lockerIdx = 0;
    
// lockState  0= fechado 
// relayPin HIGH= fechado (Ok) LOW=aberto (erro)
// lockState  1= fechado/ esperando abrir
// relayPin HIGH= fechado (Dispara fechadura) LOW=aberto (Passar lockState => 2)
// lockState  2= aberto/ esperando fechar
// relayPin HIGH= fechado (lockState => 0) LOW=aberto (Ok/ Ver Timeout)

SSD1306  display(0x3c, 21, 22);
//SSD1306  display(0x3c, 4, 15);
QRcode qrcode (&display);

//const char* ssid = "CASA";
//const char* password = "39385345";

// SSID and PW for Config Portal
String ssid = "ESP_" + String(ESP_getChipId(), HEX);
const char* password = "your_password";

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// Indicates whether ESP has WiFi credentials saved from previous session
bool initialConfig = false;

// Use true for dynamic DHCP IP, false to use static IP and  you have to change the IP accordingly to your network
#define USE_DHCP_IP     true

// Use DHCP
IPAddress stationIP   = IPAddress(0, 0, 0, 0);
IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);

IPAddress dns1IP      = gatewayIP;
IPAddress dns2IP      = IPAddress(8, 8, 8, 8);

const int TRIGGER_PIN =  16;   // Pin D0 mapped to pin GPIO0/BOOT/ADC11/TOUCH1 of ESP32
const int TRIGGER_PIN2 = 17;   // Pin D25 mapped to pin GPIO25/ADC18/DAC1 of ESP32

void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(" ");
  }
}

void check_status() {
  // is configuration portal requested?
  if ((digitalRead(TRIGGER_PIN) == LOW) && (digitalRead(TRIGGER_PIN2) == LOW))
  {
    Serial.println("\nConfiguration portal requested.");
    digitalWrite(PIN_LED, HIGH); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.

    //Local intialization. Once its business is done, there is no need to keep it around
    ESP_WiFiManager ESP_wifiManager;

    ESP_wifiManager.setMinimumSignalQuality(-1);

    //set custom ip for portal
    //ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    ESP_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask, dns1IP, dns2IP);   

    //Check if there is stored WiFi router/password credentials.
    //If not found, device will remain in configuration mode until switched off via webserver.
    Serial.print("Opening configuration portal. ");
    Router_SSID = ESP_wifiManager.WiFi_SSID();
    if (Router_SSID != "")
    {
      ESP_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
      Serial.println("Got stored Credentials. Timeout 120s");
    }
    else
    Serial.println("No stored Credentials. No timeout");

    //it starts an access point
    //and goes into a blocking loop awaiting configuration
    if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
    {
      Serial.println("Not connected to WiFi but continuing anyway.");
    }
    else
    {
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
    }

    digitalWrite(PIN_LED, LOW); // Turn led off as we are not in configuration mode.
  }

  static ulong checkstatus_timeout = 0;
  #define HEARTBEAT_INTERVAL    10000L
  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = millis() + HEARTBEAT_INTERVAL;
  }
}


void setup() {
  Serial.begin(115200);
  Serial.println("Init...");

  pinMode(2, OUTPUT);
  for (byte i=0; i < LOCKS; i++) {
    pinMode(relayPin[i], OUTPUT);
    pinMode(sensorPin[i], INPUT_PULLUP);
  }
  
  connectWifi();
  Firebase.begin("https://lime2.firebaseio.com/", "UX61Cz2L9ctDr1EziojXtMnQLoXBZKmdfgCwoL0d");
  Serial.println("Firebase init");
  Firebase.reconnectWiFi(true);
 
  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
  //In setup(), set the streaming path to "/todo" and begin stream connection

  Firebase.setStreamCallback(firebaseData, streamCallback, streamTimeoutCallback);

  if (!Firebase.beginStream(firebaseData, "/todo"))
  {
    //Could not begin stream connection, then print out the error detail
    Serial.println(firebaseData.errorReason());
  }

  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  
  // Initialize QRcode display using library
  qrcode.init();
  // create qrcode
  qrcode.create("Hello world.");

  display.clear();
  display.drawString(0, 0, "Limelocker");
  display.display();
  delay(1000);
}

void loop() {
  count += 1;
  Serial.println("Loop");
  char msg1[15];
  char msg2[15];
  char msg3[15]; 
  sprintf(msg1, "Dist.:%i", getDistance());
  sprintf(msg2, "Dist.:%i", getDistance());
  sprintf(msg3, "Dist.:%i", getDistance());
  Serial.println(msg1);
  display.clear();
  display.drawString(0, 0, "Limelocker");
  display.drawString(0, 20, msg1);
  display.drawString(0, 40, msg2);
  display.drawString(0, 60, msg3);
  display.display();
  delay(100);

  // put your main code here, to run repeatedly
  check_status();
  /*
  if (Firebase.getBool(firebaseData, "/todo/-MDL3rZ_I6c0JklDSCQ4/completed")) {
    //Serial.println(firebaseData.dataType());
    if  (firebaseData.dataType() == "boolean") {
      bool val = firebaseData.boolData();
      Serial.print("Porta1: ");
      Serial.println(val);
    }
  }
  if (Firebase.getString(firebaseData, "/todo/-MDL3uym3jpNy_kHUZ9n/subject")) {
    if  (firebaseData.dataType() == "string") {
      String val = firebaseData.stringData();  
      Serial.print("Porta2: ");
      Serial.println(val);
    }
  }
  */

  if (!Firebase.readStream(firebaseData))
  {
    Serial.println(firebaseData.errorReason());
  }

  int porta{
  
  for (byte i=0; i < LOCKS; i++) {
    Serial.print(i);
    Serial.print(lockState[i]);
    Serial.print(" : ");
    Serial.println(digitalRead(sensorPin[i]));
  }
  
  for (byte i=0; i < LOCKS; i++) {
    if (lockState[i] == 0) {
      // Locker should be closed
      // Just verify if the locker is open
      if (digitalRead(sensorPin[i]) == LOW) {
          Serial.println("Porta está aberta com erro");
          // Se tiver vazio Ok, mas se tiver algo dentro pode ser roubo
      }
    } else if (lockState[i] == 1) {
      // locker is openning
      // Verify if the locker is open
      if (digitalRead(sensorPin[i]) == LOW) {
        //Locker is opened
        lockState[i] = 2;
        Serial.print("Porta aberta: ");
        Serial.println(i);
        digitalWrite(relayPin[i], HIGH);
        //qrcode.create("Porta aberta.");
      } else {
        //Locker is closed
        // Try to open again
        digitalWrite(relayPin[i], HIGH);
        delay(500);     
        digitalWrite(relayPin[i], LOW);
      } 
    } else { // lockState[i] = 2
      // locker is closing
      if (digitalRead(sensorPin[i]) == LOW) {
        //Locker is opened
        // Wait to close
      } else {
        //Locker is closed
        lockState[i] = 0;
        Serial.print("Porta fechada: ");
        Serial.println(i);
        digitalWrite(relayPin[i], LOW);
        String Str1(lockers[lockerIdx]);
        String data = "/todo/" + Str1 + "/completed";
        Serial.println(data);
        Firebase.setBool(firebaseData, data, false);
      }
    }     
    delay(10);
  }       
  delay(500);
}
  
//Global function that handles stream data
void streamCallback(StreamData data)
{
  //Print out all information
  Serial.print("Stream DataType: ");
  Serial.println(data.dataType());
  Serial.println(data.streamPath());
  Serial.println(data.dataPath());
  Serial.println(data.dataType());

  //Print out the value
  //Stream data can be many types which can be determined from function dataType

  if (data.dataType() == "int")
    Serial.println(data.intData());
  else if (data.dataType() == "float")
    Serial.println(data.floatData(), 5);
  else if (data.dataType() == "double")
    printf("%.9lf\n", data.doubleData());
  else if (data.dataType() == "boolean"){
    Serial.println(data.boolData() == 1 ? "true" : "false");
    //TODO: procurar o locker
    int pos1 = data.dataPath().indexOf("/");
    int pos2 = data.dataPath().indexOf("/", pos1+1);
    String S1 = data.dataPath().substring(1, 21);
    Serial.println(S1);
    //dentro do array
    for (byte i=0; i < LOCKS; i++) {
      String Str(lockers[i]);
      if(Str == S1) {
        lockerIdx = i;
        // Open locker x
        lockState[lockerIdx] = 1;
        break;
      }
    }
    
  } else if (data.dataType() == "string")
    Serial.println(data.stringData());
  else if (data.dataType() == "json") {
    Serial.println(data.jsonString());
    
    //const size_t capacity  = 600;
    //DynamicJsonDocument doc(600);

    // Parse JSON object
    
    //DeserializationError error = deserializeJson(doc, data);
    //if (error) {
      //Serial.print(F("deserializeJson() failed: "));
      //Serial.println(error.c_str());
      return;
    //}
  }
}

void connectWifi() {
  /*** OLD CODE
  // Let us connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(".......");
  Serial.println("WiFi Connected....IP Address:");
  Serial.println(WiFi.localIP());
  */
  
  Serial.println("\nStarting ConfigOnSwitch on " + String(ARDUINO_BOARD));

  unsigned long startedAt = millis();

  //Local intialization. Once its business is done, there is no need to keep it around
  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("ConfigOnSwitch");

  ESP_wifiManager.setDebugOutput(true);

  // Use only to erase stored WiFi Credentials
  //resetSettings();
  //ESP_wifiManager.resetSettings();

  //set custom ip for portal
  //ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

  ESP_wifiManager.setMinimumSignalQuality(-1);

  // We can't use WiFi.SSID() in ESP32as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  // SSID to uppercase
  ssid.toUpperCase();

  if (Router_SSID == "")
  {
    Serial.println("Open Config Portal without Timeout: No stored Credentials.");

    digitalWrite(PIN_LED, HIGH); // Turn led on as we are in configuration mode.

    //it starts an access point
    //and goes into a blocking loop awaiting configuration
    if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
      Serial.println("Not connected to WiFi but continuing anyway.");
    else
      Serial.println("WiFi connected...yeey :)");
  }

  digitalWrite(PIN_LED, LOW); // Turn led off as we are not in configuration mode.

#define WIFI_CONNECT_TIMEOUT        30000L
#define WHILE_LOOP_DELAY            200L
#define WHILE_LOOP_STEPS            (WIFI_CONNECT_TIMEOUT / ( 3 * WHILE_LOOP_DELAY ))

  startedAt = millis();

  while ( (WiFi.status() != WL_CONNECTED) && (millis() - startedAt < WIFI_CONNECT_TIMEOUT ) )
  {
    WiFi.mode(WIFI_STA);
    WiFi.persistent (true);

    // We start by connecting to a WiFi network

    Serial.print("Connecting to ");
    Serial.println(Router_SSID);

    WiFi.config(stationIP, gatewayIP, netMask);
    //WiFi.config(stationIP, gatewayIP, netMask, dns1IP, dns2IP);

    WiFi.begin(Router_SSID.c_str(), Router_Pass.c_str());

    int i = 0;
    while ((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && i++ < WHILE_LOOP_STEPS)
    {
      delay(WHILE_LOOP_DELAY);
    }
  }

  Serial.print("After waiting ");
  Serial.print((millis() - startedAt) / 1000);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
}

//The library will resume the stream connection automatically
void streamTimeoutCallback(bool timeout)
{
  if(timeout){
    //Stream timeout occurred
    Serial.println("Stream timeout, resume streaming...");
  }  
}

/*
  FAZ A LEITURA DA DISTANCIA ATUAL CALCULADA PELO SENSOR
*/
int getDistance()
{
    //faz a leitura das informacoes do sensor (em cm)
    int distanciaCM;
    long microsec = ultrasonic.timing();
    // pode ser um float ex: 20,42 cm se declarar a var float 
    distanciaCM = ultrasonic.convert(microsec, Ultrasonic::CM);
 
    return distanciaCM;
}
