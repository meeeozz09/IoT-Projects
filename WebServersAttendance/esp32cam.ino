#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "Arduino.h"
#include "soc/soc.h"           
#include "soc/rtc_cntl_reg.h"  
#include "driver/rtc_io.h"
#include <WebServer.h>  

const char* ssid = "topglobalgatot";
const char* password = "123456789";

IPAddress staticIP(172, 26, 178, 200);
IPAddress gateway(172, 26, 178, 151);
IPAddress subnet(255, 255, 255, 0);

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define LED_PIN 4

WebServer server(80);

// Counter untuk monitoring
unsigned long captureCount = 0;
unsigned long lastCaptureTime = 0;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>ESP32-CAM RFID Integration</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 800px;
      margin: 50px auto;
      padding: 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    }
    .container {
      background: white;
      padding: 30px;
      border-radius: 15px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.3);
    }
    h1 { color: #333; text-align: center; margin-bottom: 10px; }
    .subtitle { text-align: center; color: #666; margin-bottom: 30px; }
    .status {
      background: #e8f5e9;
      padding: 15px;
      border-radius: 8px;
      margin: 20px 0;
      border-left: 4px solid #4caf50;
    }
    button {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      padding: 12px 30px;
      font-size: 16px;
      border-radius: 25px;
      cursor: pointer;
      margin: 10px;
      font-weight: bold;
    }
    button:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }
    #photo { max-width: 100%; margin-top: 20px; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }
    .info { color: #666; font-size: 14px; margin-top: 20px; padding: 15px; background: #f5f5f5; border-radius: 8px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📷 ESP32-CAM for RFID</h1>
    <div class="subtitle">Attendance System</div>
    
    <div class="status">
      <strong>✅ Status:</strong> Online & Ready<br>
      <strong>📡 Endpoint:</strong> http://172.26.178.200/capture<br>
      <strong>🔧 Mode:</strong> RFID Auto-Capture
    </div>
    
    <div style="text-align: center;">
      <button onclick="testCapture()">🧪 Test Capture</button>
      <button onclick="location.reload()">🔄 Refresh Page</button>
    </div>
    
    <div id="result"></div>
    
    <div class="info">
      💡 <strong>Info:</strong> Kamera akan otomatis mengambil foto saat ESP32 RFID mendeteksi kartu. 
      Gunakan tombol "Test Capture" untuk testing manual.
    </div>
  </div>

  <script>
    function testCapture() {
      document.getElementById('result').innerHTML = '<p style="text-align:center; color:#667eea;">⏳ Capturing photo...</p>';
      
      fetch('/capture?' + Date.now())
        .then(response => {
          if (!response.ok) throw new Error('Capture failed');
          return response.blob();
        })
        .then(blob => {
          const url = URL.createObjectURL(blob);
          const sizeKB = Math.round(blob.size/1024);
          document.getElementById('result').innerHTML = 
            '<p style="color:green; text-align:center;">✅ Photo captured! (' + sizeKB + ' KB)</p>' +
            '<img id="photo" src="' + url + '">';
        })
        .catch(error => {
          document.getElementById('result').innerHTML = 
            '<p style="color:red; text-align:center;">❌ Error: ' + error + '</p>';
        });
    }
  </script>
</body>
</html>
)rawliteral";

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA; // 800x600
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    Serial.println("✓ PSRAM found - SVGA (800x600)");
  } else {
    config.frame_size = FRAMESIZE_VGA; // 640x480
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("⚠ PSRAM not found - VGA (640x480)");
  }

  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH);
  delay(10);
  digitalWrite(PWDN_GPIO_NUM, LOW);
  delay(10);

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init FAILED: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_gain_ctrl(s, 1);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
  }

  Serial.println("✓ Camera initialized!");
  return true;
}

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleCapture() {
  unsigned long startTime = millis();
  
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.printf("📸 Capture request #%lu\n", ++captureCount);
  Serial.printf("   From: %s\n", server.client().remoteIP().toString().c_str());
  
  digitalWrite(LED_PIN, HIGH);
  
  camera_fb_t * fb = NULL;
  int attempts = 0;
  
  while (attempts < 3 && fb == NULL) {
    attempts++;
    Serial.printf("   Attempt %d/3...\n", attempts);
    fb = esp_camera_fb_get();
    if (fb == NULL) {
      Serial.println("   ✗ Failed, retry...");
      delay(100);
    }
  }
  
  digitalWrite(LED_PIN, LOW);
  
  if (fb == NULL || fb->len == 0) {
    Serial.println("❌ CAPTURE FAILED!");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    server.send(500, "text/plain", "Camera capture failed");
    if (fb) esp_camera_fb_return(fb);
    return;
  }
  
  unsigned long captureTime = millis() - startTime;
  Serial.printf("✅ SUCCESS!\n");
  Serial.printf("   Size: %zu bytes (%.2f KB)\n", fb->len, fb->len / 1024.0);
  Serial.printf("   Resolution: %dx%d\n", fb->width, fb->height);
  Serial.printf("   Time: %lu ms\n", captureTime);
  
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  lastCaptureTime = millis();
  Serial.println("✓ Photo sent to ESP32 RFID");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  
  esp_camera_fb_return(fb);
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"captures\":" + String(captureCount) + ",";
  json += "\"uptime\":" + String(millis()/1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"rssi\":" + String(WiFi.RSSI());
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  ESP32-CAM for RFID Attendance System ║");
  Serial.println("╚════════════════════════════════════════╝");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n[1/3] Initializing camera...");
  if (!initCamera()) {
    Serial.println("❌ Camera init FAILED!");
    Serial.println("System halted. Check hardware!");
    while(1) { 
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  }
  
  Serial.println("   Testing first capture...");
  camera_fb_t * test_fb = esp_camera_fb_get();
  if (test_fb) {
    Serial.printf("   ✓ Test OK: %zu bytes\n", test_fb->len);
    esp_camera_fb_return(test_fb);
  } else {
    Serial.println("   ⚠ WARNING: Test failed!");
  }

  Serial.println("\n[2/3] Connecting to WiFi...");
  Serial.printf("   SSID: %s\n", ssid);
  Serial.printf("   Static IP: %s\n", staticIP.toString().c_str());
  
  WiFi.mode(WIFI_STA);
  WiFi.config(staticIP, gateway, subnet);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi FAILED!");
    while(1) { delay(1000); }
  }

  Serial.println("✓ WiFi connected!");
  Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("   Signal: %d dBm\n", WiFi.RSSI());

  Serial.println("\n[3/3] Starting web server...");
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("✓ Web server started!");

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║          SYSTEM READY!                 ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("\n📷 Ready for RFID capture requests");
  Serial.printf("🌐 Test: http://%s\n", WiFi.localIP().toString().c_str());
  Serial.printf("📸 Endpoint: http://%s/capture\n\n", WiFi.localIP().toString().c_str());
}

void loop() {
  server.handleClient();
  
  // Monitoring
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠ WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
    } else {
      Serial.printf("[Monitor] Uptime: %lu s | Captures: %lu | Heap: %d | RSSI: %d dBm\n",
                    millis()/1000, captureCount, ESP.getFreeHeap(), WiFi.RSSI());
    }
    lastCheck = millis();
  }
  
  delay(10);
}