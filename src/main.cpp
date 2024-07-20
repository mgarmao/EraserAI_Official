#include <Arduino.h>
#include <Base64.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16


#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================

const char* ssid = "";
const char* password = "";

//Add a second wifi option to connect to
const char* ssid2 = "";
const char* password2 = "";






const char* apiEndpoint = "https://api.openai.com/v1/chat/completions";

//=====
//YOUR OPENAI API KEY
//=====
String api_key = "";



void scrolltext(String text, int displayTime);
void displaytext(String text, int displayTime);

void photo_save(const char * fileName);

String convertFile(const char *fileName);

void photo_save(const char * fileName);
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len);

unsigned long lastCaptureTime = 0; // Last shooting time
int imageCount = 1;                // File Counter
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status

int button = 44;

void setup() {
  Serial.begin(115200);

  pinMode(button,INPUT);

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
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_XGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
    Serial.print("NO DISPLAY");
  }
  
  display.display();
  delay(50); // Pause for 2 seconds
  display.clearDisplay();
  display.display();


  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } 
  else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
    #if CONFIG_IDF_TARGET_ESP32S3
        config.fb_count = 2;
    #endif
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    displaytext("No CAM",2000);
    return;
  }
  
  camera_sign = true; // Camera initialization check passes

  // Initialize SD card
  if(!SD.begin(21)){
    Serial.println("Card Mount Failed");
    displaytext("No SD",2000);
    return;
  }
  uint8_t cardType = SD.cardType();

  // Determine if the type of SD card is available
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    displaytext("No SD",2000);
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  sd_sign = true; // sd initialization check passes

  int startTime = millis();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED&&startTime+10000>=millis()) {
    delay(1000);
    displaytext("Cncting WIFI",10);
    Serial.print(".");
  }
  
  if(WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();

    Serial.println("option 2");
    WiFi.begin(ssid2, password2);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      displaytext("Cncting WIFI",10);
      Serial.print(".");
    }
  }

  Serial.println(" Connected!");
  displaytext("Connected!",1000);

  Serial.println("Photos will begin in one minute, please be ready.");
}

void loop() {
  if(camera_sign && sd_sign){

    displaytext("ready",200);
    byte buttonState = digitalRead(button);
      
    if (String(buttonState) =="0") {
      Serial.println("Button is pressed");
      displaytext("Taking Pic",500);

      char filename[32];
      sprintf(filename, "/image1.jpg");
      photo_save(filename);
      Serial.printf("Saved picture: %s\r\n", filename);  
      
      String base64Image = convertFile("/image1.jpg");

      displaytext("COMPILNG",10);
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(apiEndpoint); // Specify the URL

        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", String("Bearer ") + api_key);

        Serial.println("JSONString: ");
        delay(100);
        String sendingJson = "{\"model\":\"gpt-4o\",\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"Solve this problem. Return with only the answer.\"},{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/jpeg;base64,"+base64Image;
        String newJson = sendingJson+"\",\"detail\": \"high\"}}]}],\"max_tokens\": 900}";
        Serial.println(newJson);
        // Send POST request
        int httpResponseCode = http.POST(newJson);
        displaytext("SENT",10);

        String answer = "error";
        if (httpResponseCode > 0) {
          String response = http.getString(); // Get the response
          Serial.println(httpResponseCode); // Print the response code
          Serial.println(response); // Print the response

          JsonDocument doc;
          DeserializationError error = deserializeJson(doc, response);
          
          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            displaytext("JSON Error",1000);
            return;
          }

          JsonObject choices_0 = doc["choices"][0];
          const char* choices_0_message_content = choices_0["message"]["content"]; // "310 N"

          answer = choices_0_message_content;
        } 
        else {
          Serial.print("Error on HTTP request: ");
          Serial.println(httpResponseCode);
          displaytext("HTTP Error",1000);
        }

        Serial.print("Answer: ");
        Serial.println(answer);
        displaytext(answer,5000);

        http.end();
      }
    }
    Serial.println();
  }
}

void scrolltext(String text, int displayTime) {
  display.clearDisplay();

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(text);
  display.display();      // Show initial text
  delay(100);

  // Scroll in various directions, pausing in-between:
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(2000);
  display.stopscroll();
  delay(1000);
}

void displaytext(String text, int displayTime) {
  display.clearDisplay();

  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(text);
  display.display();      // Show initial text
  delay(displayTime);
}

String convertFile(const char *fileName){
  File imageFile = SD.open(fileName, FILE_READ);
  if(!imageFile) {
    Serial.println("Failed to open file!");
    return "";
  }
  else{
    // Get the size of the file
    long fileSize = imageFile.size();
    Serial.print("File size: ");
    Serial.println(fileSize);

    // Allocate memory to store the file data
    byte *fileData = (byte *)malloc(fileSize);
    if (fileData == NULL) {
      Serial.println("Failed to allocate memory!");
      return "";
    }

    // Read the file data into the buffer
    imageFile.read(fileData, fileSize);

    // Close the file
    imageFile.close();

    // Calculate the base64 encoded size
    int encodedLength = Base64.encodedLength(fileSize);

    // Allocate memory for the base64 encoded data
    char *base64Data = (char *)malloc(encodedLength + 1);
    if (base64Data == NULL) {
      Serial.println("Failed to allocate memory for base64 data!");
      free(fileData);
      return "";
    }
    else{

      // Encode the file data to base64
      Base64.encode(base64Data, (char *)fileData, fileSize);

      // Null-terminate the base64 string
      base64Data[encodedLength] = '\0';

      // Print the base64 encoded data
      return base64Data;
    }
  }
}

// Save pictures to SD card
void photo_save(const char * fileName) {
  // Take a photo
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get camera frame buffer");
    return;
  }
  // Save photo to file
  writeFile(SD, fileName, fb->buf, fb->len);
  
  // Release image buffer
  esp_camera_fb_return(fb);

  Serial.println("Photo saved to file");
}


void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.write(data, len) == len){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}