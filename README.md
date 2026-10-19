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
| Kontrol pompa, jika dipasang | GPIO4 | Sinyal kontrol driver pompa; polaritas melalui `PUMP_ACTIVE_LEVEL` |

OLED, BME280, dan BH1750 berbagi bus I2C. Gunakan modul yang kompatibel dengan logika 3,3 V dan sesuaikan catu dayanya dengan spesifikasi modul. Tegangan sinyal yang masuk ke GPIO ESP32 harus sesuai batas 3,3 V.

Input touch menggunakan **output digital modul touch**, bukan pembacaan sentuhan kapasitif langsung pada pin ESP32. Untuk modul aktif `LOW`, ubah `TOUCH_ACTIVE_LEVEL` menjadi `LOW`; program menyesuaikan pull-up/pull-down input.

Program mendukung penyiraman otomatis setelah kalibrasi tanah. Hubungkan pompa melalui driver MOSFET dengan proteksi beban induktif atau modul relay yang sesuai, bukan langsung ke GPIO. Gunakan catu daya sesuai pompa. Sesuaikan `PUMP_ACTIVE_LEVEL` (`HIGH` atau `LOW`) dengan driver. Pastikan driver tetap dalam kondisi mati saat ESP32 boot/reset menggunakan bias input yang sesuai pada rangkaian.

## Penyiraman otomatis

`AUTO_WATERING_ENABLED = true` dan `SOIL_CALIBRATED = true` diperlukan untuk mengaktifkan penyiraman. Nilai bawaan kalibrasi masih `false`, sehingga pompa tetap mati sampai endpoint ADC diukur dan konfigurasi diubah.

Pengaturan awal `WATERING_CONFIG` menggunakan nilai bawaan pada `include/WateringController.h`:

| Pengaturan | Nilai awal | Perilaku |
|---|---|---|
| `dry` / `dryMs` | 15% / 5 menit | Seluruh pembacaan harus di bawah 15% selama 5 menit |
| `wet` | 25% | Target untuk menyelesaikan satu siklus |
| `pulseMs` | 1 detik | Durasi maksimum setiap pulsa pompa |
| `soakMs` | 5 menit | Jeda pompa mati sebelum mengevaluasi pembacaan baru |
| `maxPulses` | 3 | Maksimum pulsa per siklus |
| `cooldownMs` | 24 jam | Jeda setelah siklus berhasil, lalu konfirmasi kering dimulai lagi |

Ini adalah parameter eksperimen, bukan dosis penyiraman universal untuk kaktus. Ukur debit pompa dan sesuaikan durasi dengan pot, media, dan tanaman. Keputusan memakai pembacaan individual tanah, terpisah dari rata-rata mood. Pada pulsa berikutnya dalam siklus, pompa dapat menyala jika indeks masih di bawah target 25%, meskipun sudah melewati ambang awal 15%.

Setelah pulsa terakhir, program tetap menunggu peresapan. Jika target belum tercapai, status menjadi `Cek air / sensor` dan pompa terkunci. Gangguan sensor saat memompa/menunggu peresapan juga mengunci pompa. Periksa air, selang, sensor, dan konfigurasi sebelum restart untuk membuka kunci. Tidak ada pengulangan otomatis dari kondisi terkunci.

Pembacaan sensor tidak valid, ADC tepat 0 atau 4095, atau data berumur 10 detik menghentikan/mencegah pompa. Gangguan BME280/BH1750 juga menghambat penyiraman karena validasi memakai seluruh sensor. Sensor tanah yang terputus tetapi menghasilkan nilai ADC masuk akal masih dapat lolos pemeriksaan ini; batas pulsa membatasi satu siklus, bukan mendeteksi seluruh kerusakan sensor.

Timer berjalan menggunakan `millis()` tanpa `delay()` penyiraman, sehingga animasi tetap berjalan. Timer dan kunci hanya tersimpan di RAM: restart menghapus jeda 24 jam serta riwayat pulsa, dan konfirmasi kering dimulai dari awal. Jangan gunakan restart berulang sebagai cara mengulang penyiraman.

## Integrasi MQTT cloud

ESP32 memakai MQTT 3.1.1 melalui TLS untuk publish monitoring dan subscribe perintah. Aplikasi dapat memakai broker cloud yang sama; backend aplikasi dapat ditambahkan untuk login pengguna dan menyimpan riwayat.

```text
Aplikasi/backend <-- MQTT --> Broker cloud <-- MQTT/TLS --> ESP32
```

Perintah diterima melalui subscription saat perangkat tersambung, tanpa polling HTTP. Kredensial broker belum diisi sehingga MQTT nonaktif pada konfigurasi awal. OLED, pembacaan sensor, dan penyiraman otomatis lokal tetap berjalan. Implementasi memakai [ESP-MQTT bawaan ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/protocols/mqtt.html), tanpa dependency library MQTT tambahan.

Isi konfigurasi di `src/main.cpp` setelah memilih MQTT cloud:

```cpp
const char *ssid = "NamaWiFi";
const char *password = "PasswordWiFi";
const char *MQTT_HOST = "hostname-broker-cloud-anda";
constexpr uint16_t MQTT_PORT = 8883;
const char *MQTT_USERNAME = "username-perangkat";
const char *MQTT_PASSWORD = "password-perangkat";
const char *MQTT_DEVICE_ID = "namika-01";
const char *MQTT_ROOT_CA = R"PEM(-----BEGIN CERTIFICATE-----
...isi sertifikat root CA yang memverifikasi broker...
-----END CERTIFICATE-----
)PEM";
```

`MQTT_HOST` hanya hostname, tanpa `mqtts://`, path, atau nomor port. Gunakan endpoint MQTT TCP/TLS broker, bukan alamat dashboard atau WebSocket. Firmware saat ini memakai autentikasi username/password dan verifikasi sertifikat server; broker yang mewajibkan sertifikat klien/mTLS membutuhkan penyesuaian. Root CA harus sesuai broker dan waktu NTP harus tersinkron sebelum klien dimulai. Verifikasi hostname tetap aktif. ID perangkat harus unik, 1–48 karakter, hanya huruf, angka, `-`, atau `_`; ID yang sama dipakai untuk client ID, JSON dan topik. Broker akan memutus salah satu koneksi jika dua klien memakai client ID yang sama. Aplikasi harus memakai client ID berbeda.

Topik bawaan:

| Topik | Arah dari ESP32 | QoS | Retain | Payload |
|---|---|---|---|---|
| `namika/namika-01/monitoring` | Publish | 1 | Ya | JSON sensor dan status pompa |
| `namika/namika-01/commands` | Subscribe | 1 | **Jangan retain saat publish** | JSON `water` atau `stop` |
| `namika/namika-01/ack` | Publish | 1 | Ya | ID dan hasil keputusan terakhir |
| `namika/namika-01/status` | Publish/Last Will | 1 | Ya | `online` atau `offline` |

Monitoring dipublish setiap 5 detik, setelah tersambung kembali, serta saat snapshot mendeteksi perubahan status penyiraman atau ID perintah. Sensor tetap dibaca setiap 5 detik, snapshot internal sekitar setiap 250 ms. Retain membantu aplikasi menerima nilai terakhir saat subscribe; backend harus memakai waktu penerimaan dan status koneksi untuk membedakan data lama. Last Will mengumumkan `offline` setelah broker mendeteksi koneksi terputus, bukan selalu langsung saat Wi-Fi mati. Keepalive 30 detik. MQTT reconnect otomatis, jeda awal 5 detik; jaringan/TLS dapat menambah waktu.

Atur ACL broker agar akun perangkat hanya subscribe ke `namika/<device-id>/commands` dan publish ke topik monitoring, ack, serta status miliknya. Akun aplikasi diberi izin publish commands dan subscribe tiga topik hasil sesuai perangkat pengguna. Jangan masukkan kredensial akun administrator broker ke aplikasi. Untuk aplikasi browser, layanan cloud perlu menyediakan MQTT over WebSocket Secure atau aplikasi mengakses backend; koneksi ESP32 sendiri tetap MQTT TCP/TLS.

Contoh publish perintah dari aplikasi dengan **QoS 1, retain false**:

```json
{"id": 1, "action": "water", "expires_at": 1800000060}
```

Gunakan `"action":"stop"` untuk menghentikan siklus. `expires_at` adalah Unix timestamp UTC dalam detik, harus di masa depan dan maksimal 120 detik dari waktu ESP32 saat diterima; aplikasi sebaiknya menghitung `waktu sekarang + 60`. Angka contoh hanya ilustrasi. Payload perintah maksimum 512 byte. JSON rusak/aksi tidak dikenal/retained delivery pada saat subscribe diabaikan tanpa menggerakkan pompa. Jangan gunakan retained commands: pesan lama dapat dikirim lagi ketika perangkat subscribe. Broker dapat meneruskan publish retained ke pelanggan yang sedang online dengan flag retain sudah dihapus; TTL dan ID tetap diperlukan.

`id` adalah bilangan bulat 1–4294967295 dan selalu meningkat per perangkat. Aplikasi/backend harus mempertahankan urutannya termasuk setelah ESP32 restart; bila ada beberapa pengguna, pembentukan ID perlu dikoordinasikan di backend. Saat retry, kirim ID dan isi perintah yang sama. Jangan langsung membuat ID baru hanya karena ACK belum terlihat. Langganan ACK dilakukan sebelum publish perintah dan hasil harus dicocokkan berdasarkan ID.

ID tertinggi dicatat ke NVS sebelum perintah masuk antrean pompa. ID yang sama/lebih kecil tidak mengeksekusi pompa lagi. Setelah reboot, hasil perintah lama bisa menjadi `duplicate_or_interrupted`: perintah mungkin sudah dijalankan, atau terlewat karena reset antara pencatatan dan pelaksanaan. Jangan menganggap hasil ini sebagai alasan mengulang siram dengan ID baru tanpa memeriksa kondisi perangkat. Gagal mengakses/menulis NVS menolak perintah (`storage_error`). Namespace NVS tetap `namika-cloud` seperti versi HTTPS sebelumnya; lanjutkan urutan ID saat migrasi.

Contoh ACK pada `namika/namika-01/ack`:

```json
{"id":1,"result":"accepted"}
```

`accepted` berarti kontrol lokal menerima permintaan, bukan bukti air mengalir atau siklus selesai. Hasil lain: `stopped`, `sensor_unavailable`, `soil_wet`, `busy`, `cooldown`, `locked`, `expired`, `invalid_expiry`, `clock_unavailable`, `duplicate_or_interrupted`, `storage_error`, atau `queue_full`. Nilai invalid yang tidak memiliki ID/aksi sah diabaikan dan dicetak ke Serial. QoS 1 dapat menghasilkan duplikat, sehingga aplikasi tetap perlu menangani ID. ACK dan monitoring keluar memakai outbox MQTT dalam RAM; bukan riwayat permanen. Broker hanya menyimpan satu ACK retained terakhir. Aplikasi/backend yang membutuhkan histori harus menyimpannya sendiri.

Perangkat memakai clean session, sehingga tidak meminta broker menyimpan antrean perintah selama offline. Aplikasi sebaiknya menonaktifkan tombol saat offline dan memberikan status menunggu sampai menerima ACK. Jika antrean lokal penuh, pesan dapat dibuang meskipun broker menerima PUBACK MQTT; PUBACK bukan bukti pompa menerima perintah. Batasi satu permintaan yang menunggu hasil dan retry dengan ID sama serta TTL yang masih berlaku. Perintah stop lewat internet tetap bergantung pada koneksi dan bukan tombol darurat.

Penyiraman manual membutuhkan kalibrasi dan sensor valid. Tanah pada/di atas target `wet`, siklus aktif, cooldown, serta kunci menghalangi penyiraman. `AUTO_WATERING_ENABLED = false` hanya mematikan pemicu otomatis; manual tetap dapat digunakan. Manual memakai satu pulsa `pulseMs`, menunggu `soakMs`, lalu masuk cooldown. Stop mematikan siklus aktif dan masuk cooldown, tetapi tidak membuka kunci. Timer lokal tetap menghentikan pulsa walaupun MQTT/Wi-Fi putus.

Contoh JSON monitoring:

```json
{
  "device_id": "namika-01",
  "uptime_ms": 310000,
  "sample_age_ms": 100,
  "temperature_c": 28.5,
  "humidity_pct": 55.0,
  "light_lux": 12000.0,
  "soil_pct": 12.0,
  "soil_raw": 2796,
  "sensors_valid": true,
  "soil_calibrated": true,
  "automatic_enabled": true,
  "samples": 60,
  "mood": 3,
  "pump_on": false,
  "watering_state": "monitoring",
  "pulses": 0,
  "last_command": {"id": 0, "result": "none"}
}
```

Nilai non-finite menjadi JSON `null`. `soil_pct` adalah indeks relatif kalibrasi; perhatikan `soil_calibrated` dan `sensors_valid`. Mood dinilai penuh setelah 60 sampel valid. `last_command` adalah keputusan loop pompa pada sesi saat ini; penolakan jaringan seperti expired atau duplikat dikirim melalui ACK. `watering_state` dapat berupa `monitoring`, `watering`, `soaking`, `cooldown`, atau `locked`.

Jaringan berjalan pada task terpisah dan tidak menulis GPIO pompa. Antrean masuk menampung empat pesan; hasil keputusan diproses satu per satu. Outbox dibatasi melalui pemeriksaan ukuran sebelum enqueue agar koneksi lambat tidak menumpuk monitoring tanpa batas. Tidak ada backlog histori sensor saat offline. Timer pompa tetap timer software di loop Arduino dan perlu diuji pada perangkat fisik bersama seluruh sensor.

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
- `WiFi`, `Wire`, `Preferences`, ESP-MQTT, cJSON, dan FreeRTOS berasal dari framework Arduino ESP32.

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

Kelembapan udara tinggi dan cahaya rendah saja tidak membuat mood gabungan menjadi level 1. Saat tidur, cahaya diabaikan dalam penilaian mood gabungan. Tanah kering tidak dianggap sebagai mood buruk; kontrol pompa memakai aturan penyiraman terpisah di atas.

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
├── src/main.cpp                  Sensor, pompa, Wi-Fi, waktu, dan OLED
├── src/MqttMonitoring.cpp        MQTT TLS, subscription perintah, dan ACK
├── include/MqttMonitoring.h      Antrean perintah dan snapshot antar-task
├── include/MqttPacket.h          Perakitan payload MQTT terfragmentasi
├── include/CloudCommand.h        Validasi ID, aksi, dan kedaluwarsa perintah
├── include/WateringController.h  Timer dan batas penyiraman otomatis
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

Pengujian kontrol pompa:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/host/test_watering.cpp -o /tmp/namika-test-watering
/tmp/namika-test-watering
```

Pengujian kontrak perintah cloud:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/host/test_cloud.cpp -o /tmp/namika-test-cloud
/tmp/namika-test-cloud
```

Pengujian payload MQTT:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/host/test_mqtt_packet.cpp -o /tmp/namika-test-mqtt
/tmp/namika-test-mqtt
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
