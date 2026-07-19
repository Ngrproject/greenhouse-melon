#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <DHT.h>
#include <Preferences.h> 

// --- LIBRARY KONEKTIVITAS & FIREBASE RTDB ---
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> 

// =========================================================================
// --- PENTING: KREDENSIAL FIREBASE ---
// =========================================================================
#define FIREBASE_HOST "https://greenflow-be349-default-rtdb.asia-southeast1.firebasedatabase.app/" 
#define FIREBASE_AUTH "tjmsp5xRbjN8J1rjJDChDnrBro0Vum8uJtZ9D9oj"
#define DEVICE_ID "greenflow-001"

// Objek Inti Firebase
FirebaseData fbdoStore;
FirebaseData fbdoStreamActuators;
FirebaseData fbdoStreamConfig; 
FirebaseAuth auth;
FirebaseConfig configfb;
Preferences preferences; 

bool isOfflineMode = false; 
volatile uint8_t modeSistem = 2; // (0=Manual, 1=Auto/Sensor, 2=Terjadwal)

// --- VARIABEL DINAMIS DARI WEB ---
int hst_sekarang = 0; 
int targetVolumeML = 250; 
float debitPompaPerDetik = 3.0; 
float detikKalibrasiPompa = 33.3; // Telah ditambahkan
float thresholdSuhuKipas = 39.0;    
float thresholdLembabKipas = 80.0;  
float thresholdSuhuAman = 37.0;     
float thresholdLembabAman = 78.0;   
int batasTanahKering = 45; 
int batasTanahBasah = 75;  

// Fitur Baru Battery Saver & Restart
bool batterySaverStatus = false; // Telah ditambahkan
String batterySaverTime = "night_only"; // Telah ditambahkan
bool autoRestartStatus = false; // Telah ditambahkan

unsigned long DURASI_KIPAS_ON = 7200000;  
unsigned long DURASI_KIPAS_OFF = 3600000; 
int jamAutoRestart = 2;             
int menitAutoRestart = 0;

// --- VARIABEL INTERNAL MEMORI PREFERENCES ---
int TAHUN_TANAM;
int BULAN_TANAM;
int TANGGAL_TANAM;
int penyiramanKe = 0;
int tanggalSistemUntukReset = -1; 
String jamTerakhirSiram = "--:--";

// --- VARIABEL KONTROL POMPA & COMMANDS ---
bool isPompaTerjadwalMenyala = false;
unsigned long waktuMulaiPompaTerjadwal = 0;
int jamTerakhirDisiram = -1; 
bool pesanJadwalTerkirim = true; 

unsigned long lastProcessedCmd = 0; // Telah ditambahkan untuk validasi command terbaru
String cmdToDelete = ""; // Telah ditambahkan untuk penghapusan commands
volatile bool pendingCmdDelete = false; // Telah ditambahkan

// --- VARIABEL ANTRIAN PESAN LOGS ---
String pesanAntrian = ""; 
volatile bool kirimPesanSekarang = false;
volatile bool flagRestart = false; 

// --- VARIABEL ANALISIS TELEMETRI ---
float suhuMax = 0.0;
float suhuMin = 100.0;
bool laporanTerkirim = false;
bool peringatanAkiTerkirim = false;
float vpd = 0.0;

// --- VARIABEL MANAJEMEN KIPAS ---
enum StateKipas { KIPAS_IDLE, KIPAS_ON_CYCLE, KIPAS_OFF_CYCLE };
StateKipas statusKipas = KIPAS_IDLE;
unsigned long waktuMulaiKipas = 0;

// --- Konfigurasi OLED & Sensor ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_PIN_A 32 
#define SOIL_PIN_B 35 
const int BATAS_KERING = 3400; 
const int BATAS_BASAH = 1400;  

#define VOLT_PIN 34 
float teganganAki = 0.0;
int persenAki = 0;
float faktorKalibrasi = 5.05; 
float teganganAkiSmoothed = 0.0;
bool pertamaKaliBacaAki = true;

// Relai Active LOW
#define RELAY1 25 // Kipas
#define RELAY2 26 // Pompa Air

float suhu = 0.0;
float lembab = 0.0;
int persenSoilA = 0;
int persenSoilB = 0; 

// --- Data Bitmap Logo (128x64) ---
const unsigned char epd_bitmap_logo [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0x07, 0x07, 0x3f, 0xf3, 0xfe, 0x31, 0xff, 0x00, 
  0x00, 0x1f, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0x87, 0x8f, 0x3f, 0xf3, 0xff, 0x7b, 0xff, 0x00, 
  0x00, 0x1f, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xdf, 0x7f, 0xf7, 0xfe, 0x73, 0xff, 0x00, 
  0x00, 0x1f, 0xff, 0xfb, 0xff, 0xfd, 0xff, 0xff, 0x8f, 0xff, 0x78, 0x00, 0x1e, 0xf0, 0x0f, 0x00, 
  0x00, 0x1f, 0x83, 0xfb, 0xf0, 0x00, 0xfe, 0x1f, 0x8f, 0xfe, 0x7f, 0xcf, 0x1e, 0xe7, 0x8e, 0x00, 
  0x00, 0x3f, 0x87, 0xf7, 0xf0, 0x00, 0xfc, 0x1f, 0x9e, 0xde, 0xff, 0x8f, 0x1c, 0xe7, 0xfe, 0x00, 
  0x00, 0x3f, 0x87, 0xf7, 0xe7, 0xff, 0xff, 0xff, 0x1c, 0x9c, 0xe0, 0x0e, 0x3d, 0xef, 0xfe, 0x00, 
  0x00, 0x3e, 0x27, 0xf7, 0xe7, 0xfd, 0xfd, 0xff, 0x1c, 0x1c, 0xff, 0xdf, 0xf9, 0xcf, 0x3c, 0x00, 
  0x00, 0x6f, 0x0f, 0xcf, 0xe7, 0x75, 0xf9, 0xbe, 0x3c, 0x3d, 0xff, 0x9f, 0xfb, 0xce, 0x3c, 0x00, 
  0x00, 0x53, 0x0f, 0xaf, 0x81, 0xfa, 0xf9, 0xf8, 0x38, 0x39, 0xff, 0x9f, 0xe1, 0x8c, 0x18, 0x00, 
  0x00, 0x4a, 0x4c, 0xd0, 0x40, 0x0b, 0xb8, 0x7c, 0x7f, 0xf3, 0xde, 0xcb, 0xfe, 0xde, 0xc8, 0x00, 
  0x00, 0xfa, 0x1f, 0x50, 0x7e, 0x0a, 0xf0, 0xfc, 0x7f, 0xbf, 0xf7, 0xdf, 0x3d, 0xb6, 0xd8, 0x00, 
  0x00, 0x94, 0x95, 0xd1, 0xfe, 0x87, 0xd0, 0xf8, 0x7f, 0xfc, 0xb7, 0x9e, 0x09, 0xa7, 0xf0, 0x00, 
  0x00, 0xec, 0x9b, 0xb1, 0xbd, 0x97, 0xf0, 0x7e, 0x43, 0x6d, 0xb7, 0xbe, 0x19, 0xa5, 0xf0, 0x00, 
  0x01, 0xfc, 0x3f, 0x8f, 0xff, 0xf7, 0xe0, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

RTC_DS3231 rtc;

// --- Variabel Timer ---
unsigned long waktuSebelumnyaHalaman = 0;
unsigned long jedaGantiLayar = 10000; 
uint8_t halamanAktif = 0; 
unsigned long waktuSebelumnyaSensor = 0;
const unsigned long jedaUpdateSensor = 2000; 
unsigned long waktuSebelumnyaWiFi = 0;
const unsigned long jedaCekWiFi = 75000; 
unsigned long waktuSebelumnyaTelemetri = 0;
const unsigned long jedaKirimTelemetri = 60000; 

// =========================================================================
// LOGIKA PARSING JADWAL SOP DAN TARGET AIR SECARA DINAMIS DARI WEB
// =========================================================================
int hitungTargetVolumeDinamis(int hst) {
  String jsonSchedules = preferences.getString("sched_data", "");
  if (jsonSchedules != "") {
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, jsonSchedules);
    if (!error && doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject v : arr) {
        int hstMulai = v["hst_mulai"];
        int hstSelesai = v["hst_selesai"];
        if (hst >= hstMulai && hst <= hstSelesai) {
          return v["target_ml"]; 
        }
      }
    }
  }
  if (hst == 1) return 156;
  else if (hst >= 2 && hst <= 5) return 250;
  else if (hst >= 6 && hst <= 10) return 300;
  else if (hst >= 11 && hst <= 17) return 292;
  else if (hst >= 18 && hst <= 25) return 219;
  return 250;
}

bool cekJadwalSiramDinamis(int hst, int jam, int menit) {
  String jsonSchedules = preferences.getString("sched_data", "");
  if (jsonSchedules != "") {
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, jsonSchedules);
    if (!error && doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject v : arr) {
        int hstMulai = v["hst_mulai"];
        int hstSelesai = v["hst_selesai"];
        if (hst >= hstMulai && hst <= hstSelesai) {
          JsonArray waktuSiram = v["waktu_siram"];
          for (String t : waktuSiram) {
            int targetJam = t.substring(0, 2).toInt();
            int targetMenit = t.substring(3, 5).toInt();
            if (jam == targetJam && abs(menit - targetMenit) <= 2) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

String dapatkanJadwalSelanjutnyaDinamis(int hst, int jam, int menit) {
  int waktuSekarang = (jam * 60) + menit;
  String jsonSchedules = preferences.getString("sched_data", "");
  if (jsonSchedules != "") {
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, jsonSchedules);
    if (!error && doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject v : arr) {
        int hstMulai = v["hst_mulai"];
        int hstSelesai = v["hst_selesai"];
        if (hst >= hstMulai && hst <= hstSelesai) {
          JsonArray waktuSiram = v["waktu_siram"];
          int menitTerkecilBesok = 9999;
          String jadwalBesok = "";
          
          for (String t : waktuSiram) {
            int tj = t.substring(0, 2).toInt();
            int tm = t.substring(3, 5).toInt();
            int totalMenitJadwal = (tj * 60) + tm;
            
            if (totalMenitJadwal > waktuSekarang) {
              return t; 
            }
            if (totalMenitJadwal < menitTerkecilBesok) {
              menitTerkecilBesok = totalMenitJadwal;
              jadwalBesok = t;
            }
          }
          if (jadwalBesok != "") {
            String res = jadwalBesok;
            res += " (Besok)";
            return res;
          }
        }
      }
    }
  }
  return "--:--";
}

void catatPenyiraman() {
  DateTime now = rtc.now();
  if (tanggalSistemUntukReset != now.day()) {
    penyiramanKe = 0;
    tanggalSistemUntukReset = now.day();
    preferences.putInt("tglReset", tanggalSistemUntukReset);
  }
  penyiramanKe++;
  preferences.putInt("siramKe", penyiramanKe); 
  
  char buf[6];
  sprintf(buf, "%02d:%02d", now.hour(), now.minute());
  jamTerakhirSiram = String(buf);
  preferences.putString("jamSiram", jamTerakhirSiram); 
}

void kontrolManajemenKipas(float suhuAktual, float lembabAktual, unsigned long waktuSaatIni, int jamSekarang) {
    if (suhuAktual < thresholdSuhuAman && lembabAktual < thresholdLembabAman) {
        if (statusKipas != KIPAS_IDLE) {
            digitalWrite(RELAY1, HIGH); 
            statusKipas = KIPAS_IDLE;
            pesanAntrian = "✅ Kipas Otomatis MATI\nSuhu/Lembab kembali normal.";
            kirimPesanSekarang = true;
        }
        return; 
    }

    bool isMalamHari = (jamSekarang >= 18 || jamSekarang < 6);
    bool applyCycling = false;

    // --- FIX: Penerapan Field Battery Saver dari Web ---
    if (batterySaverStatus) {
        if (batterySaverTime == "full_day") applyCycling = true;
        else if (batterySaverTime == "night_only" && isMalamHari) applyCycling = true;
    }

    switch (statusKipas) {
        case KIPAS_IDLE:
            if (suhuAktual >= thresholdSuhuKipas || lembabAktual >= thresholdLembabKipas) {
                digitalWrite(RELAY1, LOW); 
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_ON_CYCLE; 
                pesanAntrian = "⚠️ Kipas Otomatis NYALA\nTreshold Terlewati. S:";
                pesanAntrian += String(suhuAktual, 1);
                pesanAntrian += "°C | L:";
                pesanAntrian += String(lembabAktual, 1);
                pesanAntrian += "%";
                kirimPesanSekarang = true;
            }
            break;
        case KIPAS_ON_CYCLE:
            if (applyCycling && (waktuSaatIni - waktuMulaiKipas >= DURASI_KIPAS_ON)) {
                digitalWrite(RELAY1, HIGH); 
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_OFF_CYCLE;
                pesanAntrian = "🛑 Kipas JEDA (Battery Saver Aktif)";
                kirimPesanSekarang = true;
            }
            break;
        case KIPAS_OFF_CYCLE:
            if (!applyCycling || (waktuSaatIni - waktuMulaiKipas >= DURASI_KIPAS_OFF)) {
                digitalWrite(RELAY1, LOW); 
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_ON_CYCLE;
                pesanAntrian = "💨 Kipas Otomatis NYALA KEMBALI.";
                kirimPesanSekarang = true;
            }
            break;
    }
}

// =========================================================================
// STREAM ACTUATOR DARI COMMANDS WEB DASHBOARD (DIUBAH UNTUK PEMBERSIHAN RIWAYAT)
// =========================================================================
void streamActuatorCallback(FirebaseStream data) {
  String path = data.streamPath();
  String dataType = data.dataType();
  
  if (dataType == "json") {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data.jsonString());
    
    if (!error) {
      JsonObject root = doc.as<JsonObject>();
      
      // Jika Firebase mengirimkan single command yang ditaruh di dalam direktori timestamp
      if (root.containsKey("from") || root.containsKey("timestamp")) {
        if (root.containsKey("mode")) {
           String val = root["mode"].as<String>();
           if (val == "auto") modeSistem = 1;
           else if (val == "schedule") modeSistem = 2;
           else if (val == "manual") { modeSistem = 0; statusKipas = KIPAS_IDLE; }
        }
        if (root.containsKey("pump") && modeSistem == 0) {
           bool statusWeb = root["pump"].as<bool>();
           if (statusWeb && digitalRead(RELAY2) == HIGH) { digitalWrite(RELAY2, LOW); catatPenyiraman(); }
           else if (!statusWeb) { digitalWrite(RELAY2, HIGH); }
        }
        if (root.containsKey("fan") && modeSistem == 0) {
           digitalWrite(RELAY1, root["fan"].as<bool>() ? LOW : HIGH);
        }
        
       // --- FIX: Tandai spesifik command ini untuk dihapus dari riwayat Firebase ---
        if (path.length() > 1) {
          cmdToDelete = "/commands/";
          cmdToDelete += DEVICE_ID;
          cmdToDelete += path;
          pendingCmdDelete = true;
        }
      } 
      else {
        // Jika Firebase mengirimkan banyak riwayat command sekaligus (saat ESP32 baru boot/reconnect)
        unsigned long maxTs = 0;
        JsonObject newestCmd;
        
        for (JsonPair kv : root) {
          if (kv.value().is<JsonObject>()) {
            unsigned long ts = String(kv.key().c_str()).toInt();
            if (ts > maxTs) {
              maxTs = ts;
              newestCmd = kv.value().as<JsonObject>();
            }
          }
        }
        
        // --- FIX: Hanya eksekusi jika command tersebut adalah yang paling baru (Highest Timestamp) ---
        if (maxTs > lastProcessedCmd) {
          lastProcessedCmd = maxTs;
          if (newestCmd.containsKey("mode")) {
             String val = newestCmd["mode"].as<String>();
             if (val == "auto") modeSistem = 1;
             else if (val == "schedule") modeSistem = 2;
             else if (val == "manual") { modeSistem = 0; statusKipas = KIPAS_IDLE; }
          }
          if (newestCmd.containsKey("pump") && modeSistem == 0) {
             bool statusWeb = newestCmd["pump"].as<bool>();
             if (statusWeb && digitalRead(RELAY2) == HIGH) { digitalWrite(RELAY2, LOW); catatPenyiraman(); }
             else if (!statusWeb) { digitalWrite(RELAY2, HIGH); }
          }
          if (newestCmd.containsKey("fan") && modeSistem == 0) {
             digitalWrite(RELAY1, newestCmd["fan"].as<bool>() ? LOW : HIGH);
          }
          
          // --- FIX: Bersihkan seluruh node /commands agar riwayat lama musnah ---
          cmdToDelete = "/commands/" DEVICE_ID;
          pendingCmdDelete = true;
        }
      }
    }
  }
}

// =========================================================================
// STREAM CONFIG DARI WEB DASHBOARD
// =========================================================================
void streamConfigCallback(FirebaseStream data) {
  String path = data.streamPath();
  String dataType = data.dataType();

  if (dataType == "json") {
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, data.jsonString());
    if (!error) {
      // Tangkap khusus jika Single Update diarahkan ke /pumpCalibration
      if (path == "/pumpCalibration") {
         if (doc.containsKey("mlPerSecond")) debitPompaPerDetik = doc["mlPerSecond"];
      } 
      else {
          // --- FIX: Penyatuan Pembacaan Kalibrasi Pompa ---
          if (doc.containsKey("pumpCalibration")) {
             if (doc["pumpCalibration"].containsKey("mlPerSecond")) {
                 debitPompaPerDetik = doc["pumpCalibration"]["mlPerSecond"];
             }
          } else if (doc.containsKey("debit_pompa")) {
             debitPompaPerDetik = doc["debit_pompa"];
          }
          
          // --- FIX: Membaca Field Auto Restart & Battery Saver dari RTDB ---
          if (doc.containsKey("auto_restart_status")) autoRestartStatus = doc["auto_restart_status"];
          if (doc.containsKey("battery_saver_status")) batterySaverStatus = doc["battery_saver_status"];
          if (doc.containsKey("battery_saver_time")) batterySaverTime = doc["battery_saver_time"].as<String>();
          if (doc.containsKey("detik_kalibrasi_pompa")) detikKalibrasiPompa = doc["detik_kalibrasi_pompa"];
          
          if (doc.containsKey("suhu_kipas_on")) thresholdSuhuKipas = doc["suhu_kipas_on"];
          if (doc.containsKey("suhu_kipas_off")) thresholdSuhuAman = doc["suhu_kipas_off"];
          if (doc.containsKey("lembab_kipas_on")) thresholdLembabKipas = doc["lembab_kipas_on"];
          if (doc.containsKey("lembab_kipas_off")) thresholdLembabAman = doc["lembab_kipas_off"];
          
          if (doc.containsKey("tanah_pompa_on")) batasTanahKering = doc["tanah_pompa_on"];
          if (doc.containsKey("tanah_pompa_off")) batasTanahBasah = doc["tanah_pompa_off"];
          
          if (doc.containsKey("kipas_siklus_on")) DURASI_KIPAS_ON = doc["kipas_siklus_on"].as<unsigned long>() * 60000UL;
          if (doc.containsKey("kipas_siklus_off")) DURASI_KIPAS_OFF = doc["kipas_siklus_off"].as<unsigned long>() * 60000UL;

          if (doc.containsKey("auto_restart_time")) {
            String timeStr = doc["auto_restart_time"].as<String>();
            if (timeStr.length() >= 5) {
              jamAutoRestart = timeStr.substring(0, 2).toInt();
              menitAutoRestart = timeStr.substring(3, 5).toInt();
            }
          }
          if (doc.containsKey("schedules")) {
            String schedStr;
            serializeJson(doc["schedules"], schedStr);
            preferences.putString("sched_data", schedStr);
          }
      }
      
      preferences.putFloat("thSuhu", thresholdSuhuKipas);
      preferences.putFloat("thLembab", thresholdLembabKipas);
      preferences.putFloat("debitPompa", debitPompaPerDetik);
      preferences.putInt("rstJam", jamAutoRestart);
      preferences.putInt("rstMenit", menitAutoRestart);
      
      Serial.println("✅ Sinkronisasi Konfigurasi Penuh dari Web Berhasil Diterapkan!");
    }
  } 
  else {
    if (path == "/plantingDate") {
      String dateStr = String(data.stringData().c_str());
      if (dateStr.length() == 10) {
        TAHUN_TANAM = dateStr.substring(0, 4).toInt();
        BULAN_TANAM = dateStr.substring(5, 7).toInt();
        TANGGAL_TANAM = dateStr.substring(8, 10).toInt();
        preferences.putInt("tahun", TAHUN_TANAM);
        preferences.putInt("bulan", BULAN_TANAM);
        preferences.putInt("tanggal", TANGGAL_TANAM);
        penyiramanKe = 0;
        jamTerakhirSiram = "--:--";
        jamTerakhirDisiram = -1;
        preferences.putInt("siramKe", penyiramanKe);
        preferences.putString("jamSiram", jamTerakhirSiram);
        preferences.putInt("jamDisiram", jamTerakhirDisiram);
      }
    }
  }
}

void streamTimeoutCallback(bool timeout) {}

// =========================================================================
// TASK CORE 0: ENGINGE FIREBASE RTDB NON-BLOCKING (LOGS, TELEMETRI & STREAM)
// =========================================================================
void TaskFirebaseCode( void * pvParameters ) {
  for(;;) {
    if (!isOfflineMode && WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      
      // --- FIX: Eksekusi hapus riwayat perintah lama ---
      if (pendingCmdDelete && cmdToDelete != "") {
        Firebase.RTDB.deleteNode(&fbdoStore, cmdToDelete.c_str());
        pendingCmdDelete = false;
        cmdToDelete = "";
      }
      
      if (kirimPesanSekarang && pesanAntrian != "") {
        String pesanDikirim = pesanAntrian; 
        pesanAntrian = ""; 
        kirimPesanSekarang = false; 

        FirebaseJson jsonLog;
        jsonLog.set("message", pesanDikirim.c_str());
        
        String tsLog = String(rtc.now().unixtime());
        tsLog += "000";
        jsonLog.set("timestamp", tsLog.c_str());

        Firebase.RTDB.pushJSON(&fbdoStore, "/logs/" DEVICE_ID, &jsonLog);
      }
      
      unsigned long waktuSaatIni = millis();
      if (waktuSaatIni - waktuSebelumnyaTelemetri >= jedaKirimTelemetri) {
        waktuSebelumnyaTelemetri = waktuSaatIni;

        FirebaseJson jsonTelemetry, jsonSensors, jsonStatus, jsonActuators;

        jsonSensors.set("temperature", isnan(suhu) ? 0.0 : suhu);
        jsonSensors.set("humidity", isnan(lembab) ? 0.0 : lembab);
        jsonSensors.set("vpd", vpd);
        jsonSensors.set("soilMoistureA", persenSoilA);
        jsonSensors.set("soilMoistureB", persenSoilB);
        jsonSensors.set("batteryVoltage", round(teganganAki * 10) / 10.0);
        jsonSensors.set("rssi", WiFi.RSSI());
        jsonSensors.set("wifi_ssid", WiFi.SSID());
        jsonSensors.set("wifi_ip", WiFi.localIP().toString().c_str());
        
        String tsSensors = String(rtc.now().unixtime());
        tsSensors += "000";
        jsonSensors.set("timestamp", tsSensors.c_str());

        jsonStatus.set("uptime", millis() / 1000);
        jsonStatus.set("jam_terakhir_siram", jamTerakhirSiram.c_str());
        jsonStatus.set("penyiraman_ke", penyiramanKe);

        jsonActuators.set("mode", modeSistem == 1 ? "auto" : modeSistem == 2 ? "schedule" : "manual");
        jsonActuators.set("pump", digitalRead(RELAY2) == LOW);
        jsonActuators.set("fan", digitalRead(RELAY1) == LOW);

        jsonTelemetry.set("sensors", jsonSensors);
        jsonTelemetry.set("status", jsonStatus);
        jsonTelemetry.set("actuators", jsonActuators);

        if (Firebase.RTDB.updateNode(&fbdoStore, "/live/" DEVICE_ID, &jsonTelemetry)) {
          Firebase.RTDB.pushJSON(&fbdoStore, "/history/" DEVICE_ID, &jsonSensors);
        }
      }

      if (flagRestart) { delay(5000); ESP.restart(); }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  
  preferences.begin("greenhouse", false);
  TAHUN_TANAM = preferences.getInt("tahun", 2026);
  BULAN_TANAM = preferences.getInt("bulan", 5);
  TANGGAL_TANAM = preferences.getInt("tanggal", 20);
  debitPompaPerDetik = preferences.getFloat("debitPompa", 3.0);  
  thresholdSuhuKipas = preferences.getFloat("thSuhu", 39.0);
  thresholdLembabKipas = preferences.getFloat("thLembab", 80.0);
  
  batasTanahKering = preferences.getInt("thTanahOn", 45);
  batasTanahBasah = preferences.getInt("thTanahOff", 75);

  jamAutoRestart = preferences.getInt("rstJam", 2);
  menitAutoRestart = preferences.getInt("rstMenit", 0);
  penyiramanKe = preferences.getInt("siramKe", 0);
  tanggalSistemUntukReset = preferences.getInt("tglReset", -1);
  jamTerakhirSiram = preferences.getString("jamSiram", "--:--");
  jamTerakhirDisiram = preferences.getInt("jamDisiram", -1);

  Wire.begin(21, 22);
  Wire.setClock(400000); 

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, HIGH);  
  digitalWrite(RELAY2, HIGH);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20); display.print("Mencari Wi-Fi...");
  display.display();

  WiFiManager wm;
  wm.setConfigPortalTimeout(180); 

  if (!wm.autoConnect("GreenFlow-Config")) {
    isOfflineMode = true; 
    delay(3000);
  } else {
    isOfflineMode = false;
    
    configfb.database_url = FIREBASE_HOST;
    configfb.signer.tokens.legacy_token = FIREBASE_AUTH;
    
    Firebase.reconnectWiFi(true);
    Firebase.begin(&configfb, &auth);

    if (Firebase.RTDB.beginStream(&fbdoStreamActuators, "/commands/" DEVICE_ID)) {
      Firebase.RTDB.setStreamCallback(&fbdoStreamActuators, streamActuatorCallback, streamTimeoutCallback);
    }
    if (Firebase.RTDB.beginStream(&fbdoStreamConfig, "/config/" DEVICE_ID)) {
      Firebase.RTDB.setStreamCallback(&fbdoStreamConfig, streamConfigCallback, streamTimeoutCallback);
    }
    
    pesanAntrian = "🚀 Sistem GreenFlow Online! Semua parameter sinkron penuh dengan Web.";
    kirimPesanSekarang = true;
  }

  xTaskCreatePinnedToCore(TaskFirebaseCode, "TaskFirebase", 10000, NULL, 1, NULL, 0);

  dht.begin(); 
  rtc.begin();
}

void loop() {
  unsigned long waktuSekarang = millis();

  if (waktuSekarang - waktuSebelumnyaWiFi >= jedaCekWiFi) {
    waktuSekarang = millis(); // Refresh waktu setelah blocking wifi
    waktuSebelumnyaWiFi = waktuSekarang;
    if (WiFi.status() != WL_CONNECTED) {
      if (!isOfflineMode) isOfflineMode = true;
      WiFi.reconnect(); 
    } else if (isOfflineMode) {
      isOfflineMode = false; 
      pesanAntrian = "🔄 Jaringan Pulih. Sistem terhubung kembali.";
      kirimPesanSekarang = true;
    }
  }

  // --- LOGIKA POMPA AIR ---
  if (modeSistem == 2 && isPompaTerjadwalMenyala) {
    unsigned long durasiSiramMS = ((targetVolumeML / debitPompaPerDetik) * 1000) + 2000;
    if (waktuSekarang - waktuMulaiPompaTerjadwal >= durasiSiramMS) {
      isPompaTerjadwalMenyala = false;
      digitalWrite(RELAY2, HIGH); 
      
      pesanAntrian = "✅ Pengairan SOP Selesai. ";
      pesanAntrian += String(targetVolumeML);
      pesanAntrian += " ml terdistribusi.";
      kirimPesanSekarang = true;
    }
    if (!pesanJadwalTerkirim && (waktuSekarang - waktuMulaiPompaTerjadwal >= 3000)) {
      pesanAntrian = "⏱ Memulai Pengairan SOP (HST ";
      pesanAntrian += String(hst_sekarang);
      pesanAntrian += "). Target Dosis: ";
      pesanAntrian += String(targetVolumeML);
      pesanAntrian += " ml.";
      kirimPesanSekarang = true;
      pesanJadwalTerkirim = true; 
    }
  }

  // --- PEMBACAAN SENSOR & IMPLEMENTASI RULES BUDIDAYA ---
  if (waktuSekarang - waktuSebelumnyaSensor >= jedaUpdateSensor) {
    waktuSebelumnyaSensor = waktuSekarang;

    DateTime now = rtc.now();
    int jamSekarang = now.hour();

    // --- FIX: Penerapan Field Auto Restart Status dari Web ---
    if (autoRestartStatus && now.hour() == jamAutoRestart && now.minute() == menitAutoRestart) {
      if (millis() > 300000) {
        pesanAntrian = "🔄 Melakukan penyegaran RAM rutin harian.";
        kirimPesanSekarang = true;
        delay(10000); 
        ESP.restart(); 
      }
    }

    DateTime tglTanam(TAHUN_TANAM, BULAN_TANAM, TANGGAL_TANAM, 0, 0, 0);
    TimeSpan selisihWaktu = now - tglTanam;
    hst_sekarang = selisihWaktu.days() + 1; 
    if (hst_sekarang < 1) hst_sekarang = 1; 
    
    targetVolumeML = hitungTargetVolumeDinamis(hst_sekarang);

    suhu = dht.readTemperature();
    lembab = dht.readHumidity();
    persenSoilA = constrain(map(analogRead(SOIL_PIN_A), BATAS_KERING, BATAS_BASAH, 0, 100), 0, 100);
    persenSoilB = constrain(map(analogRead(SOIL_PIN_B), BATAS_KERING, BATAS_BASAH, 0, 100), 0, 100);

    int nilaiADC_Volt = analogRead(VOLT_PIN);
    float textTeganganAKI = (nilaiADC_Volt / 4095.0) * 3.3 * faktorKalibrasi; 

    if (pertamaKaliBacaAki) {
      teganganAkiSmoothed = textTeganganAKI;
      pertamaKaliBacaAki = false;
    } else {
      teganganAkiSmoothed = (textTeganganAKI * 0.05) + (teganganAkiSmoothed * 0.95);
    }
    
    teganganAki = teganganAkiSmoothed;
    int teganganInt = round(teganganAki * 100); 
    persenAki = constrain(map(teganganInt, 1160, 1280, 0, 100), 0, 100);

    if (!isnan(suhu) && !isnan(lembab)) {
      float svp = 0.61078 * exp((17.27 * suhu) / (suhu + 237.3));
      float avp = svp * (lembab / 100.0);
      vpd = svp - avp; 
      if (suhu > suhuMax) suhuMax = suhu;
      if (suhu < suhuMin) suhuMin = suhu;
    }

    if (teganganAki < 11.5 && !peringatanAkiTerkirim && teganganAki > 5.0) {
      pesanAntrian = "⚠️ ALERT: Tegangan baterai aki kritis (";
      pesanAntrian += String(teganganAki, 1);
      pesanAntrian += "V)!";
      kirimPesanSekarang = true;
      peringatanAkiTerkirim = true; 
    } else if (teganganAki >= 12.0) {
      peringatanAkiTerkirim = false; 
    }

    if (now.hour() == 18 && now.minute() == 0) {
      if (!laporanTerkirim) {
        int totalAirSatuSiklus = targetVolumeML * penyiramanKe; 
        pesanAntrian = "📝 REKAP LOG HARIAN (HST ";
        pesanAntrian += String(hst_sekarang);
        pesanAntrian += ")\nSuhu Max: ";
        pesanAntrian += String(suhuMax, 1);
        pesanAntrian += "°C\nTotal Siram: ";
        pesanAntrian += String(penyiramanKe);
        pesanAntrian += " kali (";
        pesanAntrian += String(totalAirSatuSiklus);
        pesanAntrian += "ml)";
        
        kirimPesanSekarang = true;
        laporanTerkirim = true;  
      }
    } 
    else if (now.hour() == 0 && now.minute() == 0) {
      suhuMax = 0.0; suhuMin = 100.0; laporanTerkirim = false;
    }

    if (modeSistem == 1 || modeSistem == 2) { 
      if (!isnan(suhu) && !isnan(lembab)) {
         kontrolManajemenKipas(suhu, lembab, waktuSekarang, jamSekarang);
      }
      
      if (modeSistem == 1) { 
        if (persenSoilA < batasTanahKering || persenSoilB < batasTanahKering) {
          if (digitalRead(RELAY2) == HIGH) { 
            digitalWrite(RELAY2, LOW); catatPenyiraman();
            pesanAntrian = "💦 Pompa otomatis NYALA (Tanah kering di bawah batas)";
            kirimPesanSekarang = true;
          }
        } else if (persenSoilA > batasTanahBasah && persenSoilB > batasTanahBasah) {
          if (digitalRead(RELAY2) == LOW) {
            digitalWrite(RELAY2, HIGH);  
            pesanAntrian = "🛑 Pompa otomatis MATI (Tanah telah basah)";
            kirimPesanSekarang = true;
          }
        }
      } 
      else if (modeSistem == 2) { 
        bool waktunyaSiram = cekJadwalSiramDinamis(hst_sekarang, jamSekarang, now.minute());
        if (waktunyaSiram) {
          if (jamTerakhirDisiram != jamSekarang) {
            jamTerakhirDisiram = jamSekarang;
            preferences.putInt("jamDisiram", jamTerakhirDisiram);
            isPompaTerjadwalMenyala = true;
            waktuMulaiPompaTerjadwal = millis();
            pesanJadwalTerkirim = false; 
            digitalWrite(RELAY2, LOW); 
            catatPenyiraman(); 
          }
        }
      }
    } else {
      isPompaTerjadwalMenyala = false;
    }

    if (waktuSekarang - waktuSebelumnyaHalaman >= jedaGantiLayar) {
      waktuSebelumnyaHalaman = waktuSekarang;
      halamanAktif++;
      if (halamanAktif > 6) halamanAktif = 0; 
      jedaGantiLayar = (halamanAktif >= 3) ? 5000 : 10000; 
    }

    display.clearDisplay();
    if (halamanAktif == 0) {
      display.setTextSize(1); display.setCursor(20, 0); display.print("WAKTU SAAT INI");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(2); display.setCursor(34, 20); 
      if(now.hour() < 10) display.print('0'); display.print(now.hour(), DEC); display.print(':');
      if(now.minute() < 10) display.print('0'); display.print(now.minute(), DEC);
      display.setTextSize(1); display.setCursor(28, 45); 
      display.print(now.day(), DEC); display.print('/'); display.print(now.month(), DEC); display.print('/'); display.print(now.year(), DEC);
    } 
    else if (halamanAktif == 1) {
      display.setTextSize(1); display.setCursor(12, 0); display.print("SUHU & KELEMBABAN");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setCursor(0, 20); display.print("Suhu  : "); display.setTextSize(2); display.print(suhu, 1); display.setTextSize(1); display.print(" C");
      display.setCursor(0, 42); display.print("Lembab: "); display.setTextSize(2); display.print(lembab, 1); display.setTextSize(1); display.print(" %");
    }
    else if (halamanAktif == 2) {
      display.setTextSize(1); display.setCursor(15, 0); display.print("KELEMBABAN TANAH");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setCursor(0, 20); display.print("Sns A: "); display.setTextSize(2); display.print(persenSoilA); display.setTextSize(1); display.print("%");
      display.setCursor(0, 45); display.print("Sns B: "); display.setTextSize(2); display.print(persenSoilB); display.setTextSize(1); display.print("%");
    }
    else if (halamanAktif == 3) {
      display.setTextSize(1); display.setCursor(22, 0); display.print("MODE & KONEKSI");
      display.drawLine(0, 9, 128, 9, WHITE); display.setTextSize(2);
      if (modeSistem == 1) display.print("OTOMATIS"); 
      else if (modeSistem == 2) display.print("TERJADWAL"); 
      else display.print("MANUAL");
    }
    else if (halamanAktif == 4) {
      display.setTextSize(1); display.setCursor(18, 0); display.print("INFO PENYIRAMAN");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setCursor(0, 20); display.print("Terakhir: "); display.setTextSize(2); display.print(jamTerakhirSiram);
      display.setTextSize(1); display.setCursor(0, 45); display.print("Siraman ke-"); display.print(penyiramanKe);
    }
    else if (halamanAktif == 5) {
      display.setTextSize(1); display.setCursor(25, 0); display.print("INFO TANAMAN");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setCursor(0, 20); display.print("Usia : "); display.setTextSize(2); display.print(hst_sekarang); display.setTextSize(1); display.print(" HST");
    }
    else if (halamanAktif == 6) {
      display.setTextSize(1); display.setCursor(15, 0); display.print("INFO SISTEM DAYA");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setCursor(0, 15); display.print("Tegangan: "); display.setTextSize(2); display.print(teganganAki, 1); display.setTextSize(1); display.print(" V");
      display.setCursor(0, 35); display.print("Baterai : "); display.setTextSize(2); display.print(persenAki); display.setTextSize(1); display.print(" %");
    }
    display.display();
  }
}