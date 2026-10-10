# ESP32: controles PS4 para Proyecto 2

Firmware para dos ESP32-WROOM-32, un DualShock 4 por placa. Bluetooth con Bluepad32 y salida SPI hacia la STM32 maestra.

Abrir PS4_SPI.ino en Arduino IDE usando el paquete ESP32 + Bluepad32 4.1.0. Mantener config.h y gamepad_protocol.h junto al sketch. Cada placa requiere su PLAYER_ID (1 o 2) y la MAC de su propio control.

SPI usa modo 0, MSB primero y un paquete fijo de 4 bytes: jugador, mascara de botones simultaneos, estado A0/A1 y CRC-8. La STM32 debe esperar READY, bajar el CS correspondiente, transferir cuatro bytes enviando ceros por MOSI y subir CS.

GPIO ESP32: SCK=18, MOSI=23, MISO=19, CS=27, READY=26. Las señales son de 3.3V y requieren GND comun.

La cruceta controla direcciones; Cruz es salto, Circulo es secundario y Options es inicio. Los estados se repiten mientras hay botones presionados.
