#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <WiFi.h>
#include "time.h"
#include <SD.h>
#include <BluetoothSerial.h>


// NRF24
#define pinCE 21     // ESP32 GPIO for CE
#define pinCSN 4    // ESP32 GPIO for CSN (SS)

// SD CARD
#define SD_CS 5 
uint8_t cardType;

// NRF24 
RF24 radio(pinCE, pinCSN);
const byte address[6] = "PIPE1";

// WIFI
const char* ssid     = "Xiaomi 11T Pro";
const char* password = "00000000";


// BLUETOOTH
BluetoothSerial SerialBT;
void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);

bool radio_ok = false;
bool sd_ok = false;
bool bt_ok = false;
bool time_ok = false;

// Data structure must match the sender!
struct SensorData {
  float air_temperature;
  float air_humidity;
  int16_t soil_humidity;
};

struct tm timeinfo;

static boolean avg_day_done = false;
static boolean avg_week_done = false;
static boolean avg_month_done = false;
static boolean avg_year_done = false;

char week_average[64];
char month_average[64];


void setup() {
  Serial.begin(115200); 
  delay(500);
  Serial.println("\n\n=== boot ===");


  //////////////////////////////////////////
  /////////////// BLUETOOTH ////////////////
  //////////////////////////////////////////
  // --- Start Bluetooth first so the device is discoverable asap ---
  Serial.println("Init Bluetooth...");
  if (SerialBT.begin("my green house")) {
    Serial.println("Bluetooth started!");
    SerialBT.register_callback(callback);
    bt_ok = true;
  } else {
    Serial.println("Bluetooth failed to start!");
    bt_ok = false;
  }
  
  //////////////////////////////////////////
  /////////////// NRF24L01 /////////////////
  //////////////////////////////////////////
  // to call radio.begin() in the condition is calling it and initiating radio.
  Serial.println("Init nRF24...");
  if (radio.begin()) {
    radio_ok = true;
    radio.setPALevel(RF24_PA_HIGH);
    radio.setDataRate(RF24_250KBPS);
    radio.openReadingPipe(0, address);
    radio.startListening();
    Serial.println("nRF24 initialized");
  } else {
    radio_ok = false;
    Serial.println("NRF24L01 not detected! continuing without radio");
    // DO NOT block here
  }


  //////////////////////////////////////////
  /////////////// WIFI/TIME ////////////////
  //////////////////////////////////////////

  Serial.println("Trying WiFi to get time (timeout 15s)...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long t0 = millis();
  const unsigned long WIFI_TIMEOUT = 15000UL;
  while (millis() - t0 < WIFI_TIMEOUT && WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected, requesting NTP...");
    // timezone offset seconds, dst seconds (adjust as needed)
    configTime(3600, 0, "pool.ntp.org");
    unsigned long t1 = millis();
    const unsigned long NTP_TIMEOUT = 15000UL;
    while (millis() - t1 < NTP_TIMEOUT) {
      if (getLocalTime(&timeinfo)) break;
      delay(200);
    }
    if (getLocalTime(&timeinfo)) {
      time_ok = true;
      Serial.println("Time obtained:");
      Serial.println(asctime(&timeinfo));
    } else {
      time_ok = false;
      Serial.println("Failed to obtain time from NTP (continuing without time)");
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    time_ok = false;
    Serial.println("WiFi failed to connect - skipping NTP");
  }
  //////////////////////////////////////////
  //////////////// SD CARD /////////////////
  //////////////////////////////////////////
  Serial.println("Init SD card...");
  if (SD.begin(SD_CS)) {
    sd_ok = true;
    Serial.println("SD card mounted");
  } else {
    sd_ok = false;
    Serial.println("Card Mount Failed - continuing without SD");
  }

  Serial.println("Setup complete. Features:");
  Serial.printf("BT: %s, nRF24: %s, SD: %s, Time: %s\n",
                bt_ok ? "OK" : "NO",
                radio_ok ? "OK" : "NO",
                sd_ok ? "OK" : "NO",
                time_ok ? "OK" : "NO");

  
}

// callback to signal if a bluetooth client is connected
void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    Serial.println("Client Connected");
  }
}


////////// CALCULATE AVERAGE //////////
String calculate_average(int days_number) {
  String filenames[days_number];
  get_last_n_dates(timeinfo, days_number, filenames);

  float totalH = 0;
  float totalT = 0;
  int totalS = 0;
  int validCount = 0;

  for (int i = 0; i < days_number; i++) {
    String avgLine = get_AVG_line_from_file(filenames[i]);

    if (avgLine.indexOf("AVG:") == -1) continue;  // Skip invalid entries

    int hIndex = avgLine.indexOf("H:");
    int tIndex = avgLine.indexOf("T:");
    int sIndex = avgLine.indexOf("S:");

    if (hIndex >= 0 && tIndex > hIndex && sIndex > tIndex) {
      float h = avgLine.substring(hIndex + 2, tIndex).toFloat();
      float t = avgLine.substring(tIndex + 2, sIndex).toFloat();
      int s = avgLine.substring(sIndex + 2).toInt();

      totalH += h;
      totalT += t;
      totalS += s;
      validCount++;
    }
  }

  if (validCount == 0) return "No data";

  float avgH = totalH / validCount;
  float avgT = totalT / validCount;
  int avgS = totalS / validCount;
  
  char result[128];
  snprintf(result, sizeof(result), "H:%.2f T:%.2f S:%d", avgH, avgT, avgS);
  return String(result);
}


void get_last_n_dates(tm current, int n, String out[]) {
  time_t raw = mktime(&current);
  for (int i = 0; i < n; i++) {
    tm t;
    localtime_r(&raw, &t);

    char filename[32];
    strftime(filename, sizeof(filename), "/%Y-%m-%d.txt", &t);
    out[i] = String(filename);

    raw -= 86400; // Go back 1 day
  }
}



String get_AVG_line_from_file(String path) {
  File file = SD.open(path);
  if (!file) {
    Serial.print("Failed to open file: ");
    Serial.println(path);
    return "";
  }
  
  String line;
  String avgLine = "";
  while (file.available()) {
    line = file.readStringUntil('\n');
    if (line.indexOf("AVG:") != -1) {
      avgLine = line;
    }
  }
  file.close();
  return avgLine;
}


////////// CALCULATE TODAY AVERAGE //////////
String calculate_day_average(String file_name) {
  float totalHumidity = 0;
  float totalTemp = 0;
  int totalSoil = 0;
  int validLines = 0;

  File file = SD.open(file_name); // Or dynamically get today's file

  while (file.available()) {    
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int hIndex = line.indexOf("H:");
    int tIndex = line.indexOf("T:");
    int sIndex = line.indexOf("S:");

    if (hIndex > 0 && tIndex > hIndex && sIndex > tIndex) {
      float h = line.substring(hIndex + 2, tIndex).toFloat();
      float t = line.substring(tIndex + 2, sIndex).toFloat();
      int s = line.substring(sIndex + 2).toInt();
  
      totalHumidity += h;
      totalTemp += t;
      totalSoil += s;
      validLines++;
    }
  }
  file.close();

  if (validLines == 0) return "No valid data";

  float avgH = totalHumidity / validLines;
  float avgT = totalTemp / validLines;
  int avgS = totalSoil / validLines;

  char avgLine[64];
  snprintf(avgLine, sizeof(avgLine), "AVG: H:%.2f T:%.2f S:%d", avgH, avgT, avgS);

  return String(avgLine);
}


//////////////////////////////////////////
//////////////// SD CARD /////////////////
//////////////////////////////////////////

void write_data_to_file(SensorData received_data, tm timeinfo) {

  if (received_data.soil_humidity == 0 || received_data.air_humidity == 0.00 || received_data.air_temperature == 0.00) {
    Serial.println("Data's 0's so no writing");
    return;
  }
  char file_name[32];
  strftime(file_name, sizeof(file_name), "/%Y-%m-%d.txt", &timeinfo);

  File today_file = SD.open(file_name, FILE_APPEND);

  if (!today_file) {
    Serial.println("Failed to open file for appending.");
    return;
  }

  // Format timestamp and data
  char line[128];
  strftime(line, sizeof(line),"%H:%M:%S", &timeinfo);
  char dataLine[128];
  snprintf(dataLine, sizeof(dataLine), "%s H:%.2f T:%.2f S:%d", line,
           received_data.air_humidity,
           received_data.air_temperature,
           received_data.soil_humidity);

  // Write to SD
  today_file.println(dataLine);
  today_file.close();
  Serial.println("Data written to file:");
  Serial.println(dataLine);
}



// Gets today's filename, e.g., "/2025-06-26.txt"
String getTodayFilename() {
  if (!getLocalTime(&timeinfo)) return "/unknown.txt";
  char filename[32];
  strftime(filename, sizeof(filename), "/%Y-%m-%d.txt", &timeinfo);
  return String(filename);
}


String getLastLineOfTodayFile() {
  String file_name = getTodayFilename();
  File file = SD.open(file_name, FILE_READ);
  if (!file) return "Failed to open file";

  String lastLine = "";
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 2) lastLine = line;
  }
  file.close();

  
  if (lastLine.length() == 0) return "No data found";

  return lastLine;
}



String get_avg(String period, String time_value) {
  File file;
  String line;

  if (time_value == "week") {
    file = SD.open("/weeks_average.txt");
  } else if (time_value == "month") {
    file = SD.open("/months_average.txt");
  } else {
    return "404 not found";
  }
  if (!file) return "";
  String avgLine = "";
  while (file.available()) {
    line = file.readStringUntil('\n');
    if (line.indexOf(period) != -1) {
      avgLine = line;
    }
  }
  file.close();
  return avgLine;
}

String get_file_all_lines(String file_name, String time_value) {
  File file;
  String line;
  
  if (time_value == "day") {
    file = SD.open(file_name);
  } else if (time_value == "week") {
    file = SD.open(file_name);
  } else if (time_value == "month") {
    file = SD.open(file_name);
  } else {
    return "404 not found";
  }
  
  if (!file) return "";
  while (file.available()) {
    line = file.readStringUntil('\n');
    if (!line.startsWith("AVG:")) {
      SerialBT.println(line);
    }
  }

  file.close();
  return "multiple datas returned";
}

String list_last_days_avg(int days_number) {
  String filenames[days_number];
  get_last_n_dates(timeinfo, days_number, filenames);

  String result = "";

  for (int i = 0; i < days_number; i++) {
    String avgLine = get_AVG_line_from_file(filenames[i]);

    if (avgLine.indexOf("AVG:") == -1) continue;  // Ignore si pas valide

    // Extraire la date à partir du nom du fichier
    // filenames[i] est du style "/2025-07-03.txt"
    String dateStr = filenames[i];
    if (dateStr.startsWith("/")) dateStr.remove(0, 1);   // enlever le "/"
    if (dateStr.endsWith(".txt")) dateStr.remove(dateStr.length() - 4);

    // Ajouter à la liste → format : "2025-07-03,AVG:H:..T:..S:.."
    result += dateStr + " " + avgLine + "\n";
  }

  return result;
}


void loop() {
  //////////////////////////////////////////
  /////////////// WIFI/TIME ////////////////
  //////////////////////////////////////////
  // update timeinfo if available
  if (getLocalTime(&timeinfo)) {
    time_ok = true;
  } else {
    // Don't return here! we want Bluetooth to keep working even without time.
    time_ok = false;
  }
  
  //////////////////////////////////////////
  /////////////// NRF24L01 /////////////////
  //////////////////////////////////////////
  if (radio_ok) {
    if (radio.available()) {
      SensorData received_data;
      radio.read(&received_data, sizeof(received_data));
      // will use timeinfo (may be epoch if no NTP) — OK
      write_data_to_file(received_data, timeinfo);
    }
  }

  //////////////////////////////////////////
  /////////////// BLUETOOTH ////////////////
  //////////////////////////////////////////
  if (bt_ok && SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() > 0) {
      Serial.println(cmd);
    }

    if (cmd == "GET /last") {
      String last_line = getLastLineOfTodayFile();
      SerialBT.println(last_line);
      Serial.println(last_line);
      SerialBT.println(".");
    } else if (cmd.startsWith("GET /avg/day")) {
      String date = cmd.substring(12);
      String file_name = date + ".txt";
      String day_avg = get_AVG_line_from_file(file_name);
      if (day_avg != "") {
        SerialBT.println(day_avg);
      } else {
        SerialBT.println("Data not found please verify the date is correct or that the date is not too early and that data exist for that period of time");
      }
      SerialBT.println(".");
    } else if (cmd.startsWith("GET /avg/week")) {
      String week = cmd.substring(14);
      String week_avg = get_avg(week, "week");
      if (week_avg != "") {
        SerialBT.println(week_avg);
      } else {
        SerialBT.println("Data not found please verify the date is correct or that the date is not too early and that data exist for that period of time");
      }
      SerialBT.println(".");
      
    } else if (cmd.startsWith("GET /avg/month")) {
      String month = cmd.substring(15);
      String month_avg = get_avg(month, "month");
      if (month_avg != "") {
        SerialBT.println(month_avg);
      } else {
        SerialBT.println("Data not found please verify the date is correct or that the date is not too early and that data exist for that period of time");
      }  
      SerialBT.println(".");

    } else if (cmd.startsWith("GET /all/day")) {
      String date = cmd.substring(12);
      String file_name = date + ".txt";
      
      String response = get_file_all_lines(file_name, "day");
      if (response != "") {
        Serial.println(response);
      } else {
        SerialBT.println("Data not found please verify the date is correct or that the date is not too early and that data exist for that period of time");
      }
      SerialBT.println(".");
      
    } else if (cmd.startsWith("GET /all/week")) {
      String response = get_file_all_lines("/weeks_average.txt", "week");
      if (response != "") {
        Serial.println(response);
      } else {
        SerialBT.println("Data not found please verify the date is correct or that the date is not too early and that data exist for that period of time");
      }
      SerialBT.println(".");
      
    } else if (cmd.startsWith("GET /all/month")) {
      String response = get_file_all_lines("/months_average.txt", "month");
      if (response != "") {
        Serial.println(response);
      } else {
        SerialBT.println("Data not found please verify the date is correct or that the date is not too early and that data exist for that period of time");
      }
      SerialBT.println(".");

    } else if (cmd.startsWith("GET /last/days")) {
      String days_number_str = cmd.substring(15);
      days_number_str.trim();
      int days_number = days_number_str.toInt();
      String response = list_last_days_avg(days_number);
      Serial.println(response);
      if (response != "") {
        SerialBT.println(response);
      } else {
        SerialBT.println("Data not found please verify the date is correct or that the date is not too early and that data exist for that period of time");
      }
      SerialBT.println(".");
      
    } else {
      SerialBT.println("404 Not Found");
      SerialBT.println(".");
    }
  }

  //////////////////////////////////////////
  /////////////// AVERAGES /////////////////
  //////////////////////////////////////////
  // TODAY AVERAGE
  if (timeinfo.tm_hour == 23 && timeinfo.tm_min >= 55 && !avg_day_done) {

    String file_name = getTodayFilename();
    String avgLine = calculate_day_average(file_name);
    Serial.println(avgLine);

    File file = SD.open(file_name, FILE_APPEND);
    if (file) {
      file.println(avgLine);
      file.flush();
      file.close();
      Serial.println("wrote days average");
      avg_day_done = true;
    } else {
      Serial.println("Failed to open file for appending");
    }
  } 
  if (timeinfo.tm_hour != 23) avg_day_done = false;


  // WEEK AVERAGE
  if (timeinfo.tm_wday == 0 && !avg_week_done) {
    char weekStr[16];
    strftime(weekStr, sizeof(weekStr), "%Y-W%V", &timeinfo);
    
    String avgStr = calculate_average(7);
    //char week_average[64];
    snprintf(week_average, sizeof(week_average), "%s %s", weekStr, avgStr.c_str());
    
    File file = SD.open("/weeks_average.txt", FILE_APPEND);
    if (file) {
      file.println(week_average);
      file.flush();
      Serial.println("wrote weeks average");
      avg_week_done = true;
    } else {
      Serial.println("Failed to open file for appending");
    }
    file.close();
  }
  if (timeinfo.tm_wday != 0) avg_week_done = false;

  // MONTH AVERAGE
  if (timeinfo.tm_mday == 1 && !avg_month_done) {
    tm lastMonth = timeinfo;
    lastMonth.tm_mon -= 1; // aller au mois précédent
    mktime(&lastMonth);
    
    char monthStr[16];
    strftime(monthStr, sizeof(monthStr), "%Y-%m", &lastMonth);
    
    String avgStr = calculate_average(29);
    snprintf(month_average, sizeof(month_average), "%s %s", monthStr, avgStr.c_str());
    
    File file = SD.open("/months_average.txt", FILE_APPEND);
    if (file) {
      file.println(month_average);
      file.flush();
      Serial.println("wrote months average");
      avg_month_done = true;
    } else {
      Serial.println("Failed to open file for appending");
    }      
    file.close();
  }
  if (timeinfo.tm_mday != 1) avg_month_done = false;
  

  delay(1000); // Avoid spamming the Serial Monitor
}
