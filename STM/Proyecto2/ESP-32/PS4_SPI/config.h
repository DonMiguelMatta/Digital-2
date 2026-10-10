#pragma once
#include <stdint.h>
// Cambiar a 2 para el segundo ESP32.
#define PLAYER_ID 1
// Primera prueba: false y conectar un solo ESP32 a la vez.
// Luego copiar la MAC DEL CONTROL que imprime el monitor y activar true.
#define FILTER_CONTROLLER_MAC true
static const uint8_t CONTROLLER_MAC[6] = {0xA4, 0x15, 0x66, 0x77, 0x77, 0x51};
// Activar SOLO para reparar un emparejamiento; volver a false luego.
#define ERASE_PAIRING_KEYS false
#define PIN_SCK 18
#define PIN_MISO 19
#define PIN_MOSI 23
#define PIN_CS 27
#define PIN_READY 26
#define STICK_DEADZONE 160
#define BT_REPORT_TIMEOUT_MS 500u
// true: mostrar estado solo cuando cambian botones o conexion.
#define SERIAL_DIAGNOSTICS true
// false: prueba Bluetooth solamente; true: firmware final Bluetooth + SPI.
#define ENABLE_SPI true
