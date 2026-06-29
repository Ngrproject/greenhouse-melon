#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <DHT.h>
#include <Preferences.h> 

// --- TAMBAHAN LIBRARY TELEGRAM & WIFI ---
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> 

// --- KONFIGURASI TELEGRAM ---
#define BOT_TOKEN "8967102523:AAFrH6Wu4AIudm8s_8Q8cnSSTmX-4TQo5Ao"
#define CHAT_ID "916594429"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
Preferences preferences; 

bool isOfflineMode = false; 
volatile uint8_t modeSistem = 2; // Bawaan: Terjadwal

// --- VARIABEL TANGGAL TANAM ---
int TAHUN_TANAM;
int BULAN_TANAM;
int TANGGAL_TANAM;
int hst_sekarang = 0; 

// --- VARIABEL INFO PENYIRAMAN ---
int penyiramanKe = 0;
int tanggalSistemUntukReset = -1; 
String jamTerakhirSiram = "--:--";

// --- VARIABEL POMPA ---
float debitPompaPerDetik = 3.0; 
int targetVolumeML = 250; 
bool isPompaTerjadwalMenyala = false;
unsigned long waktuMulaiPompaTerjadwal = 0;
int jamTerakhirDisiram = -1; 
bool pesanJadwalTerkirim = true; 

// --- VARIABEL ANTRIAN PESAN TELEGRAM ---
String pesanAntrian = ""; 
volatile bool kirimPesanSekarang = false;

// ==========================================
// --- [BARU] VARIABEL MANAJEMEN KIPAS ---
// ==========================================
enum StateKipas { KIPAS_IDLE, KIPAS_ON_CYCLE, KIPAS_OFF_CYCLE };
StateKipas statusKipas = KIPAS_IDLE;
unsigned long waktuMulaiKipas = 0;
const unsigned long DURASI_SIKLUS_KIPAS = 3600000; // 1 Jam (3.600.000 ms)

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
  // ... (KODE BITMAP LOGO ANDA TETAP SAMA) ...
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
const unsigned long jedaCekWiFi = 120000; 

// ==========================================
// FUNGSI MENCATAT INFO PENYIRAMAN TERAKHIR
// ==========================================
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

// ==========================================
// FUNGSI HITUNG TARGET ML BERDASARKAN HST
// ==========================================
int hitungTargetVolume(int hst) {
  if (hst == 1) return 156; 
  else if (hst >= 2 && hst <= 5) return 250;
  else if (hst >= 6 && hst <= 10) return 300;
  else if (hst >= 11 && hst <= 17) return 292; 
  else if (hst >= 18 && hst <= 25) return 219; 
  else if (hst >= 26 && hst <= 35) return 250;
  else if (hst >= 36 && hst <= 54) return 250;
  else if (hst >= 55 && hst <= 59) return 219;
  else if (hst >= 60 && hst <= 64) return 188; 
  else if (hst >= 65 && hst <= 79) return 156; 
  else if (hst >= 80) return 125;
  return 250; 
}

// ==========================================
// --- [BARU] FUNGSI MANAJEMEN KIPAS HEMAT DAYA ---
// ==========================================
void kontrolManajemenKipas(float suhuAktual, float lembabAktual, unsigned long waktuSaatIni) {
    // 1. Cek Histeresis Bawah (Batas Aman Mutlak)
    if (suhuAktual < 38.0 && lembabAktual < 82.0) {
        if (statusKipas != KIPAS_IDLE) {
            digitalWrite(RELAY1, HIGH); // Matikan Kipas (Active LOW)
            statusKipas = KIPAS_IDLE;
            pesanAntrian = "✅ *Kipas Otomatis MATI*\nSuhu & Kelembaban telah kembali ke batas aman.\nSuhu: " + String(suhuAktual,1) + "°C | Lembab: " + String(lembabAktual,1) + "%";
            kirimPesanSekarang = true;
        }
        return; 
    }

    // 2. State Machine Siklus Kipas
    switch (statusKipas) {
        case KIPAS_IDLE:
            if (suhuAktual >= 40.0 || lembabAktual >= 85.0) {
                digitalWrite(RELAY1, LOW); // Nyalakan Kipas
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_ON_CYCLE;
                pesanAntrian = "⚠️ *Kipas Otomatis NYALA*\nKondisi ekstrem terdeteksi. Memulai siklus ON 1 Jam.\nSuhu: " + String(suhuAktual,1) + "°C | Lembab: " + String(lembabAktual,1) + "%";
                kirimPesanSekarang = true;
            }
            break;

        case KIPAS_ON_CYCLE:
            if (waktuSaatIni - waktuMulaiKipas >= DURASI_SIKLUS_KIPAS) {
                digitalWrite(RELAY1, HIGH); // Matikan Kipas Sementara
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_OFF_CYCLE;
                pesanAntrian = "🛑 *Kipas JEDA (Hemat Baterai)*\nSiklus ON 1 jam selesai. Kipas diistirahatkan selama 1 jam ke depan.";
                kirimPesanSekarang = true;
            }
            break;

        case KIPAS_OFF_CYCLE:
            if (waktuSaatIni - waktuMulaiKipas >= DURASI_SIKLUS_KIPAS) {
                digitalWrite(RELAY1, LOW); // Nyalakan Kipas Kembali
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_ON_CYCLE;
                pesanAntrian = "💨 *Kipas Otomatis NYALA KEMBALI*\nJeda selesai. Suhu/Lembab masih di atas batas aman. Memulai siklus ON berikutnya.";
                kirimPesanSekarang = true;
            }
            break;
    }
}

// ==========================================
// FUNGSI MEMBACA PESAN TELEGRAM
// ==========================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) { bot.sendMessage(chat_id, "Akses ditolak!", ""); continue; }
    String text = bot.messages[i].text;

if (text == "/start") {
      String welcome; welcome.reserve(500); 
      welcome = "Sistem Greenhouse Melon Capstone!\n\n";
      welcome += "Pilih Mode Operasi:\n";
      welcome += "/auto - Mode Otomatis\n";
      welcome += "/jadwal - Mode Terjadwal\n";
      welcome += "/manual - Mode Manual\n\n";
      
      welcome += "--- Manajemen Tanam ---\n";
      welcome += "Ubah tanggal tanam dengan format:\n";
      welcome += "/settanggal YYYY-MM-DD\n";
      welcome += "(Contoh: /settanggal 2026-06-19)\n\n";
      
      welcome += "--- Kalibrasi Pompa ---\n";
      welcome += "Ketik detik yg dibutuhkan utk 100ml air:\n";
      welcome += "/kalibrasi [detik] (Contoh: /kalibrasi 33.5)\n\n";
      
      welcome += "--- Perintah Lain ---\n";
      welcome += "/status - Cek kondisi terkini\n";
      welcome += "/kipas_on | /kipas_off\n";
      welcome += "/pompa_on | /pompa_off\n";
      
      bot.sendMessage(chat_id, welcome, "");
    }
    else if (text.startsWith("/settanggal ")) {
      String dateStr = text.substring(12);
      dateStr.trim();
      if (dateStr.length() == 10 && dateStr.charAt(4) == '-' && dateStr.charAt(7) == '-') {
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
        bot.sendMessage(chat_id, "✅ Tanggal Tanam diubah menjadi: " + dateStr + "\n🔄 Histori di-reset!", "");
      } else {
        bot.sendMessage(chat_id, "⚠️ Format salah!\nGunakan format YYYY-MM-DD", "");
      }
    }
    else if (text.startsWith("/kalibrasi ")) {
      String detikStr = text.substring(11); 
      detikStr.trim();
      float detikMasuk = detikStr.toFloat();
      if (detikMasuk > 0) {
        debitPompaPerDetik = 100.0 / detikMasuk;
        preferences.putFloat("debitPompa", debitPompaPerDetik);
        String balas = "✅ Kalibrasi Pompa Berhasil!\n";
        balas += "Waktu 100ml : " + String(detikMasuk, 1) + " detik\n";
        balas += "Debit Baru : " + String(debitPompaPerDetik, 2) + " ml/detik\n";
        bot.sendMessage(chat_id, balas, "");
      } else {
        bot.sendMessage(chat_id, "⚠️ Format salah! Contoh: /kalibrasi 33.5", "");
      }
    }
    else if (text == "/status") {
      String status; status.reserve(500); 
      status = "📊 Status Greenhouse:\n\n";
      status += "📅 Tgl Tanam: " + String(TANGGAL_TANAM) + "/" + String(BULAN_TANAM) + "/" + String(TAHUN_TANAM) + "\n";
      status += "🌱 Usia: " + String(hst_sekarang) + " HST\n";
      status += "🎯 Target Siram: " + String(targetVolumeML) + " ml\n";
      status += "⚙️ Debit Pompa: " + String(debitPompaPerDetik, 2) + " ml/dtk\n";
      status += "💧 Terakhir Siram: " + jamTerakhirSiram + " (Ke-" + String(penyiramanKe) + ")\n\n";
      status += "🌡 Suhu: " + String(suhu, 1) + " °C\n";
      status += "💧 Lembab Udara: " + String(lembab, 1) + " %\n";
      status += "🌱 L.Tanah A: " + String(persenSoilA) + " % | B: " + String(persenSoilB) + " %\n";
      status += "🔋 Tegangan Aki: " + String(teganganAki, 1) + " V (" + String(persenAki) + "%)\n\n";
      String namaMode = (modeSistem == 1) ? "OTOMATIS (SENSOR)" : (modeSistem == 2) ? "TERJADWAL" : "MANUAL";
      status += "⚙️ Mode: " + namaMode + "\n";
      status += "💨 Kipas: " + String(digitalRead(RELAY1) == LOW ? "NYALA" : "MATI") + "\n";
      status += "💦 Pompa: " + String(digitalRead(RELAY2) == LOW ? "NYALA" : "MATI") + "\n";
      status += "📶 Wi-Fi: " + WiFi.SSID() + "\n";
      bot.sendMessage(chat_id, status, "");
    }
    else if (text == "/auto") { modeSistem = 1; bot.sendMessage(chat_id, "✅ Sistem diubah ke Mode SENSOR.", ""); }
    else if (text == "/jadwal") { modeSistem = 2; bot.sendMessage(chat_id, "⏱ Sistem diubah ke Mode JADWAL (SOP Nutrisi).", ""); }
    else if (text == "/manual") { modeSistem = 0; statusKipas = KIPAS_IDLE; bot.sendMessage(chat_id, "⚙️ Sistem diubah ke Mode MANUAL.", ""); }
    
    else if (text == "/kipas_on") { if(modeSistem == 0) { digitalWrite(RELAY1, LOW); bot.sendMessage(chat_id, "💨 Kipas NYALA.", ""); } else { bot.sendMessage(chat_id, "⚠️ Ubah ke /manual dulu!", ""); } }
    else if (text == "/kipas_off") { if(modeSistem == 0) { digitalWrite(RELAY1, HIGH); bot.sendMessage(chat_id, "🛑 Kipas MATI.", ""); } else { bot.sendMessage(chat_id, "⚠️ Ubah ke /manual dulu!", ""); } }
    else if (text == "/pompa_on") { 
      if(modeSistem == 0) { 
        if (digitalRead(RELAY2) == HIGH) { digitalWrite(RELAY2, LOW); catatPenyiraman(); }
        bot.sendMessage(chat_id, "💦 Pompa NYALA.", ""); 
      } else { bot.sendMessage(chat_id, "⚠️ Ubah ke /manual dulu!", ""); } 
    }
    else if (text == "/pompa_off") { if(modeSistem == 0) { digitalWrite(RELAY2, HIGH); bot.sendMessage(chat_id, "🛑 Pompa MATI.", ""); } else { bot.sendMessage(chat_id, "⚠️ Ubah ke /manual dulu!", ""); } }
  }
}

// ==========================================
// TASK KHUSUS CORE 0 (TELEGRAM SAJA)
// ==========================================
void TaskTelegramCode( void * pvParameters ) {
  for(;;) {
    if (!isOfflineMode && WiFi.status() == WL_CONNECTED) {
      if (kirimPesanSekarang && pesanAntrian != "") {
        bot.sendMessage(CHAT_ID, pesanAntrian, "Markdown"); // Menggunakan mode Markdown
        pesanAntrian = ""; 
        kirimPesanSekarang = false; 
      }
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      while (numNewMessages) {
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  
  preferences.begin("greenhouse", false);
  TAHUN_TANAM = preferences.getInt("tahun", 2026);
  BULAN_TANAM = preferences.getInt("bulan", 5);
  TANGGAL_TANAM = preferences.getInt("tanggal", 20);
  debitPompaPerDetik = preferences.getFloat("debitPompa", 3.0); 
  penyiramanKe = preferences.getInt("siramKe", 0);
  tanggalSistemUntukReset = preferences.getInt("tglReset", -1);
  jamTerakhirSiram = preferences.getString("jamSiram", "--:--");
  jamTerakhirDisiram = preferences.getInt("jamDisiram", -1);

  Wire.begin(21, 22);
  Wire.setClock(400000); 

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, HIGH); // Active LOW: HIGH = Mati
  digitalWrite(RELAY2, HIGH);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20); display.print("Mencari Wi-Fi...");
  display.setCursor(0, 40); display.print("GreenFlow-Config");
  display.display();

  WiFiManager wm;
  wm.setConfigPortalTimeout(180); 

  if (!wm.autoConnect("GreenFlow-Config")) {
    Serial.println("Gagal terhubung Wi-Fi atau Timeout! Menjalankan mode Offline...");
    isOfflineMode = true; 
    display.clearDisplay();
    display.setCursor(5, 20); display.print("Wi-Fi Gagal!");
    display.setCursor(5, 40); display.print("Mode OFFLINE Aktif");
    display.display();
    delay(3000);
  } else {
    Serial.println("Koneksi Wi-Fi Sukses!");
    isOfflineMode = false;
    secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
    bot.sendMessage(CHAT_ID, "🚀 Sistem GreenFlow Online! /start", "");
  }

  xTaskCreatePinnedToCore(TaskTelegramCode, "TaskTelegram", 10000, NULL, 1, NULL, 0);

  dht.begin(); 
  rtc.begin();

  display.clearDisplay(); display.setCursor(30, 20); display.print("Monitoring"); display.setCursor(15, 35); display.print("Green House Melon"); display.display(); delay(2000); 
  display.clearDisplay(); display.drawBitmap(0, 0, epd_bitmap_logo, 128, 64, WHITE); display.display(); delay(3000); 
}

// ==========================================
// CORE 1 (MURNI SENSOR & LAYAR)
// ==========================================
void loop() {
  unsigned long waktuSekarang = millis();

  if (waktuSekarang - waktuSebelumnyaWiFi >= jedaCekWiFi) {
    waktuSebelumnyaWiFi = waktuSekarang;
    if (WiFi.status() != WL_CONNECTED) {
      if (!isOfflineMode) { isOfflineMode = true; }
      WiFi.reconnect(); 
    } else {
      if (isOfflineMode) {
        isOfflineMode = false; 
        pesanAntrian = "🔄 Sistem GreenFlow kembali ONLINE!";
        kirimPesanSekarang = true;
      }
    }
  }

  // --- LOGIKA POMPA TERJADWAL ---
  if (modeSistem == 2 && isPompaTerjadwalMenyala) {
    unsigned long durasiSiramMS = ((targetVolumeML / debitPompaPerDetik) * 1000) + 2000;
    
    if (waktuSekarang - waktuMulaiPompaTerjadwal >= durasiSiramMS) {
      isPompaTerjadwalMenyala = false;
      digitalWrite(RELAY2, HIGH); 
      pesanAntrian = "✅ Jadwal Selesai. " + String(targetVolumeML) + " ml diberikan.";
      kirimPesanSekarang = true;
    }
    
    if (!pesanJadwalTerkirim && (waktuSekarang - waktuMulaiPompaTerjadwal >= 3000)) {
      pesanAntrian = "⏱ Jadwal Dimulai (HST " + String(hst_sekarang) + ").\nTarget: " + String(targetVolumeML) + " ml.";
      kirimPesanSekarang = true;
      pesanJadwalTerkirim = true; 
    }
  }

  // --- PEMBACAAN SENSOR 2 DETIK SEKALI ---
  if (waktuSekarang - waktuSebelumnyaSensor >= jedaUpdateSensor) {
    waktuSebelumnyaSensor = waktuSekarang;

    DateTime now = rtc.now();
    int jamSekarang = now.hour();

    // FITUR AUTO-RESTART (PEMBERSIHAN MEMORI 2 HARI SEKALI)
    // ======================================================
    if (now.day() % 2 == 0 && now.hour() == 2 && now.minute() == 0) {
      if (millis() > 300000) {
        pesanAntrian = "🔄 *Maintenance Sistem*\nMelakukan Auto-Restart rutin untuk menyegarkan memori RAM. Sistem akan kembali online dalam beberapa detik...";
        kirimPesanSekarang = true;
        
        delay(5000); 
        ESP.restart(); 
      }
    }
    // ======================================================

    DateTime tglTanam(TAHUN_TANAM, BULAN_TANAM, TANGGAL_TANAM, 0, 0, 0);
    TimeSpan selisihWaktu = now - tglTanam;
    hst_sekarang = selisihWaktu.days() + 1; 
    if (hst_sekarang < 1) hst_sekarang = 1; 
    targetVolumeML = hitungTargetVolume(hst_sekarang);

    suhu = dht.readTemperature();
    lembab = dht.readHumidity();
    persenSoilA = constrain(map(analogRead(SOIL_PIN_A), BATAS_KERING, BATAS_BASAH, 0, 100), 0, 100);
    persenSoilB = constrain(map(analogRead(SOIL_PIN_B), BATAS_KERING, BATAS_BASAH, 0, 100), 0, 100);

    int nilaiADC_Volt = analogRead(VOLT_PIN);
    float teganganAkiMentah = (nilaiADC_Volt / 4095.0) * 3.3 * faktorKalibrasi;

    if (pertamaKaliBacaAki) {
      teganganAkiSmoothed = teganganAkiMentah; 
      pertamaKaliBacaAki = false;
    } else {
      teganganAkiSmoothed = (teganganAkiMentah * 0.05) + (teganganAkiSmoothed * 0.95);
    }
    
    teganganAki = teganganAkiSmoothed;
    int teganganInt = round(teganganAki * 100); 
    persenAki = constrain(map(teganganInt, 1160, 1280, 0, 100), 0, 100);

    // ======================================================
    // EKSEKUSI LOGIKA KIPAS & POMPA BERDASARKAN MODE
    // ======================================================
    if (modeSistem == 1 || modeSistem == 2) { 
      // Kipas diatur oleh State Machine Hemat Daya (Baik di Mode Auto maupun Jadwal)
      if (!isnan(suhu) && !isnan(lembab)) {
         kontrolManajemenKipas(suhu, lembab, waktuSekarang);
      }
      
      // Logika Pompa Mode Auto
      if (modeSistem == 1) {
        if (persenSoilA < 45 || persenSoilB < 45) {
          if (digitalRead(RELAY2) == HIGH) { 
            digitalWrite(RELAY2, LOW);
            catatPenyiraman();
            pesanAntrian = "💦 Pompa otomatis NYALA (Tanah kering)";
            kirimPesanSekarang = true;
          }
        } else if (persenSoilA > 75 && persenSoilB > 75) {
          if (digitalRead(RELAY2) == LOW) {
            digitalWrite(RELAY2, HIGH);  
            pesanAntrian = "🛑 Pompa otomatis MATI (Tanah sudah basah)";
            kirimPesanSekarang = true;
          }
        }
      } 
      // Logika Pompa Mode Terjadwal
      else if (modeSistem == 2) {
        bool waktuSiramSiang = (jamSekarang == 7 || jamSekarang == 9 || jamSekarang == 10 || 
                                jamSekarang == 11 || jamSekarang == 13 || jamSekarang == 15 || jamSekarang == 17);
        bool waktuSiramMalam = (jamSekarang == 0 && now.minute() >= 15);
        
        if (waktuSiramSiang || waktuSiramMalam) {
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
      // Jika Mode Manual (modeSistem == 0), pastikan variabel pompa otomatis mati
      isPompaTerjadwalMenyala = false;
    }

    // --- PENGELOLAAN LAYAR OLED ---
    if (waktuSekarang - waktuSebelumnyaHalaman >= jedaGantiLayar) {
      waktuSekarang = millis();
      waktuSebelumnyaHalaman = waktuSekarang;
      halamanAktif++;
      if (halamanAktif > 6) halamanAktif = 0; 
      
      if (halamanAktif >= 3) jedaGantiLayar = 5000;  
      else jedaGantiLayar = 10000; 
    }

    display.clearDisplay();

    // Halaman 0
    if (halamanAktif == 0) {
      display.setTextSize(1); display.setCursor(20, 0); display.print("WAKTU SAAT INI");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(2); display.setCursor(34, 20); 
      if(now.hour() < 10) display.print('0'); display.print(now.hour(), DEC); display.print(':');
      if(now.minute() < 10) display.print('0'); display.print(now.minute(), DEC);
      display.setTextSize(1); display.setCursor(28, 45); 
      if(now.day() < 10) display.print('0'); display.print(now.day(), DEC); display.print('/');
      if(now.month() < 10) display.print('0'); display.print(now.month(), DEC); display.print('/');
      display.print(now.year(), DEC);
      if(isOfflineMode) { display.setCursor(110, 0); display.print("[X]"); }
    } 
    // Halaman 1
    else if (halamanAktif == 1) {
      display.setTextSize(1); display.setCursor(12, 0); display.print("SUHU & KELEMBABAN");
      display.drawLine(0, 9, 128, 9, WHITE);
      if (isnan(suhu) || isnan(lembab)) {
        display.setCursor(10, 30); display.print("Gagal baca sensor!");
      } else {
        display.setTextSize(1); display.setCursor(0, 20); display.print("Suhu  : ");
        display.setTextSize(2); display.print(suhu, 1); 
        display.setTextSize(1); display.print(" C"); display.drawCircle(113, 20, 2, WHITE); 
        display.setTextSize(1); display.setCursor(0, 42); display.print("Lembab: ");
        display.setTextSize(2); display.print(lembab, 1);
        display.setTextSize(1); display.print(" %");
      }
    }
    // Halaman 2
    else if (halamanAktif == 2) {
      display.setTextSize(1); display.setCursor(15, 0); display.print("KELEMBABAN TANAH");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(1); display.setCursor(0, 20); display.print("Sns A: ");
      display.setTextSize(2); display.print(persenSoilA); display.setTextSize(1); display.print("%");
      display.setTextSize(1); display.setCursor(0, 45); display.print("Sns B: ");
      display.setTextSize(2); display.print(persenSoilB); display.setTextSize(1); display.print("%");
    }
    // Halaman 3
    else if (halamanAktif == 3) {
      display.setTextSize(1); display.setCursor(22, 0); display.print("MODE & KONEKSI");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(2);
      if (modeSistem == 1) { display.setCursor(15, 20); display.print("OTOMATIS"); } 
      else if (modeSistem == 2) { display.setCursor(10, 20); display.print("TERJADWAL"); } 
      else { display.setCursor(30, 20); display.print("MANUAL"); }
      display.setTextSize(1);
      if (isOfflineMode) { display.setCursor(15, 48); display.print("Jaringan: OFFLINE"); } 
      else { display.setCursor(18, 48); display.print("Jaringan: ONLINE"); }
    }
    // Halaman 4
    else if (halamanAktif == 4) {
      display.setTextSize(1); display.setCursor(18, 0); display.print("INFO PENYIRAMAN");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(1); display.setCursor(0, 20); display.print("Terakhir: ");
      display.setTextSize(2); display.print(jamTerakhirSiram);
      display.setTextSize(1); display.setCursor(0, 45); 
      if (penyiramanKe == 0) { display.print("Belum ada penyiraman"); } 
      else { display.print("Siraman ke-"); display.print(penyiramanKe); display.print(" hari ini"); }
    }
    // Halaman 5
    else if (halamanAktif == 5) {
      display.setTextSize(1); display.setCursor(25, 0); display.print("INFO TANAMAN");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(1); display.setCursor(0, 20); display.print("Tanam: ");
      if(TANGGAL_TANAM < 10) display.print('0'); display.print(TANGGAL_TANAM); display.print('/');
      if(BULAN_TANAM < 10) display.print('0'); display.print(BULAN_TANAM); display.print('/');
      display.print(TAHUN_TANAM);
      display.setTextSize(1); display.setCursor(0, 45); display.print("Usia : ");
      display.setTextSize(2); display.print(hst_sekarang); display.setTextSize(1); display.print(" HST");
    }
    // Halaman 6
    else if (halamanAktif == 6) {
      display.setTextSize(1); display.setCursor(15, 0); display.print("INFO SISTEM DAYA");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(1); display.setCursor(0, 20); display.print("Tegangan: ");
      display.setTextSize(2); display.print(teganganAki, 1); display.setTextSize(1); display.print(" V");
      display.setTextSize(1); display.setCursor(0, 45); display.print("Kapasitas:");
      display.setTextSize(2); display.print(persenAki); display.setTextSize(1); display.print(" %");
    }
    display.display();
  }
}