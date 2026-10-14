#pragma once
#include <stdint.h>

// Identifica qué control y qué canal lógico representa este ESP32.
// Cambiar a 2 para el segundo ESP32.
#define PLAYER_ID 2

// Primera prueba: false y conectar un solo ESP32 a la vez.
// Luego copiar la MAC DEL CONTROL que imprime el monitor y activar true.
#define FILTER_CONTROLLER_MAC true
#if PLAYER_ID == 1
static const uint8_t CONTROLLER_MAC[6] = {0xA4, 0x15, 0x66, 0x77, 0x77, 0x51};
#elif PLAYER_ID == 2
static const uint8_t CONTROLLER_MAC[6] = {0x40, 0x1B, 0x5F, 0x88, 0xA1, 0x4C};
#else
#error "PLAYER_ID debe ser 1 o 2"
#endif

// Activar SOLO para reparar un emparejamiento; volver a false luego.
#define ERASE_PAIRING_KEYS false

// Ajustes del enlace UART que lleva los controles a la STM32.
#define CONTROL_UART_TX_PIN 17
#define CONTROL_UART_BAUD_RATE 115200UL
#define CONTROL_FRAME_PERIOD_MS 20U

// Reserva para lectura analógica y seguridad ante pérdida de Bluetooth.
#define STICK_DEADZONE 160
#define BT_REPORT_TIMEOUT_MS 500u

// false: prueba Bluetooth solamente; true: firmware final Bluetooth + UART.
#define ENABLE_CONTROL_UART true
