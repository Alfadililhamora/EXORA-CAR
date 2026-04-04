#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// --- KONFIGURASI JARINGAN ---
const char* ssid = "GG Kost";
const char* password = "Gezakost";

// --- PIN AI-THINKER ---
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Handler untuk data JSON status
esp_err_t status_handler(httpd_req_t *req){
    static char json_response[128];
    int rssi = WiFi.RSSI();
    uint32_t uptime = millis() / 1000;
    sprintf(json_response, "{\"rssi\":%d,\"uptime\":%u,\"vcc\":3.31}", rssi, uptime);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

// Handler Streaming - Dioptimalkan untuk Kecepatan
esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char * part_buf[64];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    if(res != ESP_OK) return res;

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            size_t hlen = snprintf((char *)part_buf, 64, "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            if(res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
            esp_camera_fb_return(fb);
        }
        if(res != ESP_OK) break;
    }
    return res;
}

// Interface Web (OSD FPV)
esp_err_t index_handler(httpd_req_t *req){
    const char* html = "<!DOCTYPE html><html><head>"
      "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=0'>"
      "<style>"
      "body{margin:0;padding:0;background:#000;width:100vw;height:100vh;overflow:hidden;font-family:monospace;color:#0f0;}"
      ".container{position:relative;width:100vw;height:100vh;background:#111;}"
      "img{width:100vw;height:100vh;object-fit:contain;z-index:1;}"
      ".osd{position:absolute;top:0;left:0;width:100%;height:100%;z-index:10;pointer-events:none;text-shadow:2px 2px #000;}"
      ".corner{position:absolute;padding:20px;font-size:16px;}"
      ".top-left{top:0;left:0;}"
      ".top-right{top:0;right:0;text-align:right;}"
      ".bottom-left{bottom:0;left:0;}"
      ".bottom-right{bottom:0;right:0;text-align:right;}"
      ".blink{animation:blink 1s infinite;} @keyframes blink{0%{opacity:0}50%{opacity:1}100%{opacity:0}}"
      ".crosshair{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);font-size:30px;color:rgba(0,255,0,0.5);}"
      "</style></head><body>"
      "<div class='container'>"
        "<img src='/stream'>"
        "<div class='osd'>"
          "<div class='corner top-left'>EXORA-SAR<br>SYS: OK</div>"
          "<div class='corner top-right'>RSSI: <span id='rssi'>-</span> dBm</div>"
          "<div class='corner bottom-left'><span class='blink' style='color:red'>● REC</span><br>UP: <span id='timer'>0</span>s</div>"
          "<div class='corner bottom-right'>BAT: <span id='vcc'>3.3</span>V<br>MODE: FPV_HIGH</div>"
          "<div class='crosshair'>+</div>"
        "</div>"
      "</div>"
      "<script>"
        "setInterval(function(){"
          "fetch('/status').then(r=>r.json()).then(d=>{"
            "document.getElementById('rssi').innerText=d.rssi;"
            "document.getElementById('timer').innerText=d.uptime;"
          "}).catch(e=>{});"
        "}, 2000);"
      "</script></body></html>";
    return httpd_resp_send(req, html, strlen(html));
}

void setup() {
  Serial.begin(115200);
  
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

  // --- SETINGAN PERFORMA TINGGI ---
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA; // 320x240 - Paling seimbang untuk low-latency
    config.jpeg_quality = 12;           // Kualitas tinggi (0-63, makin rendah makin tajam)
    config.fb_count = 2;                // Double buffering (Wajib untuk FPS tinggi)
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1); 
  
  // --- KONEKSI GG KOST ---
  WiFi.begin(ssid, password);
  Serial.print("Connecting to GG Kost");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("Stream Link: http://");
  Serial.println(WiFi.localIP());

  // --- START SERVER ---
  httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
  server_config.server_port = 80;
  server_config.ctrl_port = 32768;
  
  httpd_handle_t server = NULL;
  if (httpd_start(&server, &server_config) == ESP_OK) {
    httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };
    httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = status_handler };
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
    httpd_register_uri_handler(server, &status_uri);
  }
}

void loop() {
  // Biarkan kosong agar CPU fokus pada streaming (background task)
  delay(10); 
}
