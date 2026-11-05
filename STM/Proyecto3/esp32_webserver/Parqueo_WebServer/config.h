#ifndef PARQUEO_WEB_CONFIG_H
#define PARQUEO_WEB_CONFIG_H

// Red creada por el propio ESP32; no se necesita router ni internet.
constexpr char WIFI_NOMBRE[] = "Parqueo_Grupo1";
constexpr char WIFI_CLAVE[] = "Parqueo1234";
constexpr uint8_t NUM_GRUPOS = 2;
constexpr uint8_t ESPACIOS_POR_GRUPO = 4;
constexpr uint8_t NUM_ESPACIOS = NUM_GRUPOS * ESPACIOS_POR_GRUPO;
constexpr uint8_t DIRECCIONES_NUCLEO[NUM_GRUPOS] = {0x20, 0x21};
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr uint32_t PERIODO_I2C_MS = 100;
constexpr uint32_t DATOS_VENCIDOS_MS = 1500;

#endif
