#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define analogPin_water A0
#define analogPin_light 34
#define relayPin_watersensor 13
#define relayPin_lightsensor 12
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define dht_pin 25      // Chân kết nối của cảm biến DHT22
#define dht_type DHT22 // Loại cảm biến DHT22
#define pump_pin 26
#define fan_pin 27

DHT dht(dht_pin, dht_type);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void watersensorTask(void *pvParameters);
void lightsensorTask(void *pvParameters);
void readDHTTask(void *pvParameters) ;

void setup() {
  Serial.begin(115200);
  pinMode(relayPin_watersensor, OUTPUT);
  pinMode(relayPin_lightsensor, OUTPUT);
  // Khởi tạo cảm biến DHT22
  dht.begin();
  pinMode(pump_pin, OUTPUT);
  pinMode(fan_pin, OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) 
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.display();
  delay(2000);
  display.clearDisplay();

  xTaskCreate(watersensorTask, "watersensor Task", 1000, NULL, 3, NULL);   
  xTaskCreate(lightsensorTask, "lightsensor Task", 1000, NULL, 2, NULL);  
  xTaskCreate(readDHTTask,"ReadDHTTask",2048,NULL,1,NULL);

}
// Hàm watersensorTask chứa nội dung của Task tương ứng
void watersensorTask(void *pvParameters) {
  while(1) {
    int sensorValue = analogRead(analogPin_water);
    int percent = map(sensorValue, 0, 4000, 0, 100);

    Serial.print("Water Level: ");
    Serial.print(sensorValue);
    Serial.print(" - Percentage: ");
    Serial.println(percent);

    if (percent < 40) {
      digitalWrite(relayPin_watersensor, HIGH);
    } else {
      digitalWrite(relayPin_watersensor, LOW);
    }

    vTaskDelay(pdMS_TO_TICKS(5000)); 
  }
} 

// Hàm lightsensorTask chứa nội dung của Task tương ứng
void lightsensorTask(void *pvParameters) {
  while(1){
    int sensorValue = analogRead(analogPin_light);
    int percent = 100 - map(sensorValue, 0, 4000, 0, 100);

    Serial.print("Light Level: ");
    Serial.print(sensorValue);
    Serial.print(" - Percentage: ");
    Serial.println(percent);

    if (percent < 20) {
      digitalWrite(relayPin_lightsensor, HIGH);
    } else {
      digitalWrite(relayPin_lightsensor, LOW);
    }

    vTaskDelay(pdMS_TO_TICKS(10000)); 
  }
}

// Hàm readDHTTask chứa nội dung của Task tương ứng
void readDHTTask(void *pvParameters) 
{
  (void) pvParameters;

  for(;;)
  {
    // Đọc nhiệt độ từ cảm biến DHT22
    float temperature = dht.readTemperature();
    // Đọc độ ẩm từ cảm biến DHT22
    float humidity = dht.readHumidity();
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,20);

    display.print("Temperature: ");
    display.print(temperature);
    display.print(" oC");
    display.println(" ");
    display.print("Humidity: ");
    display.print(humidity);
    display.println(" %");
    display.display();

    //so sánh nhiệt độ bật quạt & máy bơm
    if (temperature > 28)
    {
      digitalWrite(pump_pin, HIGH);
      digitalWrite(fan_pin, HIGH);
    }
    else
    {
      digitalWrite(pump_pin, LOW);
      digitalWrite(fan_pin, LOW);
    }
    // Delay để tránh việc đọc dữ liệu quá nhanh và gây quá tải
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void loop() {

}
