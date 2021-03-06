/*
 This example reads audio data from an Invensense's ICS43432 I2S microphone
 breakout board, and prints out the spectrum to the Serial console. The
 Serial Plotter built into the Arduino IDE can be used to plot the audio
 amplitude data (Tools -> Serial Plotter)

 Circuit:
 * Arduino/Genuino Zero, MKRZero or MKR1000 board
 * ICS43432:
   * GND connected GND
   * 3.3V (3V) connected 3.3V (Zero) or VCC (MKR1000, MKRZero)
   * WS (LRCL) connected to pin 0 (Zero) or pin 3 (MKR1000, MKRZero)
   * CLK (BCLK) connected to pin 1 (Zero) or pin 2 (MKR1000, MKRZero)
   * SD (DOUT) connected to pin 9 (Zero) or pin A6 (MKR1000, MKRZero)

 created 21 November 2016
 by Sandeep Mistry
 */

// This is originated from i2s_SpectrumSerialPlotter
#include <ArduinoSound.h>

// sample rate for the input
const int sampleRate = 8192;

// size of the FFT to compute
const int fftSize = 64;

// size of the spectrum output, half of FFT size
const int spectrumSize = fftSize / 2;

// array to store spectrum output
int spectrum[spectrumSize];

// array to store spectrum average
int spectrum_avg[spectrumSize];

// create an FFT analyzer to be used with the I2S input
FFTAnalyzer fftAnalyzer(fftSize);

// const bool single = true;  // Print multiple values (false) or just one (true)
const bool single = false;  // Print multiple values (false) or just one (true)

void setup() {
// Open serial communications and wait for port to open:
  // A baud rate of 115200 is used instead of 9600 for a faster data rate
  // on non-native USB ports
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // setup the I2S audio input for the sample rate with 32-bits per sample
  if (!AudioInI2S.begin(sampleRate, 32)) {
    Serial.println("Failed to initialize I2S input!");
    while (1); // do nothing
  }

  // configure the I2S input as the input for the FFT analyzer
  if (!fftAnalyzer.input(AudioInI2S)) {
    Serial.println("Failed to set FFT analyzer input!");
    while (1); // do nothing
  }
}

void loop() {
  int last_save = millis();
  int sample_cnt = 0;
  // Loop until 1000 ms is gone
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
  for (int i = 0; i < spectrumSize; i++) {
    spectrum_avg[i] /= sample_cnt;
  }
    
  int g_start_idx = 1;
  //int g_stop_idx = 20; // or spectrumSize
  int g_stop_idx = spectrumSize;
  int g_step = 1;
  // print out the spectrum
  if (single) {
    for (int i = g_start_idx; i < g_stop_idx; i++) {
      Serial.print((i * sampleRate) / fftSize); // the starting frequency
      Serial.print("\t"); // 
      Serial.println(spectrum_avg[i]); // the spectrum value
    }
  } else {
    // print out the spectrum, multiple lines
    for (int i = g_start_idx; i < g_stop_idx; i += g_step) {
      Serial.print((i * sampleRate) / fftSize); // the starting frequency
      Serial.print("\t"); // 
      Serial.print(spectrum_avg[i]); // the spectrum value
      Serial.print("\t"); // 
    }
    Serial.println();
  }
  
}
