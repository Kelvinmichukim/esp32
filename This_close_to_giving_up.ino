#include "Arduino.h"
#include "OV2640.h"   //Camera
#include "WiFi.h"
#include "esp_http_client.h"   
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <Wire.h>
#include <ThingSpeak.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPL3115A2.h>
#include <Firebase_ESP_Client.h>
#include "s300i2c.h"
//Provide the token generation process info.
#include <addons/TokenHelper.h>
#include <EEPROM.h>      //read and write from flash memory

//Replace with your network credentials
const char* ssid = "MASHARRIA";
const char* password = "machungwa";

Adafruit_MPL3115A2 baro = Adafruit_MPL3115A2(); //ThingSpeak
const long CHANNEL_ID = 2215290;
const char *WRITE_API_KEY = "QW5Z08BODJVO5ANI";

// Insert Firebase project API Key
#define API_KEY "AIzaSyDQmu20j_jwQZ7cOub_0-EV7QogZTNQjuE"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "mashariabenjamin@gmail.com"
#define USER_PASSWORD "mashaben123"

// Insert Firebase storage bucket ID
#define STORAGE_BUCKET_ID "esp32-camera-42500.appspot.com"

//OV2640 camera module pins (CAMERA_MODEL_TTGO_T_CAMERA)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 32
#define SIOD_GPIO_NUM 13
#define SIOC_GPIO_NUM 12
#define Y9_GPIO_NUM 39
#define Y8_GPIO_NUM 36
#define Y7_GPIO_NUM 23
#define Y6_GPIO_NUM 18
#define Y5_GPIO_NUM 15
#define Y4_GPIO_NUM 4
#define Y3_GPIO_NUM 14
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 27
#define HREF_GPIO_NUM 25
#define PCLK_GPIO_NUM 19

//PIN DEFINITIONS
#define PIR_PIN  33
#define OLED_ADDR 0x3C
#define OLED_SDA 21
#define OLED_SCL 22
#define S300_SCL_GPIO_NUM 21
#define S300_SDA_GPIO_NUM 22

// Photo File Name to save in LittleFS
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/data/photo.jpg"

#define EEPROM_SIZE 4

int photoCounter = 0;  // Counter for incremental photos

Adafruit_SSD1306 display(128, 64, &Wire, OLED_ADDR); //OLED

//Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;
 
S300I2C s3(Wire);
unsigned int co2;

void initWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  } else {
    delay(1000);
    Serial.println("LittleFS mounted successfully");
  }
}

  void initCamera() {
 // OV2640 camera module
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
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
//Define image capture increment
uint8_t increment = 0;

// Camera init
esp_err_t err = esp_camera_init(&config);
if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  } 
}

bool taskCompleted = false;

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS(void)
{
  // Dispose first pictures because of bad quality
  camera_fb_t* fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++)
  {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }
  // Take a new photo
   fb = NULL;
  fb = esp_camera_fb_get();  
  if (!fb) 
  {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  // Photo file name
  char fileNumber[15]; // Allocate space for 4 digits (max 9999)
  sprintf(fileNumber, "%04d", photoCounter); // Format the counter with leading zeros
  String fileName = FILE_PHOTO_PATH + String(fileNumber) + ".jpg";
  String bucketName = BUCKET_PHOTO + String(fileNumber) + ".jpg";
  File file = LittleFS.open(fileName, FILE_WRITE);
  

  // Insert the data in the photo file
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(fileName);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }

  // Close the file
  file.close();
  esp_camera_fb_return(fb);

   // Print the file name
  Serial.println("Updating picture filename: " + fileName);
  
}

void initOLED() 
{
  // Initialize OLED display here
    display.display();
     display.clearDisplay();
      display.drawPixel(10, 10, SSD1306_WHITE);
  
}

 //Function to Send CO2 data to ThingSpeak  
void sendCO2ToThingSpeak() {
  int co2Reading = analogRead(S300_SCL_GPIO_NUM & S300_SDA_GPIO_NUM);

  // Write CO2 sensor reading to ThingSpeak field 2
  ThingSpeak.setField(2, co2Reading);

  // Send data to ThingSpeak
  int response = ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);

  if (response == 200) 
  {
    Serial.println("CO2 Reading sent to ThingSpeak successfully");
  }
  else 
  {
    Serial.println("Failed to send CO2 Reading to ThingSpeak");
  }
  // Optional delay before sending next update
  delay(15000); // 15-second delay between updates (adjust as needed)
}

//Function to send PIR data to ThingSpeak
void sendPIRToThingSpeak()
{ 
  int pirStatus = digitalRead(PIR_PIN);
  // Write PIR sensor status to ThingSpeak field 1
  ThingSpeak.setField(1, pirStatus);
  // Send data to ThingSpeak
  int response = ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);
  if (response == 200) 
  {
    Serial.println("Data sent to ThingSpeak successfully"); 
  }
  else
  {
    Serial.println("Failed to send data to ThingSpeak");
  }
  // Optional delay before sending next update
  delay(15000); // 15-second delay between updates (adjust as needed)
}
  
void setup() {
  // Initialize components
  Wire.begin();
  Serial.begin(115200);
  
   // Initialize camera, OLED, CO2 sensor, PIR, and WiFi
  initCamera();
  initWiFi();
  S300I2C s3(Wire);
   s3.begin(S300I2C_ADDR);
   Serial.println("START S300I2C");
  delay(10000); // 10sec wait.
  
//Firebase
// Assign the api key
configF.api_key = API_KEY;
//Assign the user sign in credentials
auth.user.email = USER_EMAIL;
auth.user.password = USER_PASSWORD;

//Assign the callback function for the long running token generation task
configF.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

Firebase.begin(&configF, &auth);
Firebase.reconnectWiFi(true);
Firebase.begin(&configF, &auth);

}

void loop() {
   if (digitalRead(PIR_PIN))
 { 
    // Read CO2 sensor
    s3.begin(S300I2C_ADDR);
    
    // Capture image 
    if (takeNewPhoto) {
    capturePhotoSaveLittleFS();
    takeNewPhoto = false;
  }
    increment++;
 }

// Display motion detected and CO2 reading on OLED
if (digitalRead(PIR_PIN))
  { // Check if motion is detected by the PIR sensor
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.println("Motion detected!");
    display.display();
    delay(1000); // Wait for a second to stabilize motion detection

unsigned int co2;
co2 = s3.getCO2ppm();
Serial.print("CO2=");
Serial.println(co2);
delay(30000); // 30 sec wait

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.print("C02=");
  display.println(co2);
  display.display();
  delay(1000);
     }
     
  else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.println("No motion detected!");
    display.display();
  }
   
// Call function to send PIR sensor data to ThingSpeak
sendPIRToThingSpeak();

// Call function to send CO2 readings to ThingSpeak
sendCO2ToThingSpeak();

 // Wait for 5 seconds before next loop iteration
 delay(5000);

//Capture Image and Prepare Firebase to accept image

  delay(1);
  if (Firebase.ready() && !taskCompleted){
    taskCompleted = true;
    Serial.print("Uploading picture... ");

    //MIME type should be valid to avoid the download problem.
    //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO_PATH /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, BUCKET_PHOTO /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */,fcsUploadCallback)){
      Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
    }
    else{
      Serial.println(fbdo.errorReason());
    }
  }
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info){
    if (info.status == firebase_fcs_upload_status_init){
        Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
    }
    else if (info.status == firebase_fcs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_upload_status_complete)
    {
        Serial.println("Upload completed\n");
        FileMetaInfo meta = fbdo.metaData();
        Serial.printf("Name: %s\n", meta.name.c_str());
        Serial.printf("Bucket: %s\n", meta.bucket.c_str());
        Serial.printf("contentType: %s\n", meta.contentType.c_str());
        Serial.printf("Size: %d\n", meta.size);
        Serial.printf("Generation: %lu\n", meta.generation);
        Serial.printf("Metageneration: %lu\n", meta.metageneration);
        Serial.printf("ETag: %s\n", meta.etag.c_str());
        Serial.printf("CRC32: %s\n", meta.crc32.c_str());
        Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
        Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    }
    else if (info.status == firebase_fcs_upload_status_error){
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}
