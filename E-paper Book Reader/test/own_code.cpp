#include <Arduino.h>
#include <Wire.h>
#include "SdFat.h"
#include "SPI.h"
#include <string.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <ArduinoJson.h>
#include <GxEPD2_BW.h> 
#include <GxEPD2_3C.h>
#include <vector>


#define SD_CS 5             // Chip select pin for SD card


//from GxEPD2_BW.h

// mapping of Waveshare ESP32 Driver Board
// BUSY -> 25, RST -> 26, DC -> 27, CS-> 15, CLK -> 13, DIN -> 14
// NOTE: this board uses "unusual" SPI pins and requires re-mapping of HW SPI to these pins in SPIClass
//       see example GxEPD2_WS_ESP32_Driver.ino, it shows how this can be done easily





#define CS 15
#define DC 27
#define RST 26
#define BUSY 25
#define CLK 13

#define NEXT_BUTTON 12      // Button pin for next page
#define PREV_BUTTON 13      // Button pin for previous page
#define MENU_BUTTON 14      // button pin for menu

#define BATTERY_PIN 15      // analog pin for reaing the battery voltage
#define FULL_BATTERY_VOLTAGE 4.7  // specific battery full voltage 

#define SECONDS_10 10000    // 10 seconds delay before deep sleep

#define PRESSED LOW         // Input is active low, so "pressed" is intuitive

#define SCREEN_WIDTH 480    // default screen width in portrait
#define SCREEN_HEIGHT 648   // default screen height in portrait


#define USE_HSPI_FOR_EPD

#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
SPIClass hspi(HSPI);
#endif


// Define your e-paper display
#define GxEPD2_DRIVER_CLASS GxEPD2_583c_GDEQ0583Z31 // GDEW0583Z83 648x480, EK79655 (GD7965), (WFT0583CZ61)
GxEPD2_3C<GxEPD2_583c_GDEQ0583Z31, GxEPD2_583c_GDEQ0583Z31::HEIGHT / 2> display(GxEPD2_583c_GDEQ0583Z31(CS, DC, RST, BUSY)); // GDEQ0583Z31 648x480, UC8179C
// GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display(GxEPD2_290(CS, DC, RST, BUSY));


// Global variables
SdFat sd;
SdFile openedBook;
DynamicJsonDocument jsonDoc(4098);    // Adjust size based on your JSON file size
std::vector<String> pageBuffer; // Buffer to store 10 pages at a time

int screenHeight = SCREEN_HEIGHT; //default 
int screenWidth = SCREEN_WIDTH; // default
bool isLandscape = false;       //track orientation

int bufferStart = 0;            // Starting page number of the current buffer
int bufferEnd = 9;              // Ending page number of the current buffer (inclusive)

int currentPage = 0;            // Track the current real page
int totalPages = 0;             // Total number of real pages in the book (from JSON)

int textSize = 2;               // Default text size
int linesPerPage = 10;          // Adjust based on the display size and text line height
int charsPerLine = 30;          // Number of characters per line for content wrapping

unsigned long lastInteractionTime = 0;

// books
String CurrentBook = "Beyond-Order"; // Can be changed to other books dynamically
String CurrentBookjson = "Beyond-Order.json"; // Can be changed to other books dynamically

bool isMenuActive = false;
int selectedMenuOption = 0;
// Menu options
String menuOptions[] = {"Select Book", "Text Size: ", "Exit Menu"};
int numMenuOptions = sizeof(menuOptions) / sizeof(menuOptions[0]);

// ------------ Function declarations ------------
void screenLayout();
void toggleLayout();

void displayMenu();
void executeMenuOption();
void navigateMenu();
void selectBookMenu();
bool loadBookData();
void loadBuffer(int startPage);
void loadProgress();

void showPage(int pageNum);
void nextPage();
void prevPage();

int calculateLines(String content);
void saveProgess();

void showbatteryLevel();
//  ---------- END of Function declartations -----------


void setup() {
    Serial.begin(9600);
    while (!Serial){};
    delay(500);

    pinMode(NEXT_BUTTON, INPUT_PULLUP);
    pinMode(PREV_BUTTON, INPUT_PULLUP);
    pinMode(MENU_BUTTON, INPUT_PULLUP);
    pinMode(BATTERY_PIN, INPUT);


    // Initialize SD card
    if (!sd.begin(SD_CS, SPI_FULL_SPEED)) sd.initErrorHalt();
    
    Serial.println("SD card initialized.");
    
    hspi.begin(13, 12, 14, 15); // remap hspi for EPD (swap pins)
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

    display.init(9600);
    screenLayout(); //default is portrait


    // Load the Book data from the JSON file
    if (loadBookData()) {
        loadProgress();  // Load last read page
        showPage(currentPage);  // Display the first page
    }

    // Record the time of the last interaction
    lastInteractionTime = millis();
    // setCpuFrequencyMhz(20);
}


void loop() {

    // Check for button presses
    if (digitalRead(MENU_BUTTON) == PRESSED) { // debounce is taken care of via hardware cap
        isMenuActive = !isMenuActive; //toggle menu active
        if(isMenuActive) displayMenu();
    }

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
        esp_sleep_enable_ext1_wakeup((1ULL << NEXT_BUTTON) | (1ULL << PREV_BUTTON) | (1ULL << MENU_BUTTON), ESP_EXT1_WAKEUP_ALL_LOW); // Low since active LOW
        esp_light_sleep_start();
    }
}


//orientation as either portrait or landscape
void screenLayout(){ 
    if (isLandscape) {
        screenWidth = SCREEN_HEIGHT;
        screenHeight = SCREEN_WIDTH;
        display.setRotation(1);  // Landscape rotation
    } else {
        screenWidth = SCREEN_WIDTH;
        screenHeight = SCREEN_HEIGHT;
        display.setRotation(0);  // Portrait rotation
    }
}


void toggleLayout(){
    isLandscape = !isLandscape; //toggle orientation
    screenLayout();             //update the screen
}


// show menu graphics
void displayMenu(){
    display.setTextColor(GxEPD_BLACK);
    
    //menu outline
    display.fillRoundRect(109, 178, 263, 209, 7, 1);   //set blank background in the size of the menu first 
    display.drawRoundRect(109, 178, 263, 209, 7, 1); 

    //items
    display.drawRoundRect(118, 212, 244, 44, 5, 1); 
    display.drawRoundRect(118, 271, 244, 44, 5, 1);
    display.drawRoundRect(118, 331, 244, 44, 5, 1);

    //select seperator line 
    display.drawLine(155, 271, 155, 314, 1); 
    display.drawLine(155, 212, 155, 255, 1);
    display.drawLine(155, 331, 155, 374, 1);

    display.setTextSize(2);
    display.setCursor(119, 187);
    display.print("MENU");

    for (int i = 0; i < numMenuOptions; i++){
        display.setCursor(173, 227 + (i*60));
        display.print(menuOptions[i]);
    }
}

// Handles navigating through the menu
void navigateMenu(){
    // circulart navigation
    if (digitalRead(NEXT_BUTTON) == PRESSED) {
        selectedMenuOption = (selectedMenuOption + 1) % numMenuOptions;
    }
    if (digitalRead(PREV_BUTTON) == PRESSED) {
        selectedMenuOption = (selectedMenuOption - 1 + numMenuOptions) % numMenuOptions;
    }
    if (digitalRead(MENU_BUTTON) == PRESSED) {
        executeMenuOption();
    }
    
    display.setTextSize(2);
    switch (selectedMenuOption)
    {
        //select first rest is blank
    case 0:
        display.setCursor(133, 227);
        display.print(">");
        display.setCursor(133, 287);
        display.print(" ");
        display.setCursor(133, 347);
        display.print(" ");
        break;

        //select second rest is blank
    case 1:
        display.setCursor(133, 287);
        display.print(">");
        display.setCursor(133, 227);
        display.print(" ");
        display.setCursor(133, 347);
        display.print(" ");
        break;
    
        //select third rest is blank
    case 2:
        display.setCursor(133, 346);
        display.print(">");
        display.setCursor(133, 227);
        display.print(" ");
        display.setCursor(133, 287);
        display.print(" ");
        break;
    
        //default is select first rest is blank
    default:
        display.setCursor(133, 227);
        display.print(">");
        display.setCursor(133, 287);
        display.print(" ");
        display.setCursor(133, 347);
        display.print(" ");
        break;
    }
}


// Executes the selected menu option
void executeMenuOption(){
    switch(selectedMenuOption){
        // select book
        case 0:
            selectBookMenu();
            break;
        
        // select text size
        case 1:
            textSize = (textSize % 3) + 1;  // Cycle through text sizes 1, 2, 3
            display.setCursor(295, 287);
            display.print(textSize);
            break;
        
        // exit menu
        case 2:
            isMenuActive = false;
            break;
    }

}

void selectBookMenu(){
    return;
}

// Load Book data from the SD card (JSON file)
bool loadBookData() {
    // Open the JSON file
    
    File32 myFile = sd.open(CurrentBookjson, FILE_READ);
    if (!myFile) {
        Serial.println("Failed to open Book data file.");
        return false;
    }

    // Read the JSON file into the document
    DeserializationError error = deserializeJson(jsonDoc, myFile);
    myFile.close();
    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    // Validate that CurrentBook exists
    if (!jsonDoc.containsKey(CurrentBook)) {
        Serial.println("Book key not found in JSON.");
        return false;
    }

    // Calculate the total number of pages
    JsonArray pages = jsonDoc[CurrentBook];

    int maxPageNumber = 0; //variable to iterate and find the max number

    for(JsonObject page : pages){
        int pageNumber = page["page_number"];
        if(pageNumber > maxPageNumber){
            maxPageNumber = pageNumber;
        }
    }

    // Set the total pages
    totalPages = maxPageNumber;
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
    // Ensure pageNum is within the valid range
    if (pageNum < 0 || pageNum >= totalPages) {
        Serial.println("Invalid page number.");
        return;
    }

    // Fetch page content from the loaded book
    String pageContent = jsonDoc[CurrentBook][pageNum]["content"].as<String>();
    if (pageContent.length() == 0) {
        Serial.println("No content found for the page.");
        Serial.println("Writing blank on page");

                // Clear the screen
        display.fillScreen(GxEPD_WHITE);

        // Display the content
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(2);  // Adjust text size as needed
        display.setCursor(10, 10);


        return;
    }

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


// Save the current page number to a file
void saveProgress() {
    FILE progressFile = sd.open("/progress.txt", FILE_WRITE);
    if (progressFile) {
        progressFile.print(currentPage);
        progressFile.close();
        Serial.print("Saved current page: ");
        Serial.println(currentPage);
    } else {
        Serial.println("Failed to save progress.");
    }
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



// Load the last read page from file
void loadProgress() {
    FILE progressFile = sd.open("/progress.txt");
    if (progressFile) {
        currentPage = progressFile.parseInt();
        progressFile.close();
        Serial.print("Loaded last page: ");
        Serial.println(currentPage);
    } else {
        Serial.println("No progress file found, starting from the beginning.");
    }
}

void showbatteryLevel(){
    // Voltage divider: +B ---[ 100k ]--- Pin ---[ 100k ]--- GND
    //   Pin = +B/2
    float voltage = (2.0 * analogRead(BATTERY_PIN) * 3.3) / 4096.0;
    int batteryPercentage = int((voltage / FULL_BATTERY_VOLTAGE) * 100);  // Define FULL_BATTERY_VOLTAGE, e.g., 4.2V

    display.setTextColor(GxEPD_BLACK);
    display.drawRoundRect(448, 3, 29, 10, 4, 1); // Battery outline
    display.setTextWrap(false);
    display.setCursor(422, 5);
    display.print(batteryPercentage); // Display percentage
    display.print("%");

    //update battery fill rectangle based on percentage
    if(batteryPercentage >= 5){
        int batteryWidth = map(batteryPercentage, 0, 100, 6, 27);  // Map percentage to fill width
        display.fillRoundRect(449, 4, batteryWidth, 8, 2, 1); // Fill the rectangle based on battery percentage
    }
}

