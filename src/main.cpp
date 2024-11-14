#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>  // Assuming you are using a B/W e-paper display

#define SD_CS 5  // Chip select pin for SD card
#define BUTTON_NEXT_PIN 12  // Button pin for next page
#define BUTTON_PREV_PIN 13  // Button pin for previous page

String CurrentBook = "Beyond-Order"; // Can be changed to other books dynamically
String CurrentBookjson = "Beyond-Order.json"; // Can be changed to other books dynamically

// Define your e-paper display
GxEPD2_BW<GxEPD2_213_B72, GxEPD2::Orientation::Portrait> display(GxEPD2_213_B72(15, 2, 4, 16));  // Example for 2.13" display

// Global variables
int currentPage = 0;  // Track the current real page
int totalPages = 0;   // Total number of real pages in the book
File sdFile;
StaticJsonDocument<2048> jsonDoc;  // Adjust size based on your JSON file size

// Page display size and content
int linesPerPage = 10;  // Adjust based on the display size and text line height
int charsPerLine = 30;  // Number of characters per line for content wrapping

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PREV_PIN, INPUT_PULLUP);

    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        Serial.println("SD card initialization failed!");
        return;
    }
    Serial.println("SD card initialized.");

    // Load the Book data from the JSON file
    if (loadBookData()) {
        display.begin();
        display.setRotation(1);  // Adjust the display orientation if needed
        loadProgress();  // Load last read page
        showPage(currentPage);  // Display the first page
    } else {
        Serial.println("Failed to load Book data.");
    }

    // Set wake-up source (GPIO pin) and enter deep sleep
    esp_sleep_enable_ext0_wakeup(BUTTON_PREV_PIN || BUTTON_NEXT_PIN, LOW);  // Wake up on button press (LOW)
    Serial.println("Entering deep sleep mode...");
    delay(1000);  // Allow time for the message to be printed
    esp_deep_sleep_start();  // Start deep sleep
}

void loop() {
    // The loop won't run because the ESP32 is in deep sleep after setup
    // It will wake up from deep sleep and run the setup again when a button is pressed
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
