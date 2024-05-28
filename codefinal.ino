#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <semphr.h> // Include FreeRTOS semaphore library

#define analogPin_water A0
#define analogPin_light 34
const int relayPin_watersensor = 13;
const int relayPin_lightsensor = 12;
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define dht_pin 25      // Chân kết nối của cảm biến DHT22
#define dht_type DHT22  // Loại cảm biến DHT22
#define pump_pin 26
#define fan_pin 27

DHT dht(dht_pin, dht_type);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct DHTData {
  float temperature;
  float humidity;
};

QueueHandle_t queueSensor;
QueueHandle_t dhtQueue;
SemaphoreHandle_t waterSemaphore; // Declare semaphore handle
SemaphoreHandle_t dhtMutex; // Declare mutex handle

void watersensorTask(void *pvParameters);
void lightsensorTask(void *pvParameters);
void readDHTTask(void *pvParameters);
void displayDHTTask(void *pvParameters);

void setup() {
  Serial.begin(115200);
  pinMode(relayPin_watersensor, OUTPUT);
  pinMode(relayPin_lightsensor, OUTPUT);
  // Khởi tạo cảm biến DHT22
  dht.begin();
  pinMode(pump_pin, OUTPUT);
  pinMode(fan_pin, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.display();
  delay(2000);
  display.clearDisplay();

  queueSensor = xQueueCreate(120, sizeof(int)); // Correct type size
  dhtQueue = xQueueCreate(100, sizeof(DHTData)); // Instantiate dhtQueue
  waterSemaphore = xSemaphoreCreateBinary(); // Create semaphore
  dhtMutex = xSemaphoreCreateMutex(); // Create mutex
  xSemaphoreGive(waterSemaphore); // Initialize semaphore to be available

  if (queueSensor != NULL && dhtQueue != NULL && waterSemaphore != NULL && dhtMutex != NULL) {
    xTaskCreate(watersensorTask, "watersensor Task", 1000, NULL, 3, NULL);
    xTaskCreate(lightsensorTask, "lightsensor Task", 1000, NULL, 2, NULL);
    xTaskCreate(readDHTTask, "ReadDHTTask", 2048, NULL, 1, NULL);
    xTaskCreate(displayDHTTask, "DisplayDHTTask", 2048, NULL, 1, NULL);
  }
}
// Hàm watersensorTask chứa nội dung của Task tương ứng
void watersensorTask(void *pvParameters) {
  while (1) {
    int sensorValue = analogRead(analogPin_water);
    int percent = map(sensorValue, 0, 4000, 0, 100);

    Serial.print("Water Level: ");
    Serial.print(sensorValue);
    Serial.print(" - Percentage: ");
    Serial.println(percent);

    if (percent < 15) {
      if (xSemaphoreTake(waterSemaphore, portMAX_DELAY) == pdTRUE) {
        digitalWrite(relayPin_watersensor, HIGH);
        Serial.println("Water system activated.");
        xSemaphoreGive(waterSemaphore); // Release semaphore
      }
    } else {
      if (xSemaphoreTake(waterSemaphore, portMAX_DELAY) == pdTRUE) {
        digitalWrite(relayPin_watersensor, LOW);
        Serial.println("Water system deactivated.");
        xSemaphoreGive(waterSemaphore); // Release semaphore
      }
    }

    // Send relay state to queue
    if (xQueueSend(queueSensor, &relayPin_watersensor, portMAX_DELAY) != pdPASS) {
      Serial.println("Water: Queue is full.");
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

// Hàm lightsensorTask chứa nội dung của Task tương ứng
void lightsensorTask(void *pvParameters) {
  while (1) {
    int sensorValue = analogRead(analogPin_light);
    int percent = 100 - map(sensorValue, 0, 4000, 0, 100);

    Serial.print("Light Level: ");
    Serial.print(sensorValue);
    Serial.print(" - Percentage: ");
    Serial.println(percent);

    if (percent < 30) {
      digitalWrite(relayPin_lightsensor, HIGH);
    } else {
      digitalWrite(relayPin_lightsensor, LOW);
    }

    // Send relay state to queue
    if (xQueueSend(queueSensor, &relayPin_lightsensor, portMAX_DELAY) != pdPASS) {
      Serial.println("Light: Queue is full.");
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

// Hàm readDHTTask chứa nội dung của Task tương ứng
void readDHTTask(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    // Attempt to take the mutex before accessing the DHT sensor
    if (xSemaphoreTake(dhtMutex, portMAX_DELAY) == pdTRUE) {
      // Đọc nhiệt độ và độ ẩm từ cảm biến DHT22
      float temperature = dht.readTemperature();
      float humidity = dht.readHumidity();

      // Tạo cấu trúc DHTData
      DHTData data;
      data.temperature = temperature;
      data.humidity = humidity;

      // Gửi dữ liệu vào Queue
      if (xQueueSend(dhtQueue, &data, portMAX_DELAY) != pdPASS) {
        Serial.println("Failed to send data to queue");
      }

      //so sánh nhiệt độ bật quạt & máy bơm
      if (temperature > 33) {
        digitalWrite(pump_pin, HIGH);
        digitalWrite(fan_pin, HIGH);
      } else {
        digitalWrite(pump_pin, LOW);
        digitalWrite(fan_pin, LOW);
      }

      Serial.print("Temperature: ");
      Serial.print(data.temperature);
      Serial.print(" oC");
      Serial.println(" ");
      Serial.print("Humidity: ");
      Serial.print(data.humidity);
      Serial.println(" %");

      // Release the mutex after accessing the DHT sensor
      xSemaphoreGive(dhtMutex);
    }

    // Delay để tránh việc đọc dữ liệu quá nhanh và gây quá tải
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// Task to display DHT data
void displayDHTTask(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    DHTData data;

    // Nhận dữ liệu từ Queue
    if (xQueueReceive(dhtQueue, &data, portMAX_DELAY) == pdPASS) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 20);

      display.print("Temperature: ");
      display.print(data.temperature);
      display.print(" oC");
      display.println(" ");
      display.print("Humidity: ");
      display.print(data.humidity);
      display.println(" %");
      display.display();
    }
  }
}

void loop() {
  // Không cần nội dung trong loop() khi sử dụng FreeRTOS
}












