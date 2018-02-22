#include "settings.h"
#include <WiFi101.h>
#include <WiFiClient.h>
#include <I2S.h>
#include <ArduinoSound.h>
// #include <FastLED.h>
 
int ledpin = 6;

// WiFi variables
//int status = WL_IDLE_STATUS;
int last_send = 0;
byte mac[6];
IPAddress ip;
WiFiClient client;

// I2S / FFT settings
// sample rate for the input
const int sampleRate = 20500;

// size of the FFT to compute
const int fftSize = 512;

// size of the spectrum output, half of FFT size
const int spectrumSize = fftSize / 2;

unsigned long time=0;
uint8_t vals=0;
uint8_t first=1;

uint64_t avg_vals=0;
uint64_t loops=0;
uint64_t avgval=0;
uint64_t avg_60s = 0;
uint64_t s60_len = 60;
uint64_t vals60s[60];
char buffer[16];


// array to store spectrum output
int spectrum[spectrumSize];

// array to store spectrum averages
int spectrum_avg[spectrumSize];
int spectrum_avg60s[spectrumSize];
int sample_cnt60s = 0;

// create an FFT analyzer to be used with the I2S input
// FFTAnalyzer fftAnalyzer(fftSize);


void connectWifi() {
  int status=0;
  Serial.print("Attempting to connect to Network named: ");
  Serial.println(WIFI_SSID);          // print the network name (SSID);
  int connect_start = millis();
  while (status != WL_CONNECTED) {
    Serial.print(".");
    if(strlen(WIFI_PASSWORD) == 0) {
      // Connect to open network
      status = WiFi.begin(WIFI_SSID);
    } else {
      // Connect to WPA/WPA2 network
      status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
  printMacAddr();
}

void printMacAddr() {
  for (byte i = 0; i < 6; ++i) {
    char buf[3];
    sprintf(buf, "%02X", mac[i]);
    Serial.print(buf);
  }
  Serial.println(" = MAC ADDRESS");
}
 
void setup() {
  Serial.begin(115200);      // initialize serial communication
  delay(1000);
  Serial.print("Start Serial ");
  // start I2S at 16 kHz with 32-bits per sample
  if (!I2S.begin(I2S_PHILIPS_MODE, sampleRate, 32)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }
  Serial.println("I2S input initialized successfully");

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


void zero_vals60s() {
  for(int i = 0; i < s60_len; i++) {
    vals60s[i] = 0;
  }
}

void sendData() {
  printMacAddr();
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

    // Add uptime parameter
    client.print("&uptime=");
    client.print(millis());

    // The rest of parameters
    client.print("&1s=");
    for(int i = 0; i < s60_len; i++) {
      itoa(vals60s[i],buffer,10);
      client.print(buffer);
      client.print(",");
    }

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
    // client.stop();
  } else {
    connectWifi();
  }
  zero_vals60s();
}


void loop() {
  int last_save = millis();
  int sample_cnt = 0;
  // Loop until 1000 ms is gone
  // and save samples to avg array
  while ((millis() - last_save) < 1000) {
    int32_t samples[fftSize];
  
    for (uint16_t i=0; i<fftSize; i++) {
      int32_t sample = 0;
      while ((sample == 0) || (sample == -1) ) {
        sample = I2S.read();
      }
      // convert to 18 bit signed
      sample >>= 14;
      samples[i] = sample;
    }
    // find the 'peak to peak' max
    int32_t maxsample=0, minsample=0;
    for (uint16_t i=0; i<fftSize; i++) {
      minsample = min(minsample, samples[i]);
      maxsample = max(maxsample, samples[i]);
    }
    avgval+=(maxsample-minsample);
    vals++;
  }
  if(first) {
    first=0;
    Serial.println("Skip first data (unstable data).");
  } else {
    itoa((avgval/vals),buffer,10);
    //Serial.println("");
    Serial.print(buffer);
    Serial.println();
    //Serial.println(" per 1 sec");
    vals60s[loops] = avgval/vals;
    avg_vals+=vals;
    loops++;
    avgval=0;
    vals=0;
  }
  
  // Data send 
  if ((millis() - last_send) >= (s60_len * 1000)) {
    last_send = millis();
    Serial.println();
    // FIXME: For some reason this is not working correctly
    avg_60s = avg_vals/loops;
    itoa(avg_60s,buffer,10);
    avg_vals=0;
    loops=0;
    //Serial.print(buffer);
    //Serial.println();
    //Serial.println(" per 60 secs");
    //Serial.println("Sending ...");
    sendData();
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

