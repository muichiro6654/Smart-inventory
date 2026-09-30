#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

// --- KONFIGURASI WIFI & TELEGRAM ---
const char* ssid     = "Kelas Robot";     // Ganti dengan SSID WiFi Anda
const char* password = "kumaha aa we"; // Ganti dengan Password WiFi Anda

const String BOT_TOKEN = "8678194443:AAE8WJev0El1qo9Yo6pUy22T12nBQ0AtXWo";
const String CHAT_ID   = "8954068028";

// --- KONFIGURASI PIN HARDWARE (JANGAN DIUBAH) ---
#define RXD2 16
#define TXD2 17

// Inisialisasi LCD I2C 20x4 pada GPIO 21 dan 22
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- STRUKTUR DATA PRODUK ---
struct Product {
  String barcode;
  String nama;
  int stok;
  int sold;
};

// Data Awal Produk Penjualan
Product products[] = {
  {"24567DE45rT5", "Joystick Module", 4, 0},
  {"39854GJ968bj1", "Step Up Module", 10, 0},
  {"29D69756E629", "Sensor Module", 1, 0},
  {"9696748Yu98x1t5", "Steper Mini", 4, 0},
};

const int numProducts = sizeof(products) / sizeof(products[0]);

// Variabel untuk sinkronisasi waktu (NTP)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // Waktu Indonesia Barat (WIB / UTC+7)
const int   daylightOffset_sec = 0;

// Fungsi untuk mendapatkan waktu saat ini dalam format HH:MM:SS
String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "13:55:20"; // Waktu fallback jika gagal sinkronisasi NTP
  }
  char timeBuffer[10];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  return String(timeBuffer);
}

// Fungsi Kirim Pesan ke Telegram
void sendTelegramMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TELEGRAM] WiFi tidak terhubung, pesan tidak dikirim.");
    return;
  }

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"chat_id\":\"" + CHAT_ID + "\",\"text\":\"" + message + "\",\"parse_mode\":\"Markdown\"}";
  
  int httpResponseCode = http.POST(payload);
  if (httpResponseCode > 0) {
    Serial.println("[TELEGRAM] Pesan berhasil terkirim.");
  } else {
    Serial.print("[TELEGRAM] Gagal mengirim, error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// Fungsi Menampilkan Tampilan Utama di LCD
void showIdleScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMART INVENTORY");
  lcd.setCursor(0, 1);
  lcd.print("--------------------");
  lcd.setCursor(0, 2);
  lcd.print("Scan QR/Barcode");
  lcd.setCursor(0, 3);
  lcd.print("System Ready");
}

void setup() {
  // Inisialisasi Serial Monitor
  Serial.begin(115200);
  delay(1000);

  // Inisialisasi Scanner GM66 pada Serial2
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("[SYSTEM] GM66 Scanner siap.");

  // Inisialisasi I2C & LCD 20x4
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  
  // Tampilan awal booting
  lcd.setCursor(0, 0);
  lcd.print("SMART INVENTORY");
  lcd.setCursor(0, 1);
  lcd.print("Connecting to WiFi..");
  
  // Koneksi WiFi
  WiFi.begin(ssid, password);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Terhubung!");
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
    lcd.setCursor(0, 2);
    lcd.print("WiFi Connected      ");
    
    // Sinkronisasi Waktu NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("\n[WIFI] Gagal terhubung! Sistem berjalan offline.");
    lcd.setCursor(0, 2);
    lcd.print("WiFi Failed (Offline)");
  }

  delay(2000);
  showIdleScreen();
}

void loop() {
  // Membaca data dari Scanner GM66
  if (Serial2.available()) {
    String scannedCode = Serial2.readStringUntil('\n');
    scannedCode.trim(); // Menghapus spasi atau karakter newline ekstra

    if (scannedCode.length() > 0) {
      Serial.println("\n==========================================");
      Serial.print("Barcode Terdeteksi: ");
      Serial.println(scannedCode);
      Serial.println("==========================================");

      int productIndex = -1;
      // Cari barcode di dalam daftar produk
      for (int i = 0; i < numProducts; i++) {
        if (products[i].barcode == scannedCode) {
          productIndex = i;
          break;
        }
      }

      // Jika produk ditemukan
      if (productIndex != -1) {
        String currentTime = getFormattedTime();

        if (products[productIndex].stok > 0) {
          // Stok masih ada, lakukan transaksi penjualan
          products[productIndex].stok--;
          products[productIndex].sold++;

          // Tampilkan di LCD
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("SMART INVENTORY");
          lcd.setCursor(0, 1);
          lcd.print(products[productIndex].nama.substring(0, 20)); // Batasi 20 karakter
          lcd.setCursor(0, 2);
          lcd.print("SOLD: " + String(products[productIndex].sold));
          lcd.setCursor(0, 3);
          lcd.print("SISA: " + String(products[productIndex].stok));

          // Tampilkan di Serial Monitor
          Serial.println("==========================================");
          Serial.println("HASIL PENJUALAN");
          Serial.println("==========================================");
          Serial.print("Kode      : "); Serial.println(products[productIndex].barcode);
          Serial.print("Produk    : "); Serial.println(products[productIndex].nama);
          Serial.print("Status    : SOLD\n");
          Serial.print("Sold      : "); Serial.println(products[productIndex].sold);
          Serial.print("Sisa Stok : "); Serial.println(products[productIndex].stok);
          Serial.print("Waktu     : "); Serial.println(currentTime);
          Serial.println("==========================================");

          // Kirim Notifikasi Telegram
          String tgMessage = "SMART INVENTORY\n"
                             "--------------------\n"
                             "Produk : " + products[productIndex].nama + "\n"
                             "Kode   : " + products[productIndex].barcode + "\n"
                             "Status : SOLD\n"
                             "Sold   : " + String(products[productIndex].sold) + "\n"
                             "Sisa   : " + String(products[productIndex].stok) + "\n"
                             "Waktu  : " + currentTime;
          sendTelegramMessage(tgMessage);

        } else {
          // Stok Habis (SOLD OUT)
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("SMART INVENTORY");
          lcd.setCursor(0, 1);
          lcd.print(products[productIndex].nama.substring(0, 20));
          lcd.setCursor(0, 2);
          lcd.print("SOLD OUT");
          lcd.setCursor(0, 3);
          lcd.print("SISA: 0");

          // Tampilkan di Serial Monitor
          Serial.println("==========================================");
          Serial.println("STOK HABIS");
          Serial.println("==========================================");
          Serial.print("Kode      : "); Serial.println(products[productIndex].barcode);
          Serial.print("Produk    : "); Serial.println(products[productIndex].nama);
          Serial.print("Status    : SOLD OUT\n");
          Serial.print("Sold      : "); Serial.println(products[productIndex].sold);
          Serial.print("Sisa Stok : 0\n");
          Serial.println("==========================================");

          // Kirim Notifikasi Telegram Stok Habis
          String tgMessage = "SMART INVENTORY\n"
                             "--------------------\n"
                             "Produk : " + products[productIndex].nama + "\n"
                             "Kode   : " + products[productIndex].barcode + "\n"
                             "Status : SOLD OUT\n"
                             "Sold   : " + String(products[productIndex].sold) + "\n"
                             "Sisa   : 0";
          sendTelegramMessage(tgMessage);
        }

      } else {
        // Produk Tidak Dikenal
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SMART INVENTORY");
        lcd.setCursor(0, 1);
        lcd.print("Produk Tidak Dikenal");
        lcd.setCursor(0, 2);
        lcd.print("Kode: " + scannedCode.substring(0, 14));

        Serial.println("==========================================");
        Serial.println("PERINGATAN: Barcode Tidak Dikenal!");
        Serial.print("Kode Input: "); Serial.println(scannedCode);
        Serial.println("==========================================");

        // Kirim Notifikasi Telegram Produk Tidak Dikenal
        sendTelegramMessage("SMART INVENTORY\n--------------------\nStatus: Produk Tidak Dikenal\nKode: " + scannedCode);
      }

      // Jeda jeda tampilan hasil scan sebelum kembali ke menu utama
      delay(4000);
      showIdleScreen();
    }
  }
}