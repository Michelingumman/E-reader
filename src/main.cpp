#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <ArduinoJson.h>
#include <GxEPD2_BW.h> 
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <vector>


#define SD_CS 5             // Chip select pin for SD card
#define NEXT_BUTTON 12      // Button pin for next page
#define PREV_BUTTON 13      // Button pin for previous page
#define BATTERY_PIN 15      // analog pin for reaing the battery voltage
#define PRESSED LOW         // Input is active low, so "pressed" is intuitive
#define SECONDS_10 10000    // 10 seconds delay before deep sleep


String CurrentBook = "Beyond-Order"; // Can be changed to other books dynamically
String CurrentBookjson = "Beyond-Order.json"; // Can be changed to other books dynamically

// Define your e-paper display
GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display(GxEPD2_290(/*CS=*/ SD_CS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));



// Global variables
File sdFile;
DynamicJsonDocument jsonDoc;    // Adjust size based on your JSON file size
std::vector<String> pageBuffer; // Buffer to store 10 pages at a time

int bufferStart = 0;            // Starting page number of the current buffer
int bufferEnd = 9;              // Ending page number of the current buffer (inclusive)

int currentPage = 0;            // Track the current real page
int totalPages = 0;             // Total number of real pages in the book (from JSON)

int linesPerPage = 10;          // Adjust based on the display size and text line height
int charsPerLine = 30;          // Number of characters per line for content wrapping

unsigned long lastInteractionTime = 0;



// ------------ Function declarations ------------
bool loadBookData(void);
void loadBuffer(int startPage);
void showPage(int pageNum);
int calculateLines(String content);
void nextPage(void);
void prevPage(void);
void saveProgess(void);
void loadProgress(void);
int batteryLevel(void);
//  ---------- END of Function declartations -----------


void setup() {
    Serial.begin(115200);
    pinMode(NEXT_BUTTON, INPUT_PULLUP);
    pinMode(PREV_BUTTON, INPUT_PULLUP);


    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        Serial.println("SD card initialization failed!");
        return;
    }
    Serial.println("SD card initialized.");
    display.init();
    display.setRotation(1);

    // Load the Book data from the JSON file
    if (loadBookData()) {
        loadProgress();  // Load last read page
        showPage(currentPage);  // Display the first page
    }

    // Record the time of the last interaction
    lastInteractionTime = millis();
    setCpuFrequencyMhz(20);
}


void loop() {
    // Check for button presses
    if (digitalRead(NEXT_BUTTON) == PRESSED) { // debounce is taken care of via hardware cap
        nextPage();
        lastInteractionTime = millis();  // Reset sleep timer
    }

    if (digitalRead(PREV_BUTTON) == PRESSED) { // debounce is taken care of via hardware cap
        prevPage();
        lastInteractionTime = millis();  // Reset sleep timer
    }

    // Check if it's time to sleep
    if (millis() - lastInteractionTime >= SECONDS_10) {
        Serial.println("Entering deep sleep mode...");
        esp_sleep_enable_ext1_wakeup((1ULL << NEXT_BUTTON) | (1ULL << PREV_BUTTON), ESP_EXT1_WAKEUP_ANY_LOW); // LOw since active LOW
        esp_light_sleep_start();
    }
}


// Load Book data from the SD card (JSON file)
bool loadBookData() {
    sdFile = SD.open(CurrentBookjson);


    if (!sdFile) {
        Serial.println("Failed to open Book data file.");
        return false;
    }

    // Read the JSON file into the document
    DeserializationError error = deserializeJson(jsonDoc, sdFile);
    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    // Calculate the total number of pages
    totalPages = jsonDoc[CurrentBook].size();  // Get the number of pages
    sdFile.close();
    return true;
}



// Load pages into buffer
void loadBuffer(int startPage) {
    bufferStart = startPage;
    bufferEnd = min(startPage + 9, totalPages - 1);  // Ensure we don't exceed total pages

    pageBuffer.clear();  // Clear the old buffer

    for (int i = bufferStart; i <= bufferEnd; i++) {
        // Fetch content from JSON and add it to the buffer
        String pageContent = jsonDoc[CurrentBook][i]["content"].as<String>();
        pageBuffer.push_back(pageContent);
    }

    Serial.print("Loaded buffer: Pages ");
    Serial.print(bufferStart);
    Serial.print(" to ");
    Serial.println(bufferEnd);
}


// Display the current page content and page number
void showPage(int pageNum) {
    // Ensure pageNum is within valid range
    if (pageNum < 0 || pageNum >= totalPages) return;

    String pageContent = jsonDoc[CurrentBook][pageNum]["content"].as<String>();
    int numLines = calculateLines(pageContent);

    // Clear the screen
    display.fillScreen(GxEPD_WHITE);

    // Display the content
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);  // Adjust text size as needed
    display.setCursor(10, 10);

    // Display the content, broken into lines
    int yPos = 10;
    for (int i = 0; i < numLines; i++) {
        String line = pageContent.substring(i * charsPerLine, (i + 1) * charsPerLine);
        display.setCursor(10, yPos);
        display.print(line);
        yPos += 16;  // Adjust line height
    }

    // Display the current page number at the bottom
    display.setCursor(10, display.height() - 20);  // Position the page number at the bottom
    display.print("Page: ");
    display.print(pageNum + 1);  // Display real page number

    // Update the display
    display.display();
    Serial.print("Showing page: ");
    Serial.println(pageNum + 1);
}

// Calculate how many lines of content fit in the current page
int calculateLines(String content) {
    return (content.length() / charsPerLine) + 1;  // Number of lines based on content length
}

// Go to the next page
void nextPage() {
    if (currentPage < totalPages - 1) {
        currentPage++;
        showPage(currentPage);
        saveProgress();
    }
}

// Go to the previous page
void prevPage() {
    if (currentPage > 0) {
        currentPage--;
        showPage(currentPage);
        saveProgress();
    }
}

// Save the current page number to a file
void saveProgress() {
    File progressFile = SD.open("/progress.txt", FILE_WRITE);
    if (progressFile) {
        progressFile.print(currentPage);
        progressFile.close();
        Serial.print("Saved current page: ");
        Serial.println(currentPage);
    } else {
        Serial.println("Failed to save progress.");
    }
}

// Load the last read page from file
void loadProgress() {
    File progressFile = SD.open("/progress.txt");
    if (progressFile) {
        currentPage = progressFile.parseInt();
        progressFile.close();
        Serial.print("Loaded last page: ");
        Serial.println(currentPage);
    } else {
        Serial.println("No progress file found, starting from the beginning.");
    }
}

int batteryLevel(){
    // Voltage divider: +B ---[ 100k ]--- Pin ---[ 100k ]--- GND
    //   Pin = +B/2
    int level = 2 * analogRead(BATTERY_PIN) / 4096; // 0 - 3.3 will represent half the battery voltage bcause of the voltage divider
    float percentage = level / 3.3;
    return int(percentage);
}
