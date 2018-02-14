#include "settings.h"
#include <WiFi101.h>
#include <WiFiClient.h>
#include <ArduinoSound.h>
// #include <FastLED.h>
 
int ledpin = 6;

// WiFi variables
int status = WL_IDLE_STATUS;
int last_send = 0;
byte mac[6];
IPAddress ip;
WiFiClient client;

// I2S / FFT settings
// sample rate for the input
const int sampleRate = 8192;

// size of the FFT to compute
const int fftSize = 64;

// size of the spectrum output, half of FFT size
const int spectrumSize = fftSize / 2;

// array to store spectrum output
int spectrum[spectrumSize];

// array to store spectrum averages
int spectrum_avg[spectrumSize];
int spectrum_avg60s[spectrumSize];
int sample_cnt60s = 0;

// create an FFT analyzer to be used with the I2S input
FFTAnalyzer fftAnalyzer(fftSize);


void connectWifi() {
  Serial.print("Attempting to connect to Network named: ");
  Serial.println(ssid);          // print the network name (SSID);
  int connect_start = millis();
  while (status != WL_CONNECTED) {
    Serial.print(".");
    if(strlen(pass) == 0) {
      // Connect to open network
      status = WiFi.begin(ssid);
    } else {
      // Connect to WPA/WPA2 network
      status = WiFi.begin(ssid, pass);
    }
    if ((millis() - connect_start) > 30000) {
      Serial.println();
      Serial.println("Connect failed.");
      return;
    }
    // blink leds connection:
    digitalWrite(ledpin, LOW);
    delay(500);
    digitalWrite(ledpin, HIGH);
    delay(500);
  }
  Serial.println("WiFi connected");
  WiFi.macAddress(mac);
  ip = WiFi.localIP();
}

 
void setup() {
  Serial.begin(115200);      // initialize serial communication
  delay(1000);
  Serial.print("Start Serial ");
  // setup the I2S audio input for the sample rate with 32-bits per sample
  if (!AudioInI2S.begin(sampleRate, 32)) {
    Serial.println("Failed to initialize I2S input!");
    while (1); // do nothing
  }
  Serial.println("I2S input initialized successfully");

  // configure the I2S input as the input for the FFT analyzer
  if (!fftAnalyzer.input(AudioInI2S)) {
    Serial.println("Failed to set FFT analyzer input!");
    while (1); // do nothing
  }
  Serial.println("FFT analyzer input initialized successfully");

  pinMode(ledpin, OUTPUT);      // set the LED pin mode
  // Check for the presence of the shield
  Serial.print("WiFi101 shield: ");
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("NOT PRESENT");
    return; // don't continue
  }
  Serial.println("DETECTED");
  // attempt to connect to Wifi network:
  connectWifi();
  printWifiStatus();                        // you're connected now, so print out the status
  digitalWrite(ledpin, HIGH);
}


void sendData() {
  Serial.println("60 sec avg");
  // Display data part
  int g_start_idx = 1;
  //int g_stop_idx = 20; // or spectrumSize
  int g_stop_idx = spectrumSize;
  int g_step = 1;
  
  for (int i = g_start_idx; i < g_stop_idx; i+=g_step) {
    Serial.print((i * sampleRate) / fftSize); // the starting frequency
    Serial.print("\t"); // 
    Serial.print(spectrum_avg60s[i]); // the spectrum value
    Serial.print("\t"); // 
  }
  Serial.println(); 
  Serial.println("60 sec avg end");
  
  Serial.println("Connect to the server... ");
  if (client.connect(server, 80)) {
    Serial.println("connected");
    // Make a HTTP request:
    client.print("GET ");
    client.print(path);
    client.print("?");

    // Add mac address parameter
    client.print("mac=");
    for (byte i = 0; i < 6; ++i) {
      char buf[3];
      sprintf(buf, "%02X", mac[i]);
      client.print(buf);
    }

    // Add WiFi RSSI parameter
    client.print("&rssi=");
    client.print(WiFi.RSSI());

    // The rest of parameters
    client.print("&data=demotest&date=20180201");

    // Protocol
    client.println(" HTTP/1.1");

    // HTTP header lines
    client.print("Host: ");
    client.println(host);

    // WiFi SSID header
    client.print("X-WiFi-SSID: ");
    client.println(WiFi.SSID());

    // WiFi IP address header
    client.print("X-WiFi-IP: ");
    client.println(ip);    

    client.println("Accept: text/plain");
    client.println("Connection: close");
    client.println();
  }
}


void loop() {
  // FFT part
  int last_save = millis();
  int sample_cnt = 0;
  // Loop until 1000 ms is gone
  // and save samples to avg array
  while ((millis() - last_save) < 1000) {
    // check if a new analysis is available
    if (fftAnalyzer.available()) {
      sample_cnt++;
      // read the new spectrum
      fftAnalyzer.read(spectrum, spectrumSize);
      // Add all values to avg array
      for (int i = 0; i < spectrumSize; i++) {
        spectrum_avg[i] += spectrum[i];
      }
    }
  }
  // Calculate average
  for (int i = 0; i < spectrumSize; i++) {
    spectrum_avg[i] /= sample_cnt;
  }
  // Add all values to avg60s array
  for (int i = 0; i < spectrumSize; i++) {
    spectrum_avg60s[i] += spectrum_avg[i];
    sample_cnt60s++;
  }

  // Display data part
  int g_start_idx = 0;
  //int g_stop_idx = 20; // or spectrumSize
  int g_stop_idx = spectrumSize;
  int g_step = 1;
  // print out the spectrum
  for (int i = g_start_idx; i < g_stop_idx; i+=g_step) {
    Serial.print((i * sampleRate) / fftSize); // the starting frequency
    Serial.print("\t"); // 
    Serial.print(spectrum_avg[i]); // the spectrum value
    Serial.print("\t"); // 
  }
  Serial.println(); 

  // Clear avg array
  for (int i = 0; i < spectrumSize; i++) {
    spectrum_avg[i] = 0;
  }

  // Data send part
  if ((millis() - last_send) > 60000) {
    last_send = millis();
    for (int i = 0; i < spectrumSize; i++) {
      spectrum_avg60s[i] /= sample_cnt60s;
    }
    sample_cnt60s = 0;
    sendData();
    // Clear 60s avg array
    for (int i = 0; i < spectrumSize; i++) {
      spectrum_avg60s[i] = 0;
    }    
  }

  // Response display part
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
 
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
 
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

