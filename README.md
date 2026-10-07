# NaMiKa

NaMiKa adalah proyek monitoring kaktus mini berbasis ESP32 dan Arduino melalui PlatformIO. Perangkat membaca suhu, kelembapan udara, intensitas cahaya, dan kebasahan tanah, lalu menampilkan animasi mata pada OLED SSD1306 128×64. Sensor touch digunakan untuk berinteraksi dengan ekspresi mata.

Ambang sensor merupakan nilai awal untuk eksperimen pada kaktus gurun. Sesuaikan dengan jenis kaktus, media tanam, lokasi, dan hasil kalibrasi. Mood menggambarkan kondisi hasil pengukuran, bukan diagnosis kesehatan tanaman.

## Fitur

- Pembacaan sensor setiap 5 detik dan rata-rata bergerak 60 sampel, sekitar 5 menit.
- Tiga level mood lingkungan: 1 perlu perhatian, 2 kurang optimal, dan 3 baik.
- Animasi wakeup, sleep, happy, saccade, sad, dan mata netral.
- Deteksi malam menggunakan waktu NTP dalam zona WIB, serta deteksi gelap dari BH1750.
- Monitoring berjalan tanpa menunggu Wi-Fi tersambung.
- Peringatan tanah basah terus selama 48 jam setelah kalibrasi.
- Status kalibrasi, pengumpulan data, dan gangguan sensor pada OLED; pembacaan lengkap melalui Serial Monitor.

## Perangkat dan wiring

| Perangkat atau sinyal | Sambungan ESP32 | Keterangan |
|---|---|---|
| OLED SSD1306 128×64 | SDA GPIO21, SCL GPIO22 | Alamat I2C `0x3C` |
| BME280 | SDA GPIO21, SCL GPIO22 | Mencoba alamat `0x76`, lalu `0x77` |
| BH1750 | SDA GPIO21, SCL GPIO22 | Alamat default library `0x23` |
| Output analog sensor tanah | GPIO34 | ADC1, menggantikan GPIO5 |
| Output digital modul touch | GPIO27 | Menggantikan GPIO3; default aktif `HIGH` |
| GND seluruh modul | GND | Ground bersama |
| Kontrol pompa, jika dipasang | GPIO4 | Dicadangkan; program hanya menulis `LOW` |

OLED, BME280, dan BH1750 berbagi bus I2C. Gunakan modul yang kompatibel dengan logika 3,3 V dan sesuaikan catu dayanya dengan spesifikasi modul. Tegangan sinyal yang masuk ke GPIO ESP32 harus sesuai batas 3,3 V.

Input touch menggunakan **output digital modul touch**, bukan pembacaan sentuhan kapasitif langsung pada pin ESP32. Untuk modul aktif `LOW`, ubah `TOUCH_ACTIVE_LEVEL` menjadi `LOW`; program menyesuaikan pull-up/pull-down input.

Program belum mengendalikan penyiraman otomatis. Jika memakai driver pompa, sesuaikan polaritas aktif driver dengan keadaan GPIO4 sebelum menghubungkan pompa.

## Menjalankan proyek

1. Pasang VS Code dengan ekstensi PlatformIO IDE, atau gunakan PlatformIO Core.
2. Buka folder proyek yang berisi `platformio.ini`.
3. Hubungkan modul sesuai tabel wiring dan sambungkan ESP32 melalui USB.
4. Atur konfigurasi di [src/main.cpp](src/main.cpp), terutama kalibrasi tanah dan Wi-Fi jika digunakan.
5. Jalankan **Build**, kemudian **Upload**, dan buka **Serial Monitor** pada 115200 baud.

Perintah berikut dijalankan dari folder proyek jika `pio` tersedia di PATH:

```bash
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Jika ada beberapa perangkat serial, tentukan port saat upload dan monitor:

```bash
pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 -b 115200
```

Ganti `/dev/ttyUSB0` dengan port ESP32 Anda. Di lingkungan pengembangan proyek ini, CLI juga tersedia di `/home/miko/.platformio/penv/bin/pio`.

### Dependency

Environment `esp32dev` memakai platform `espressif32` dan framework `arduino`.

- `Adafruit SSD1306` dideklarasikan melalui `lib_deps` di [platformio.ini](platformio.ini), bersama dependency transitifnya.
- Adafruit GFX, Adafruit BME280, Adafruit Unified Sensor, dan BH1750 juga tersedia sebagai library lokal di `lib/`.
- `WiFi` dan `Wire` berasal dari framework Arduino ESP32.

Pertahankan folder library lokal ketika menyalin proyek. Library **SSD1306 OLED Display** yang juga ada di `lib/` menyediakan `SSD1306.h`; kode proyek menggunakan library **Adafruit SSD1306** dengan header `Adafruit_SSD1306.h`.

## Konfigurasi Wi-Fi dan waktu

Isi kredensial di [src/main.cpp](src/main.cpp):

```cpp
const char *ssid = "NamaWiFi";
const char *password = "PasswordWiFi";
```

SSID kosong atau nilai bawaan `YourWiFiSSID` membuat koneksi Wi-Fi tidak dijalankan. Monitoring tetap bekerja secara offline. Jika koneksi terputus, program mencoba tersambung kembali setiap 30 detik.

Waktu diambil dari `pool.ntp.org` dan `time.google.com`, dengan zona **WIB atau UTC+7**. Tidak diperlukan modul RTC untuk sinkronisasi NTP. Setelah sinkronisasi, jam tetap berjalan saat Wi-Fi terputus selama ESP32 menyala. Setelah restart, perangkat membutuhkan sinkronisasi lagi agar mengetahui waktu kalender.

Jadwal malam diatur melalui:

```cpp
constexpr int SLEEP_HOUR = 20, WAKE_HOUR = 6;
```

## Kalibrasi sensor tanah

Nilai awal `3000` dan `1300` hanya contoh. Sebelum dikalibrasi, nilai persentase tanah bersifat sementara, OLED menampilkan `Kalibrasi tanah`, dan mood lingkungan maksimal level 2.

1. Buka Serial Monitor pada 115200 baud dan amati nilai `raw` yang dicetak setiap 5 detik.
2. Ukur nilai yang stabil saat sensor berada dalam media tanam kering. Gunakan sebagai `SOIL_RAW_DRY`.
3. Siram media yang sama, biarkan air berlebih selesai mengalir, lalu ukur nilai yang stabil. Gunakan sebagai `SOIL_RAW_WET`.
4. Isi kedua nilai di `src/main.cpp`, kemudian ubah `SOIL_CALIBRATED` menjadi `true`.
5. Build dan upload ulang.

```cpp
constexpr int SOIL_RAW_DRY = 3000;  // Ganti dengan hasil pengukuran.
constexpr int SOIL_RAW_WET = 1300;  // Ganti dengan hasil pengukuran.
constexpr bool SOIL_CALIBRATED = true;
```

Konversi yang digunakan:

```text
soil = clamp(100 × (raw − dry) / (wet − dry), 0, 100)
```

Nilai ini merupakan **indeks relatif kering–basah**, bukan persentase kandungan air volumetrik. Rumus mendukung kedua arah perubahan nilai sensor. Nilai dry dan wet harus berbeda.

## Penilaian mood lingkungan

Setelah terkumpul 60 sampel valid, mood dihitung ulang setiap 5 detik menggunakan rata-rata bergerak. Level tiap sensor mengikuti tabel berikut.

| Parameter | Level 1 | Level 2 | Level 3 |
|---|---|---|---|
| Suhu | `<10°C` atau `>40°C` | `10–<18°C` atau `>32–40°C` | `18–32°C` |
| Kelembapan udara | `>80%` | `>60–80%` | `0–60%` |
| Cahaya | `<2.000 lux` | `2.000–<10.000 lux` | `≥10.000 lux` |
| Indeks tanah terkalibrasi | `>60%` selama ≥48 jam terus-menerus | `>25%` selama belum memenuhi peringatan 48 jam | `0–25%` |

Mood gabungan memakai aturan berikut, bukan rata-rata aritmetika level sensor:

1. **Level 1:** suhu ekstrem atau tanah memenuhi peringatan basah terus selama 48 jam.
2. **Level 2:** tanah belum dikalibrasi, suhu kurang optimal, tanah masih mengering, cahaya saat terjaga kurang, atau kelembapan udara tinggi.
3. **Level 3:** seluruh kondisi yang dinilai masuk kategori baik.

Kelembapan udara tinggi dan cahaya rendah saja tidak membuat mood gabungan menjadi level 1. Saat tidur, cahaya diabaikan dalam penilaian mood gabungan. Tanah kering tidak dianggap sebagai kondisi buruk dan tidak memicu pompa.

Timer tanah basah memakai pembacaan individual `soil >60%`, sedangkan level tanah memakai nilai rata-rata. Satu pembacaan tidak basah, pembacaan sensor tidak valid, atau restart akan memulai ulang timer tersebut.

## Animasi dan prioritas

| Kondisi | Animasi | Perilaku |
|---|---|---|
| Baru menyala atau keluar dari tidur | `WAKEUP` | Mata membuka sekali selama 1,2 detik |
| Malam atau gelap menetap | `SLEEP` | Mata tertutup dengan animasi `zZ` |
| Touch disentuh | `HAPPY` | Aktif selama disentuh, bertahan sekitar 3 detik setelah dilepas |
| Terjaga dan mood 3 | `SACCADE_RANDOM` | Mata bergerak ke posisi acak dengan kedipan |
| Terjaga dan mood 1–2 yang sudah dapat dinilai | `SAD` | Kelopak atas menurun dan mulut sedih |
| Data belum lengkap, sensor bermasalah, atau belum dikalibrasi | `RESET` | Mata netral; status menjelaskan penyebabnya |

Urutan prioritas adalah **happy → wakeup → sleep → kondisi lingkungan**. Karena itu, touch dapat membuat mata happy ketika tidur atau ketika lingkungan kurang baik. Setelah efeknya selesai, animasi kembali mengikuti kondisi terkini. Sleep juga tetap tampil pada malam hari walaupun mood lingkungan buruk; alasan kondisi masih dapat ditampilkan pada baris status.

Suhu ekstrem yang sudah terdeteksi tetap dapat menghasilkan sad meskipun tanah belum dikalibrasi. Pengumpulan data dan kalibrasi tidak dianggap sebagai bukti lingkungan buruk.

### Aturan tidur dan bangun

- **Malam:** waktu sudah tersinkron dan jam berada pada rentang 20.00 sampai sebelum 06.00 WIB.
- **Gelap:** cahaya `<20 lux` selama 5 menit terus-menerus, termasuk saat NTP tersedia.
- **Keluar dari kondisi gelap:** cahaya `>100 lux` selama 30 detik terus-menerus.
- **Bangun:** kondisi malam dan kondisi gelap sudah sama-sama tidak aktif. Peralihan ini memicu wakeup sekali.

Jika sudah pukul 06.00 tetapi lingkungan masih gelap, perangkat tetap tidur. Lampu terang pada malam hari tidak membatalkan tidur berdasarkan jadwal. Tanpa waktu tersinkron, deteksi tidur menggunakan cahaya sehingga ruangan gelap saat siang juga dapat memicu sleep.

## Tampilan dan log

OLED menampilkan nama perangkat, animasi aktif, serta satu baris status. Setelah data lengkap dan kalibrasi selesai, baris status bergantian setiap 4 detik antara alasan mood dan rata-rata suhu, kelembapan udara, serta indeks tanah.

Serial Monitor menampilkan data berikut:

```text
raw=... soil=...% T=...C RH=...% lux=... levels[T,H,L,S]=... mood=... sleep=... valid=... samples=.../60 calibrated=... reason=...
Animation: Happy
```

`T,H,L,S` adalah level suhu, kelembapan udara, cahaya, dan tanah. Selama data belum lengkap, level yang dicetak masih berupa nilai sementara 2. Pergantian animasi tidak menghentikan pembacaan sensor.

## Struktur proyek

```text
namika/
├── platformio.ini                Konfigurasi board, dependency, dan baud monitor
├── src/main.cpp                  Inisialisasi, sensor, Wi-Fi, waktu, dan OLED
├── include/CactusMood.h          Kalibrasi, rata-rata, timer, dan penilaian mood
├── include/EyeAnimation.h        Debounce touch, deteksi tidur, dan kontrol animasi
├── include/EyeRenderer.h         Penggambaran mata pada Adafruit SSD1306
├── lib/                         Library lokal
└── test/host/                   Pengujian logika tanpa ESP32
```

## Pengujian logika

Dengan compiler C++ `g++`, jalankan dari folder proyek:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/host/test_mood.cpp -o /tmp/namika-test-mood
/tmp/namika-test-mood
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/host/test_animation.cpp -o /tmp/namika-test-animation
/tmp/namika-test-animation
```

Pengujian mencakup ambang mood, kalibrasi dua arah, rata-rata bergerak, timer tanah basah, wakeup, sleep, prioritas touch, debounce, dan rollover timer. Build ESP32 dilakukan melalui `pio run -e esp32dev`. Pembacaan sensor, polaritas touch, dan tampilan OLED perlu diperiksa pada perangkat fisik.

## Pemecahan masalah

| Gejala | Pemeriksaan |
|---|---|
| `cannot open source file "Adafruit_SSD1306.h"` | Pastikan dependency Adafruit SSD1306 terpasang melalui build. Jalankan **PlatformIO: Rebuild C/C++ Project Index** dan **Developer: Reload Window** jika hanya editor yang belum membaca header. |
| OLED kosong | Periksa daya, GND, SDA/SCL, ukuran layar, dan alamat `0x3C`. Alamat lain memerlukan perubahan `display.begin()` di program. |
| `Cek sensor` | Periksa BME280, BH1750, koneksi I2C, dan nilai kalibrasi dry/wet. Sampel tidak valid menghapus rata-rata sehingga pengumpulan data dimulai lagi. |
| `Kalibrasi tanah` terus tampil | Isi endpoint ADC yang sesuai, set `SOIL_CALIBRATED = true`, lalu upload ulang. |
| Mata selalu happy | Periksa sambungan output digital ke GPIO27, polaritas modul, dan `TOUCH_ACTIVE_LEVEL`. |
| Jam malam tidak sesuai | Periksa kredensial, akses NTP, zona WIB, dan jadwal tidur. Tanpa sinkronisasi waktu, perangkat hanya memakai deteksi gelap. |
| Masih sleep saat pagi | Kondisi gelap harus selesai juga: cahaya `>100 lux` selama 30 detik. |
| Belum terlihat saccade | Tunggu 60 sampel valid, selesaikan kalibrasi, dan pastikan mood 3 serta tidak sedang sleep atau happy. |

Pembacaan analog yang terputus atau macet belum dideteksi secara khusus. Karena konversi ADC dibatasi ke 0–100, pemeriksaan wiring dan kewajaran nilai `raw` tetap diperlukan saat kalibrasi dan pengujian.
