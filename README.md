# ESP-TERMINAL-CT100

Modułowy fundament projektu terminala CT-100 dla WT32-ETH01.

## Co już jest
- Ethernet na WT32-ETH01
- OTA po sieci
- Web panel konfiguracyjny z Basic Auth
- zapis konfiguracji w NVS/Preferences
- RFID 125 kHz po UART (typ EM-18 / RDM6300 / podobne ASCII)
- wyświetlacz graficzny 128x64 na ST7920 przez U8g2
- klawiatura membranowa 4x4
- logi lokalne + podgląd w web panelu
- TCP client jako działający fundament
- tryby TCP `host` i `server` jako przygotowane opcje konfiguracyjne pod dalszy etap

## Ważne
To jest solidny szkielet rozwojowy, ale nie wszystkie elementy są jeszcze skończone produkcyjnie.
Najbardziej kompletne są: konfiguracja, web panel, logowanie, RFID/UART i LCD.
Tryby TCP `host/server` są obecnie placeholderem pod kolejny etap z raw `WiFiServer`.

## Domyślne logowanie WWW
- login: `admin`
- hasło: `admin`

## Domyślne hasło OTA
- `admin123`

## Gdzie zmieniać
Większość ustawień zmienisz z panelu WWW po starcie urządzenia.

## Pinout startowy
Zobacz `include/Pins.h`.

## Uwaga sprzętowa
WT32-ETH01 ma ograniczoną liczbę wygodnych GPIO. Finalny pinout może wymagać korekty po spięciu wszystkich modułów naraz.
