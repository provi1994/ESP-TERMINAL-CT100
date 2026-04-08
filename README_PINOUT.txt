CT-100 / WT32-ETH01 - finalny pinout

1) Ethernet WT32-ETH01 (nie używać do niczego innego)
- GPIO16 -> ETH_POWER / PHY enable
- GPIO18 -> ETH_MDIO
- GPIO23 -> ETH_MDC
- GPIO0  -> RMII clock (wewnętrznie dla Ethernet)

2) RFID 125 kHz po UART2
- GPIO5  -> ESP RX2  <- TX czytnika RFID
- GPIO17 -> ESP TX2  -> RX czytnika RFID (jeżeli czytnik używa tylko TX, ten pin może zostać niepodłączony)

3) Klawiatura 4x4 przez PCF8574 po I2C
- GPIO33 -> SDA
- GPIO32 -> SCL
- PCF8574 adres domyślny w projekcie: 0x20
- mapowanie linii matrycy:
  P0 -> ROW1
  P1 -> ROW2
  P2 -> ROW3
  P3 -> ROW4
  P4 -> COL1
  P5 -> COL2
  P6 -> COL3
  P7 -> COL4

4) LCD 12864 ST7920 w trybie szeregowym 3-wire
- GPIO14 -> LCD E / CLK
- GPIO4  -> LCD RW / DATA
- GPIO15 -> LCD RS / CS
- LCD PSB -> GND (obowiązkowo, wymusza tryb szeregowy)
- LCD RST -> zostawione bez sterowania z ESP; zrób pull-up / typowe podłączenie modułu
- LCD VDD -> 5V
- LCD VSS -> GND
- LCD A/K -> podświetlenie zgodnie z modułem

Uwaga ważna:
- moduł LCD jest 5V. Sygnały z ESP32 są 3.3V.
- produkcyjnie zalecam konwerter poziomów logicznych 3.3V -> 5V dla 3 linii LCD.
- bez konwertera może działać, ale tego nie traktuj jako gwarancji docelowej.

Sprawdzone założenia projektowe:
- klawiatura NIE siedzi już na pinach Ethernetu
- RFID siedzi na UART2
- LCD używa tylko 3 GPIO
- projekt nie wymaga zewnętrznej biblioteki keypad ani biblioteki PCF8574
