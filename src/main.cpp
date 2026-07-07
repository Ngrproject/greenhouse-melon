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
volatile bool flagRestart = false; // <--- PENAMBAHAN FIX BOOTLOOP TELEGRAM

// ==========================================
// --- [BARU] VARIABEL FITUR TAMBAHAN (VPD & REKAP) ---
// ==========================================
float suhuMax = 0.0;
float suhuMin = 100.0;
bool laporanTerkirim = false;
bool peringatanAkiTerkirim = false;
float vpd = 0.0;

// ==========================================
// --- VARIABEL MANAJEMEN KIPAS ---
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
// FUNGSI CEK JADWAL JAM BERDASARKAN HST
// ==========================================
bool cekJadwalSiram(int hst, int jam, int menit) {
  // FASE 1: HST 1 - 10 (5x siram)
  // Jadwal: 07:00, 10:00, 13:00, 17:00, dan 00:15
  if (hst >= 1 && hst <= 10) {
    if (jam == 7 || jam == 10 || jam == 13 || jam == 17) return true;
    if (jam == 0 && menit >= 15) return true;
    return false;
  }
  // FASE 2: HST 11 - 17 (6x siram)
  // Jadwal: 07:00, 10:00, 11:00, 13:00, 17:00, dan 00:15
  else if (hst >= 11 && hst <= 17) {
    if (jam == 7 || jam == 10 || jam == 11 || jam == 13 || jam == 17) return true;
    if (jam == 0 && menit >= 15) return true;
    return false;
  }
  // FASE 3: HST 18 ke atas (8x siram)
  // Jadwal: 07:00, 09:00, 10:00, 11:00, 13:00, 15:00, 17:00, dan 00:15
  else {
    if (jam == 7 || jam == 9 || jam == 10 || jam == 11 || jam == 13 || jam == 15 || jam == 17) return true;
    if (jam == 0 && menit >= 15) return true;
    return false;
  }
}

// ==========================================
// FUNGSI MENCARI JADWAL SIRAM SELANJUTNYA
// ==========================================
String dapatkanJadwalSelanjutnya(int hst, int jam, int menit) {
  // Mengubah jam dan menit menjadi total menit agar mudah dibandingkan
  int waktuSekarang = (jam * 60) + menit; 
  
  // Array jadwal dalam format total menit (Contoh: 07:00 = 7 * 60 = 420)
  int jadwalFase1[] = {15, 420, 600, 780, 1020}; // 00:15, 07:00, 10:00, 13:00, 17:00
  int jadwalFase2[] = {15, 420, 600, 660, 780, 1020}; // + 11:00
  int jadwalFase3[] = {15, 420, 540, 600, 660, 780, 900, 1020}; // + 09:00, 15:00
  
  int* jadwalAktif;
  int jumlahJadwal = 0;
  
  if (hst >= 1 && hst <= 10) { jadwalAktif = jadwalFase1; jumlahJadwal = 5; } 
  else if (hst >= 11 && hst <= 17) { jadwalAktif = jadwalFase2; jumlahJadwal = 6; } 
  else { jadwalAktif = jadwalFase3; jumlahJadwal = 8; }
  
  for (int i = 0; i < jumlahJadwal; i++) {
    if (jadwalAktif[i] > waktuSekarang) {
      int j = jadwalAktif[i] / 60;
      int m = jadwalAktif[i] % 60;
      char buf[6];
      sprintf(buf, "%02d:%02d", j, m);
      return String(buf);
    }
  }
  // Jika semua jadwal hari ini sudah terlewat, maka selanjutnya adalah besok
  return "00:15 (Besok)"; 
}

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
// --- FUNGSI MANAJEMEN KIPAS HEMAT DAYA ---
// ==========================================
// [PERUBAHAN]: Menambahkan parameter 'int jamSekarang' untuk mendeteksi siang/malam
void kontrolManajemenKipas(float suhuAktual, float lembabAktual, unsigned long waktuSaatIni, int jamSekarang) {
    if (suhuAktual < 37.0 && lembabAktual < 82.0) {
        if (statusKipas != KIPAS_IDLE) {
            digitalWrite(RELAY1, HIGH); 
            statusKipas = KIPAS_IDLE;
            pesanAntrian = "✅ *Kipas Otomatis MATI*\nSuhu & Kelembaban telah kembali ke batas aman.\nSuhu: " + String(suhuAktual,1) + "°C | Lembab: " + String(lembabAktual,1) + "%";
            kirimPesanSekarang = true;
        }
        return; 
    }

    // Menentukan apakah saat ini malam hari (antara jam 18:00 sore hingga 05:59 pagi)
    bool isMalamHari = (jamSekarang >= 18 || jamSekarang < 6);

    switch (statusKipas) {
        case KIPAS_IDLE:
            if (suhuAktual >= 39.0 || lembabAktual >= 85.0) {
                digitalWrite(RELAY1, LOW); 
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_ON_CYCLE;
                pesanAntrian = "⚠️ *Kipas Otomatis NYALA*\nKondisi ekstrem terdeteksi.\nSuhu: " + String(suhuAktual,1) + "°C | Lembab: " + String(lembabAktual,1) + "%";
                kirimPesanSekarang = true;
            }
            break;

        case KIPAS_ON_CYCLE:
            // Jeda 1 jam HANYA dieksekusi JIKA MALAM HARI
            if (isMalamHari && (waktuSaatIni - waktuMulaiKipas >= DURASI_SIKLUS_KIPAS)) {
                digitalWrite(RELAY1, HIGH); 
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_OFF_CYCLE;
                pesanAntrian = "🛑 *Kipas JEDA (Hemat Baterai)*\nSiklus ON 1 jam selesai. Kipas diistirahatkan karena saat ini malam hari.";
                kirimPesanSekarang = true;
            }
            break;

        case KIPAS_OFF_CYCLE:
            // Kipas menyala kembali jika jeda 1 jam selesai, ATAU jika hari sudah pagi (sinar matahari mulai masuk)
            if (!isMalamHari || (waktuSaatIni - waktuMulaiKipas >= DURASI_SIKLUS_KIPAS)) {
                digitalWrite(RELAY1, LOW); 
                waktuMulaiKipas = waktuSaatIni;
                statusKipas = KIPAS_ON_CYCLE;
                pesanAntrian = "💨 *Kipas Otomatis NYALA KEMBALI*\nSuhu/Lembab masih di atas batas aman.";
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
      welcome += "/restart - Mulai ulang sistem\n";
      
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
      // Ambil waktu saat ini untuk memprediksi jadwal berikutnya
      DateTime nowStatus = rtc.now();
      String infoSelanjutnya = "-";
      if (modeSistem == 2) {
        infoSelanjutnya = dapatkanJadwalSelanjutnya(hst_sekarang, nowStatus.hour(), nowStatus.minute());
      } else {
        infoSelanjutnya = (modeSistem == 1) ? "Menunggu Sensor" : "Mode Manual";
      }

      String status; status.reserve(500); 
      status = "📊 Status Greenhouse:\n\n";
      status += "📅 Tgl Tanam: " + String(TANGGAL_TANAM) + "/" + String(BULAN_TANAM) + "/" + String(TAHUN_TANAM) + "\n";
      status += "🌱 Usia: " + String(hst_sekarang) + " HST\n";
      status += "🎯 Target Siram: " + String(targetVolumeML) + " ml\n";
      status += "⚙️ Debit Pompa: " + String(debitPompaPerDetik, 2) + " ml/dtk\n";
      status += "💧 Terakhir Siram: " + jamTerakhirSiram + " (Ke-" + String(penyiramanKe) + ")\n";
      status += "🔜 Selanjutnya: " + infoSelanjutnya + "\n\n"; // <--- INFO BARU MUNCUL DI SINI
      
      status += "🌡 Suhu: " + String(suhu, 1) + " °C\n";
      status += "💧 Lembab Udara: " + String(lembab, 1) + " %\n";
      status += "🧬 Angka VPD: " + String(vpd, 2) + " kPa\n";
      status += "🌱 L.Tanah A: " + String(persenSoilA) + " % | B: " + String(persenSoilB) + " %\n";
      
      // --- LOGIKA STATUS DAYA ---
      String statusDaya = "";
      if (teganganAki > 13.2) {
        statusDaya = "CHARGE ☀️";
      } else if (teganganAki >= 11.6) {
        statusDaya = "DISCHARGE 🔋";
      } else {
        statusDaya = "PLN 🔌";
      }
      
      status += "🔋 Tegangan Aki: " + String(teganganAki, 1) + " V (" + String(persenAki) + "%)\n";
      status += "⚡ Status Daya: " + statusDaya + "\n\n";
      
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
    
    // --- [PERUBAHAN FIX BOOTLOOP] ---
    else if (text == "/restart") {
      bot.sendMessage(chat_id, "🔄 Memulai ulang sistem (Restart) dari jarak jauh...\nMohon tunggu sekitar 10 detik hingga sistem kembali online.", "");
      flagRestart = true; // Hanya memberikan tanda (flag), ESP32 tidak langsung dimatikan di sini
    }
  }
}

// ==========================================
// TASK KHUSUS CORE 0 (TELEGRAM SAJA)
// ==========================================
void TaskTelegramCode( void * pvParameters ) {
  for(;;) {
    if (!isOfflineMode && WiFi.status() == WL_CONNECTED) {
      // KODE BARU (ANTI-DOUBLE SEND):
      if (kirimPesanSekarang && pesanAntrian != "") {
        // 1. Kunci dan amankan pesan ke memori lokal
        String pesanDikirim = pesanAntrian; 
        
        // 2. Segera kosongkan antrean global agar Core 1 aman
        pesanAntrian = ""; 
        kirimPesanSekarang = false; 
        
        // 3. Eksekusi pengiriman pesan (bebas hambatan/delay)
        bot.sendMessage(CHAT_ID, pesanDikirim, "Markdown"); 
      }
      
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      while (numNewMessages) {
        handleNewMessages(numNewMessages);
        // Di baris inilah server Telegram diberitahu bahwa pesan telah selesai dibaca
        numNewMessages = bot.getUpdates(bot.last_message_received + 1); 
      }
      
      // --- [PERUBAHAN FIX BOOTLOOP] EKSEKUSI RESTART YANG AMAN ---
      if (flagRestart) {
        delay(10000); // Waktu ekstra 10 detik untuk mengatasi Wi-Fi lambat
        ESP.restart();
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
  digitalWrite(RELAY1, HIGH); 
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

    // FITUR AUTO-RESTART (PEMBERSIHAN MEMORI SETIAP HARI JAM 05:00)
    if (now.hour() == 6 && now.minute() == 0) {
      if (millis() > 300000) {
        pesanAntrian = "🔄 *Maintenance Sistem*\nMelakukan Auto-Restart rutin harian untuk menyegarkan memori RAM. Sistem akan kembali online dalam beberapa detik...";
        kirimPesanSekarang = true;
        delay(10000); 
        ESP.restart(); 
      }
    }

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
    // [BARU] FITUR ANALISIS VPD, PERINGATAN BATERAI & REKAP
    // ======================================================
    
    // 1. MENGHITUNG VPD & PENCATATAN SUHU
    if (!isnan(suhu) && !isnan(lembab)) {
      float svp = 0.61078 * exp((17.27 * suhu) / (suhu + 237.3));
      float avp = svp * (lembab / 100.0);
      vpd = svp - avp; 
      
      if (suhu > suhuMax) suhuMax = suhu;
      if (suhu < suhuMin) suhuMin = suhu;
    }

    // 2. PERINGATAN BATERAI KRITIS
    if (teganganAki < 11.5 && !peringatanAkiTerkirim && teganganAki > 5.0) {
      pesanAntrian = "⚠️ *PERINGATAN DAYA KRITIS* ⚠️\nTegangan aki turun hingga " + String(teganganAki, 1) + "V.\nMohon periksa sistem kelistrikan (Panel Surya / ATS)!";
      kirimPesanSekarang = true;
      peringatanAkiTerkirim = true; 
    } else if (teganganAki >= 12.0) {
      peringatanAkiTerkirim = false; 
    }

    // 3. LAPORAN HARIAN OTOMATIS (Setiap jam 18:00)
    if (now.hour() == 18 && now.minute() == 0) {
      if (!laporanTerkirim) {
        int totalAirSatuSiklus = targetVolumeML * penyiramanKe; 
        
        pesanAntrian = "📝 *REKAP HARIAN GREENFLOW* (HST " + String(hst_sekarang) + ")\n\n";
        pesanAntrian += "🌡 Suhu Tertinggi: " + String(suhuMax, 1) + " °C\n";
        pesanAntrian += "🌡 Suhu Terendah : " + String(suhuMin, 1) + " °C\n";
        pesanAntrian += "💦 Total Siram   : " + String(penyiramanKe) + " kali\n";
        pesanAntrian += "💧 Estimasi Air  : " + String(totalAirSatuSiklus) + " ml\n\n";
        pesanAntrian += "Selamat beristirahat! 🌙";
        
        kirimPesanSekarang = true;
        laporanTerkirim = true; 
      }
    } 
    // Reset variabel rekap di tengah malam (Jam 00:00)
    else if (now.hour() == 0 && now.minute() == 0) {
      suhuMax = 0.0;
      suhuMin = 100.0;
      laporanTerkirim = false;
    }
    // ======================================================

    // ======================================================
    // EKSEKUSI LOGIKA KIPAS & POMPA BERDASARKAN MODE
    // ======================================================
    if (modeSistem == 1 || modeSistem == 2) { 
      if (!isnan(suhu) && !isnan(lembab)) {
         // Memasukkan variabel jamSekarang agar sistem tahu siang/malam
         kontrolManajemenKipas(suhu, lembab, waktuSekarang, jamSekarang);
      }
      
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
      else if (modeSistem == 2) {
        bool waktunyaSiram = cekJadwalSiram(hst_sekarang, jamSekarang, now.minute());
        
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
    else if (halamanAktif == 2) {
      display.setTextSize(1); display.setCursor(15, 0); display.print("KELEMBABAN TANAH");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(1); display.setCursor(0, 20); display.print("Sns A: ");
      display.setTextSize(2); display.print(persenSoilA); display.setTextSize(1); display.print("%");
      display.setTextSize(1); display.setCursor(0, 45); display.print("Sns B: ");
      display.setTextSize(2); display.print(persenSoilB); display.setTextSize(1); display.print("%");
    }
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
    else if (halamanAktif == 4) {
      display.setTextSize(1); display.setCursor(18, 0); display.print("INFO PENYIRAMAN");
      display.drawLine(0, 9, 128, 9, WHITE);
      display.setTextSize(1); display.setCursor(0, 20); display.print("Terakhir: ");
      display.setTextSize(2); display.print(jamTerakhirSiram);
      display.setTextSize(1); display.setCursor(0, 45); 
      if (penyiramanKe == 0) { display.print("Belum ada penyiraman"); } 
      else { display.print("Siraman ke-"); display.print(penyiramanKe); display.print(" hari ini"); }
    }
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
    else if (halamanAktif == 6) {
      display.setTextSize(1); display.setCursor(15, 0); display.print("INFO SISTEM DAYA");
      display.drawLine(0, 9, 128, 9, WHITE);
      
      // Baris 1: Tegangan
      display.setTextSize(1); display.setCursor(0, 15); display.print("Tegangan: ");
      display.setTextSize(2); display.print(teganganAki, 1); display.setTextSize(1); display.print(" V");
      
      // Baris 2: Kapasitas Baterai
      display.setTextSize(1); display.setCursor(0, 35); display.print("Baterai : ");
      display.setTextSize(2); display.print(persenAki); display.setTextSize(1); display.print(" %");

      // Baris 3: Status Daya (PLN/Charge/Discharge)
      display.setTextSize(1); display.setCursor(0, 55); display.print("Status  : ");
      if (teganganAki > 13.2) {
        display.print("CHARGE");
      } else if (teganganAki >= 11.6) {
        display.print("DISCHARGE");
      } else {
        display.print("PLN");
      }
    }
}