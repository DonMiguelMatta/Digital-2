# ESP32: controles PS4 por UART

Firmware para dos ESP32-WROOM-32, un DualShock 4 por ESP32. Bluepad32 recibe
el control por Bluetooth y cada ESP32 envia su estado a un UART independiente
de la STM32.

## Cargar en Arduino IDE

Instalar **ESP32 + Bluepad32 4.1.0** y seleccionar `ESP32 Dev Module` dentro de
ese paquete. Abrir `PS4_SPI.ino` conservando `config.h` y
`gamepad_protocol.h` en la misma carpeta.

Antes de cargar cada placa, editar solo `PLAYER_ID` en `config.h`:

```cpp
// ESP32 del jugador 1
#define PLAYER_ID 1

// ESP32 del jugador 2
#define PLAYER_ID 2
```

Las MAC de ambos controles ya estan guardadas. Emparejar con SHARE + PS. El
monitor USB permanece disponible a 115200 baudios para diagnostico.

## Conexion

Cada ESP32 transmite por `GPIO17` (`TX2`) a 115200 baudios, 8 bits, sin
paridad y un bit de parada. No se conecta un RX al ESP32. No usar `TX0` o
`GPIO1`: ese pin contiene los mensajes de arranque y diagnostico de Bluepad32.

| Jugador | ESP32 | STM32 |
|---|---|---|
| J1 | GPIO17 / TX2 | PD2 / UART5_RX |
| J2 | GPIO17 / TX2 | PC11 / USART3_RX |
| Ambos | GND | GND |

No conectar los dos TX entre si. Cada uno llega a un RX diferente de la STM32.
Todas las senales son de 3.3 V.

## Trama UART

Cada ESP32 envia una trama cada 20 ms desde una tarea periodica independiente
del procesamiento Bluetooth:

| Byte | Dato |
|---|---|
| 0 | Sincronizacion `0xA5` |
| 1 | ID del jugador: 1 o 2 |
| 2 | Mascara de botones simultaneos |
| 3 | `0xA1` conectado o `0xA0` desconectado |
| 4 | CRC-8/SMBUS de bytes 0 a 3 |

CRC: polinomio `0x07`, inicial `0x00`, sin reflexion y sin XOR final.

| Bit | Accion |
|---|---|
| 0 / `0x01` | Izquierda |
| 1 / `0x02` | Derecha |
| 2 / `0x04` | Arriba |
| 3 / `0x08` | Abajo |
| 4 / `0x10` | Cruz / salto |
| 5 / `0x20` | Circulo / segundo salto |
| 6 / `0x40` | Options / inicio |
| 7 | Reservado, siempre cero |

La STM32 reconstruye la trama a partir de `0xA5`, valida ID, formato y CRC, y
genera eventos solo en el flanco de pulsacion. Si pasan 300 ms sin una trama
valida, libera cualquier accion mantenida.
