#include <WiFi.h>
#include <HTTPClient.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"

#define RST_PIN         4         // RC522 RST
#define SS_PIN          5         // RC522 SDA (SS)
#define BUZZER_PIN      17        // Buzzer pin

#define LCD_SDA         21
#define LCD_SCL         22
#define LCD_COLS        16
#define LCD_ROWS        2
uint8_t LCD_I2C_ADDR = 0x27;      // Ganti ke 0x3F jika alamat I2C berbeda

// --- Konfigurasi Jaringan WiFi ---
const char* ssid = "topglobalgatot";
const char* password = "123456789";

// --- Konfigurasi Alamat Server ---
const char* SERVER_IP = "172.26.178.117"; 
const int SERVER_PORT = 3000;
const char* ESP32_CAM_IP = "172.26.178.200";

String serverEndpoint = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/attendance/tap";

// =================================================================
// ==                   VARIABEL GLOBAL & KONSTANTA               ==
// =================================================================
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
unsigned long last_tap_time = 0;

#define MIN_TAP_INTERVAL 3000
#define CAM_CAPTURE_TIMEOUT 10000
#define SERVER_POST_TIMEOUT 15000

// =================================================================
// ==                     FUNGSI-FUNGSI HELPER                    ==
// =================================================================

String uidToString(byte *uid, byte bufferSize) {
  String uidStr = "";
  for (byte i = 0; i < bufferSize; i++) {
    uidStr += (uid[i] < 0x10 ? "0" : "");
    uidStr += String(uid[i], HEX);
  }
  return uidStr;
}

// BUZZER DIPERBAIKI - Support Active Low & Active High
void buzz(bool success) {
  if (success) {
    // Bunyi 1x panjang (sukses)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // Diperpanjang dari 100ms ke 200ms
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  } else {
    // Bunyi 3x pendek (gagal)
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);  // Diperpanjang dari 50ms ke 100ms
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);  // Diperpanjang dari 50ms ke 100ms
    }
  }
}

String base64Encode(const uint8_t* data, size_t length) {
  if (length == 0 || data == NULL) return "";
  size_t output_length = 0;
  mbedtls_base64_encode(NULL, 0, &output_length, data, length);
  char* buffer = (char*)malloc(output_length);
  if (buffer == NULL) { Serial.println("Base64 malloc failed"); return ""; }
  if (mbedtls_base64_encode((unsigned char*)buffer, output_length, &output_length, data, length) != 0) {
    Serial.println("Base64 encode failed");
    free(buffer);
    return "";
  }
  String result(buffer);
  free(buffer);
  return result;
}

bool requestPhotoData(uint8_t** frame_buffer, size_t* frame_size) {
  if (WiFi.status() != WL_CONNECTED) { 
    Serial.println("WiFi disconnected, cannot request photo."); 
    return false; 
  }
  
  HTTPClient http;
  String photoUrl = "http://" + String(ESP32_CAM_IP) + "/capture";
  Serial.println("Requesting photo from: " + photoUrl);
  
  http.begin(photoUrl);
  http.setTimeout(CAM_CAPTURE_TIMEOUT);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == HTTP_CODE_OK) {
    *frame_size = http.getSize();
    if (*frame_size <= 0 || *frame_size > 500000) { 
      Serial.printf("Invalid photo size received: %zu\n", *frame_size);
      http.end(); 
      return false; 
    }
    *frame_buffer = (uint8_t*)malloc(*frame_size);
    if (*frame_buffer == NULL) { 
      Serial.println("Failed to allocate memory for photo buffer.");
      http.end(); 
      return false; 
    }
    
    WiFiClient* stream = http.getStreamPtr();
    size_t bytesRead = stream->readBytes(*frame_buffer, *frame_size);
    http.end();
    
    if (bytesRead == *frame_size) {
      Serial.printf("Photo received successfully: %zu bytes\n", *frame_size);
      return true;
    } else {
      Serial.printf("Incomplete photo data. Read %zu/%zu bytes.\n", bytesRead, *frame_size);
      free(*frame_buffer);
      *frame_buffer = NULL;
      return false;
    }
  } else {
    Serial.printf("CAM HTTP GET Error. Code: %d\n", httpResponseCode);
    http.end();
    return false;
  }
}

void displayLCD(String line1, String line2 = "", int delayMs = 0) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, LCD_COLS));
  if (line2 != "") {
    lcd.setCursor(0, 1);
    lcd.print(line2.substring(0, LCD_COLS));
  }
  if (delayMs > 0) {
    delay(delayMs);
  }
}

// =================================================================
// ==                        FUNGSI SETUP                         ==
// =================================================================
void setup() {
  Serial.begin(115200);
  
  // SETUP BUZZER - PENTING!
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);  // Pastikan off saat startup
  
  // Test buzzer saat startup
  Serial.println("Testing buzzer...");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("Buzzer test done.");
  
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  displayLCD("Sistem Absensi", "Inisialisasi...", 1500);  // Diperpanjang dari 1000ms

  // Hubungkan ke WiFi
  displayLCD("Menghubungkan ke", ssid);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  displayLCD("WiFi Terhubung!", WiFi.localIP().toString(), 2500);  // Diperpanjang dari 2000ms

  SPI.begin();
  rfid.PCD_Init();
  
  displayLCD("Tap Kartu...", "");
}

// =================================================================
// ==                         FUNGSI LOOP                         ==
// =================================================================
void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    if (last_tap_time != 0 && millis() - last_tap_time > MIN_TAP_INTERVAL) {
      displayLCD("Tap Kartu...", "");
      last_tap_time = 0;
    }
    delay(50);
    return;
  }

  if (millis() - last_tap_time < MIN_TAP_INTERVAL) {
    Serial.println("Tap terlalu cepat, diabaikan.");
    rfid.PICC_HaltA(); 
    rfid.PCD_StopCrypto1();
    return;
  }
  last_tap_time = millis();

  // --- PROSES TAP DIMULAI ---
  String uid = uidToString(rfid.uid.uidByte, rfid.uid.size);
  Serial.println("=====================");
  Serial.println("Kartu Ditemukan! UID: " + uid);
  
  // BUZZER SAAT KARTU TERDETEKSI - BUNYI PENDEK
  digitalWrite(BUZZER_PIN, HIGH);
  delay(50);
  digitalWrite(BUZZER_PIN, LOW);
  
  displayLCD("Memproses...", uid);
  delay(500);  // Biar user sempat baca

  // 1. Ambil foto dari ESP32-CAM
  displayLCD("Mengambil Foto", "Tunggu...");
  uint8_t* fb = NULL;
  size_t fb_len = 0;
  String base64Photo = "";

  if (requestPhotoData(&fb, &fb_len)) {
    displayLCD("Foto Berhasil!", "Encoding...");
    delay(800);  // Biar user tau foto berhasil
    base64Photo = base64Encode(fb, fb_len);
    free(fb);
  } else {
    Serial.println("Peringatan: Gagal mengambil foto. Melanjutkan tanpa foto.");
    displayLCD("Foto Gagal!", "Lanjut tanpa foto");
    delay(1500);  // Peringatan lebih lama
  }

  // 3. Kirim data ke server Express.js
  displayLCD("Mengirim Data", "Ke Server...");
  HTTPClient http;
  http.begin(serverEndpoint);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(SERVER_POST_TIMEOUT);

  String jsonPayload = "{\"uid\":\"" + uid + "\"";
  if (base64Photo.length() > 0) {
    jsonPayload += ",\"image_data\":\"" + base64Photo + "\"";
  }
  jsonPayload += "}";

  Serial.println("Mengirim POST ke server...");
  int httpResponseCode = http.POST(jsonPayload);

  // 4. Proses balasan dari server
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("Server Response Code: %d\n", httpResponseCode);
    Serial.println("Server Response Body: " + response);

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);
    String message = error ? "Respon Error" : doc["message"].as<String>();

    switch (httpResponseCode) {
      case 200: // Sukses
        buzz(true);  // BUZZER SUKSES
        if (message.startsWith("Registration prompt")) {
          displayLCD("Kartu Baru!", "Daftar di Web");
          delay(3000);  // DIPERPANJANG - biar sempat baca
        } else {
          displayLCD("Absensi Sukses!", message);
          delay(3000);  // DIPERPANJANG - biar sempat baca
        }
        break;
      case 404: // Tidak Terdaftar
        buzz(false);  // BUZZER GAGAL
        displayLCD("Gagal!", "Tidak Terdaftar");
        delay(3000);  // DIPERPANJANG
        break;
      case 409: // Sudah Absen
        buzz(false);  // BUZZER GAGAL
        displayLCD("Info", message);
        delay(3000);  // DIPERPANJANG
        break;
      default: // Error Lain
        buzz(false);  // BUZZER GAGAL
        displayLCD("Server Error", "Code: " + String(httpResponseCode));
        delay(3000);  // DIPERPANJANG
        break;
    }
  } else { // Error koneksi HTTP
    buzz(false);  // BUZZER GAGAL
    Serial.printf("HTTP POST Error: %s\n", http.errorToString(httpResponseCode).c_str());
    displayLCD("Koneksi Gagal", http.errorToString(httpResponseCode).c_str());
    delay(3000);  // DIPERPANJANG
  }
 
  http.end();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  // Kembali ke idle setelah delay
  delay(500);
  displayLCD("Tap Kartu...", "");
}