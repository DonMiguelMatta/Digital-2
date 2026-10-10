# ESP32: controles PS4 para Proyecto 2

Firmware para dos ESP32-WROOM-32, un DualShock 4 por ESP32. Bluetooth con Bluepad32 y comunicacion SPI hacia la STM32 maestra.

## Cargar en Arduino IDE

Instalar el paquete **ESP32 + Bluepad32 4.1.0** siguiendo https://bluepad32.readthedocs.io/en/latest/plat_arduino/ y seleccionar ESP32 Dev Module de ese paquete. Abrir `PS4_SPI.ino` conservando `config.h` y `gamepad_protocol.h` en esta carpeta. El codigo se verifico por compilacion con PLAYER_ID 1 y 2; la compilacion no reemplaza una prueba fisica.

Editar `config.h` para cada placa, manteniendo FILTER_CONTROLLER_MAC true, ERASE_PAIRING_KEYS false y ENABLE_SPI true:

```cpp
// ESP32 jugador 1
#define PLAYER_ID 1
static const uint8_t CONTROLLER_MAC[6] = {0xA4,0x15,0x66,0x77,0x77,0x51};

// ESP32 jugador 2: reemplazar las dos lineas anteriores por estas
#define PLAYER_ID 2
static const uint8_t CONTROLLER_MAC[6] = {0x40,0x1B,0x5F,0x88,0xA1,0x4C};
```

No declarar ambas configuraciones simultaneamente. Emparejar con SHARE + PS. Monitor Serial a 115200. Direcciones solo de la cruceta; Cruz=salto, Circulo=secundario y Options=inicio.

## Pines de cada ESP32

| Senal | GPIO |
|---|---|
| SCK | 18 |
| MOSI | 23 |
| MISO | 19 |
| CS activo bajo | 27 |
| READY | 26 |

Las dos placas usan estos mismos GPIO, pero cada enlace debe tener seleccion CS y READY propios. Si se comparte el reloj, no unir salidas MISO sin verificar su aislamiento; la prueba Arduino usaba entradas MISO separadas. Nucleo y ESP32 usan señales de 3.3V y GND comun; no requieren convertidor a 5V.

## Recibir en STM32: paquete SPI de 4 bytes

STM32 maestra, ESP32 esclavo. Modo 0, MSB primero, datos de 8 bits. Consultar READY; al estar alto, bajar solo el CS de ese ESP, transferir **4 bytes enviando ceros en MOSI**, y subir CS. Son 32 pulsos de reloj con CS bajo durante toda la trama.

| Byte | Dato |
|---|---|
| 0 | ID del jugador: 1 o 2 |
| 1 | Mascara de botones simultaneos |
| 2 | 0xA1 conectado / 0xA0 desconectado |
| 3 | CRC-8/SMBUS de bytes 0..2 |

CRC: polinomio 0x07, inicial 0, sin reflexion y sin XOR final. Vector ASCII `123456789` produce 0xF4. Usar `gp_valid()` del header para verificar cada trama.

| Bit de la mascara | Accion |
|---|---|
| 0 / 0x01 | Izquierda |
| 1 / 0x02 | Derecha |
| 2 / 0x04 | Arriba |
| 3 / 0x08 | Abajo |
| 4 / 0x10 | Cruz / salto |
| 5 / 0x20 | Circulo / secundario |
| 6 / 0x40 | Options / inicio |
| 7 | Reservado, cero |

Evaluar cada bit independientemente, sin else-if entre acciones. Por ejemplo, derecha + salto = 0x12. Mantener estados independientes por jugador.

El ESP32 envia estado inicial, cambios, liberaciones y desconexiones. Mientras hay botones presionados repite el estado; en reposo no necesita repetir. READY indica una trama preparada, no necesariamente un control conectado. Leer cada trama con gp_valid antes de aplicarla. Detener una accion mantenida si no llega otra trama valida por 300ms; neutralizar al recibir desconexion. La cola almacena hasta 16 estados y prioriza los recientes si se llena.

Este formato de 4 bytes sustituye al antiguo de 24. No hay secuencia, ejes, token ni contadores en el paquete; los contadores del monitor local son diagnostico. SPI envia binario, no los comandos ASCII j1i/j2i/x/o/a que mostraba el Arduino de prueba.
