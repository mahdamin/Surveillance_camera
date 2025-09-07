#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"
#include "SD_MMC.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

// --- Network Credentials ---
const char* ssid = "ZTE_2.4G_EhqFdr";
const char* password = "nFMvEpwM";

// --- Static IP Configuration ---
IPAddress staticIP(192, 168, 1, 253);     // Your desired static IP
IPAddress gateway(192, 168, 1, 1);        // Your router's gateway (usually .1)
IPAddress subnet(255, 255, 255, 0);       // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);         // Primary DNS (Google DNS)
IPAddress secondaryDNS(8, 8, 4, 4);       // Secondary DNS (Google DNS)

// --- Pin Definitions (AI-Thinker ESP32-CAM) ---
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22
#define FLASH_LED_PIN     4

// --- Global Variables & State Management ---
AsyncWebServer server(80);

// Define the system's operational modes
enum SystemMode { MODE_IDLE, MODE_STREAMING, MODE_RECORDING };
SystemMode currentMode = MODE_IDLE;

// --- Recording Control ---
unsigned long recordingStartTime = 0;
const unsigned long segmentDuration = 3600 * 1000UL; // 1 hour
File videoFile;
char currentFileName[30];
const uint64_t STORAGE_THRESHOLD = 2 * 1024 * 1024 * 1024ULL; // 2GB

// --- Performance monitoring ---
unsigned long frameCount = 0;
float currentFPS = 0;
unsigned long lastFPSTime = 0;
unsigned long lastFrameTime = 0;

// --- Streaming optimization ---
bool streamActive = false;
volatile bool frameReady = false;
camera_fb_t* currentFrame = nullptr;
SemaphoreHandle_t frameMutex;

// --- Task handles ---
TaskHandle_t streamTaskHandle = nullptr;
TaskHandle_t cameraTaskHandle = nullptr;

// --- Web Interface ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32-CAM High FPS Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #1a1a1a; color: white; }
    .container { max-width: 800px; margin: 0 auto; }
    h1 { text-align: center; color: #4CAF50; }
    .status-bar { background: #333; padding: 15px; border-radius: 8px; text-align: center; margin-bottom: 20px; }
    .status-value { font-size: 24px; font-weight: bold; }
    .controls { background: #333; padding: 20px; border-radius: 10px; margin-bottom: 20px; text-align: center; }
    .stream-container { text-align: center; background: #222; padding: 10px; border-radius: 10px; min-height: 240px;}
    .stream-img { max-width: 100%; height: auto; border-radius: 8px; }
    button { background: #007bff; color: white; border: none; padding: 12px 24px; border-radius: 6px; cursor: pointer; margin: 5px; font-size: 16px; }
    button:hover { opacity: 0.9; }
    button.stop-btn { background: #f44336; }
    #mode-status { transition: color 0.3s; }
    .fps-indicator { font-size: 14px; color: #4CAF50; font-weight: bold; }
    .perf-stats { font-size: 11px; color: #888; margin-top: 5px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32-CAM High Performance Controller</h1>
    
    <div class="status-bar">
      <div>Current Mode</div>
      <div class="status-value" id="mode-status">IDLE</div>
      <div class="fps-indicator">Stream FPS: <span id="fps">0.0</span> | Target: 10+ FPS</div>
      <div class="perf-stats">
        Heap: <span id="heap">0</span> KB | SD: <span id="sd_space">0.00</span> GB | 
        Load: <span id="frame-time">0</span>ms
      </div>
    </div>

    <div class="controls">
      <button id="stream-btn" onclick="startStream()">Start Stream</button>
      <button id="record-btn" onclick="startRecording()">Start Recording</button>
      <button id="stop-btn" class="stop-btn" onclick="stopAll()">Stop</button>
    </div>

    <div class="stream-container">
      <img id="stream" class="stream-img" style="display: none;">
      <div id="stream-placeholder" style="color: #666; padding: 50px;">
        High-performance stream will appear here<br>
        <small>Optimized for 10+ FPS</small>
      </div>
    </div>
  </div>

  <script>
    const streamImg = document.getElementById('stream');
    const streamPlaceholder = document.getElementById('stream-placeholder');
    const modeStatus = document.getElementById('mode-status');
    
    let frameCount = 0;
    let lastFrameTime = Date.now();
    let currentFPS = 0;
    let isStreaming = false;
    let frameLoadTime = 0;

    function updateStatus() {
      fetch('/stats')
        .then(response => response.json())
        .then(data => {
          modeStatus.textContent = data.mode;
          document.getElementById('sd_space').textContent = data.sd_free_gb.toFixed(2);
          document.getElementById('heap').textContent = (data.heap / 1024).toFixed(0);
          document.getElementById('fps').textContent = currentFPS.toFixed(1);
          document.getElementById('frame-time').textContent = frameLoadTime;
          
          if (data.mode === 'RECORDING') {
            modeStatus.style.color = '#f44336';
          } else if (data.mode === 'STREAMING') {
            modeStatus.style.color = '#4CAF50';
          } else {
            modeStatus.style.color = '#ffffff';
          }
        });
    }

    function startStream() {
      stopAll();
      
      fetch('/stream/start')
        .then(response => {
          if (response.ok) {
            streamPlaceholder.style.display = 'none';
            streamImg.style.display = 'block';
            isStreaming = true;
            frameCount = 0;
            lastFrameTime = Date.now();
            loadFrame();
          }
        });
    }
    
    function loadFrame() {
      if (!isStreaming || modeStatus.textContent !== 'STREAMING') return;
      
      const loadStart = Date.now();
      const img = new Image();
      
      img.onload = function() {
        streamImg.src = img.src;
        frameCount++;
        
        const now = Date.now();
        frameLoadTime = now - loadStart;
        
        // Calculate FPS every second
        if (now - lastFrameTime >= 1000) {
          currentFPS = frameCount / ((now - lastFrameTime) / 1000);
          frameCount = 0;
          lastFrameTime = now;
        }
        
        // Immediate next frame request for maximum FPS
        setTimeout(loadFrame, 20); // Target ~50fps client side, limited by server
      };
      
      img.onerror = function() {
        console.log('Frame error, retrying...');
        setTimeout(loadFrame, 100);
      };
      
      img.src = '/frame?t=' + Date.now();
    }
    
    function startRecording() {
      stopAll();
      fetch('/recording/start');
    }
    
    function stopAll() {
      isStreaming = false;
      streamImg.style.display = 'none';
      streamPlaceholder.style.display = 'block';
      streamImg.src = '';
      fetch('/stop');
    }
    
    setInterval(updateStatus, 1000);
    window.onload = updateStatus;
  </script>
</body>
</html>
)rawliteral";

// --- Camera Task for Continuous Frame Capture ---
void cameraTask(void* parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(50); // ~20 FPS capture rate target
  
  while (true) {
    if (currentMode == MODE_STREAMING && streamActive) {
      camera_fb_t* fb = esp_camera_fb_get();
      if (fb) {
        if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          if (currentFrame) {
            esp_camera_fb_return(currentFrame);
          }
          currentFrame = fb;
          frameReady = true;
          xSemaphoreGive(frameMutex);
        } else {
          esp_camera_fb_return(fb);
        }
      }
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// --- Function Prototypes ---
void setupCamera();
void setupWebServer();
void startRecording();
void stopRecording();
void recordFrame();
void manageStorage();
String getModeString();

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32-CAM High FPS Controller ===");

  // Create mutex for frame access
  frameMutex = xSemaphoreCreateMutex();

  // --- Initialize SD Card ---
  if (!SD_MMC.begin()) {
    Serial.println("SD Card Mount Failed!");
    return;
  }
  Serial.printf("SD Card Size: %.2f GB\n", (float)SD_MMC.cardSize() / (1024 * 1024 * 1024));

  // Setup CPU frequency for maximum performance
  setCpuFrequencyMhz(240);
  Serial.println("CPU set to 240MHz for maximum performance");

  setupCamera();

  // --- Connect to WiFi with Static IP ---
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable WiFi sleep for better performance
  
  // Configure static IP
  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Static IP configuration failed!");
  }
  
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi with static IP ");
  Serial.print(staticIP);
  Serial.print("...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 50) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.println("Check your network settings and try again.");
    // You might want to restart or enter AP mode here
    return;
  }

  setupWebServer();
  server.begin();
  Serial.println("High-performance web server started.");
  Serial.printf("Access the camera at: http://%s\n", staticIP.toString().c_str());

  // Create camera task on Core 0 (separate from main loop on Core 1)
  xTaskCreatePinnedToCore(
    cameraTask,
    "CameraTask",
    4096,
    nullptr,
    2, // High priority
    &cameraTaskHandle,
    0  // Pin to Core 0
  );
}

void loop() {
  // Main recording logic (runs on Core 1)
  if (currentMode == MODE_RECORDING) {
    if (millis() - recordingStartTime >= segmentDuration) {
      Serial.println("Segment duration reached. Starting new file.");
      stopRecording();
      startRecording();
    }
    recordFrame();
  }
  
  // Update FPS counter
  unsigned long now = millis();
  if (now - lastFPSTime >= 1000) {
    currentFPS = frameCount;
    frameCount = 0;
    lastFPSTime = now;
  }
  
  vTaskDelay(1); // Minimal delay to prevent watchdog
}

void setupCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 24000000; // Increased to 24MHz for better performance
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  if (psramFound()) {
    config.frame_size = FRAMESIZE_CIF; // Default, will change dynamically
    config.jpeg_quality = 20; // Default, will change dynamically
    config.fb_count = 2; // Double buffering
    config.fb_location = CAMERA_FB_IN_PSRAM;
    Serial.println("PSRAM found - using optimized settings");
  } else {
    config.frame_size = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = 25;
    config.fb_count = 1;
    Serial.println("No PSRAM - using minimal settings");
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    ESP.restart();
  }
  
  // Initial sensor settings
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_framesize(s, config.frame_size);
    s->set_quality(s, config.jpeg_quality);
    
    // Optimize for speed by default
    s->set_brightness(s, 0);
    s->set_contrast(s, 2); // Higher contrast for better compression
    s->set_saturation(s, -1); // Lower saturation for better compression
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_ae_level(s, 0);
    s->set_aec_value(s, 200); // Faster exposure
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 5); // Higher gain for faster shutter
    s->set_gainceiling(s, (gainceiling_t)2);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 0); // Disable lens correction for speed
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 0); // Disable downsize for speed
    s->set_colorbar(s, 0);
    
    Serial.println("Camera initialized with default settings");
  }
}

void setupWebServer() {
  // Serve main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  // Stats endpoint
  server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request){
    float sdFreeGB = (float)(SD_MMC.cardSize() - SD_MMC.usedBytes()) / (1024.0 * 1024.0 * 1024.0);
    String json = "{";
    json += "\"mode\":\"" + getModeString() + "\",";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"fps\":" + String(currentFPS, 1) + ",";
    json += "\"sd_free_gb\":" + String(sdFreeGB, 2);
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // High-performance frame endpoint
  server.on("/frame", HTTP_GET, [](AsyncWebServerRequest *request){
    if (currentMode != MODE_STREAMING || !frameReady) {
      request->send(503, "text/plain", "Not ready");
      return;
    }
    
    if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      if (currentFrame && frameReady) {
        AsyncWebServerResponse *response = request->beginResponse_P(
          200, 
          "image/jpeg", 
          currentFrame->buf, 
          currentFrame->len
        );
        response->addHeader("Cache-Control", "no-cache");
        response->addHeader("Connection", "close");
        request->send(response);
        frameCount++;
        frameReady = false;
      } else {
        request->send(503, "text/plain", "No frame");
      }
      xSemaphoreGive(frameMutex);
    } else {
      request->send(503, "text/plain", "Busy");
    }
  });
  
  // Start streaming
  server.on("/stream/start", HTTP_GET, [](AsyncWebServerRequest *request){
    if(currentMode != MODE_IDLE) {
      request->send(409, "text/plain", "Device is busy.");
      return;
    }
    
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
      s->set_special_effect(s, 2); // Grayscale for black and white
      s->set_framesize(s, FRAMESIZE_QVGA); // Smaller resolution for higher FPS
      s->set_quality(s, 30); // Lower quality (higher compression) for smaller frames, higher FPS
      s->set_saturation(s, -2); // Minimize color processing
    }
    
    currentMode = MODE_STREAMING;
    streamActive = true;
    frameCount = 0;
    lastFPSTime = millis();
    frameReady = false;
    
    Serial.println("High-performance black and white streaming mode activated");
    request->send(200, "text/plain", "Streaming started.");
  });

  // Start recording
  server.on("/recording/start", HTTP_GET, [](AsyncWebServerRequest *request){
    if(currentMode != MODE_IDLE) {
      request->send(409, "text/plain", "Device is busy.");
      return;
    }
    
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
      s->set_special_effect(s, 0); // Color mode
      s->set_framesize(s, FRAMESIZE_VGA); // Higher resolution for quality
      s->set_quality(s, 10); // Higher quality (lower compression)
      s->set_saturation(s, 0); // Normal saturation for color
    }
    
    startRecording();
    request->send(200, "text/plain", "Recording started.");
  });

  // Stop all
  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    stopRecording();
    streamActive = false;
    currentMode = MODE_IDLE;
    frameReady = false;
    
    if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (currentFrame) {
        esp_camera_fb_return(currentFrame);
        currentFrame = nullptr;
      }
      xSemaphoreGive(frameMutex);
    }
    
    // Reset sensor to default
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
      s->set_special_effect(s, 0);
      s->set_framesize(s, FRAMESIZE_CIF);
      s->set_quality(s, 20);
    }
    
    Serial.println("All operations stopped");
    request->send(200, "text/plain", "Stopped.");
  });
}

void startRecording() {
  manageStorage();

  int videoFileNumber = 0;
  do {
    videoFileNumber++;
    sprintf(currentFileName, "/rec_%03d.mjpg", videoFileNumber);
  } while (SD_MMC.exists(currentFileName));

  videoFile = SD_MMC.open(currentFileName, FILE_WRITE);

  if (!videoFile) {
    Serial.println("Failed to open file for writing!");
    currentMode = MODE_IDLE;
    return;
  }

  currentMode = MODE_RECORDING;
  recordingStartTime = millis();
  Serial.printf("Recording started: %s\n", currentFileName);
}

void stopRecording() {
  if (currentMode == MODE_RECORDING && videoFile) {
    videoFile.close();
    Serial.printf("Recording saved: %s\n", currentFileName);
  }
  currentMode = MODE_IDLE;
}

void recordFrame() {
  if (!videoFile) return;

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;

  // Write raw JPEG frame to MJPEG file
  videoFile.write(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
}

void manageStorage() {
  uint64_t freeSpace = SD_MMC.cardSize() - SD_MMC.usedBytes();
  if (freeSpace < STORAGE_THRESHOLD) {
    Serial.println("Managing storage...");
    File root = SD_MMC.open("/");
    File file = root.openNextFile();
    char oldestFile[30] = "";
    int oldestFileNum = INT_MAX;
    
    while(file){
      String fileName = file.name();
      if (fileName.startsWith("rec_") && fileName.endsWith(".mjpg")) {
        int fileNum = fileName.substring(4, 7).toInt();
        if (fileNum < oldestFileNum) {
          oldestFileNum = fileNum;
          strcpy(oldestFile, file.name());
        }
      }
      file = root.openNextFile();
    }
    root.close();
    
    if (oldestFileNum != INT_MAX) {
      char fullPath[40];
      sprintf(fullPath, "/%s", oldestFile);
      Serial.printf("Deleted: %s\n", fullPath);
      SD_MMC.remove(fullPath);
    }
  }
}

String getModeString() {
  switch(currentMode) {
    case MODE_IDLE: return "IDLE";
    case MODE_STREAMING: return "STREAMING";
    case MODE_RECORDING: return "RECORDING";
    default: return "UNKNOWN";
  }
}