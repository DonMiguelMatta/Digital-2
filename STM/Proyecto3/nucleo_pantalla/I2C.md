# I2C: STM32 con pantalla, esclavo 2

Esta etapa implementa la STM32F446RE como **esclavo 2**, con el ESP32 como unico
maestro. La otra STM debe usar una direccion distinta. Este programa no inicia
transferencias como maestro y tiene deshabilitada la llamada general (general call).

## Conexion

| Bus compartido | STM32 con pantalla | ESP32 maestro |
| --- | --- | --- |
| SCL | PB8, I2C1_SCL | Pin configurado como SCL en `Wire.begin(SDA, SCL)` |
| SDA | PB9, I2C1_SDA | Pin configurado como SDA en `Wire.begin(SDA, SCL)` |
| Tierra | GND | GND |

PB8 y PB9 no sustituyen ningun pin actual de la TFT, los botones o los LEDs.
En la Nucleo tambien se encuentran en los conectores Morpho, accesibles aunque
la pantalla ocupe los conectores Arduino. Usar las etiquetas PB8/PB9 de la placa.

SDA y SCL requieren pull-ups externos a **3.3 V**; para la prueba a 100 kHz con
cables cortos puede partirse de **4.7 kohm por linea** y verificar la senal.
Usar un solo par para el bus completo, considerando las resistencias ya presentes
en otros modulos. La otra STM se conectara a las mismas lineas y GND; debe usar
una direccion de 7 bits distinta de `0x20`.

## Configuracion

- Periferico: I2C1, direccion de 7 bits `0x20` (decimal 32), esclavo 2.
- `hi2c1.Init.OwnAddress1 = 0x40`: HAL espera la direccion desplazada un bit.
- En CubeMX, `Own Address 1` se guarda como 64; no poner 32 en ese campo.
- El maestro debe generar SCL a **100000 Hz**; el esclavo no genera el reloj.
- Stretch de reloj habilitado; direccion dual y general call deshabilitados.
- Interrupciones I2C1 de eventos y errores habilitadas, prioridad 5.
- HAL I2C agregado desde el paquete local STM32Cube FW_F4 V1.28.3.

`ParqueoI2C_Init()` se llama despues de `Parqueo_Init()` y antes de iniciar la
pantalla. `ParqueoI2C_Update()` publica los estados despues de leer los botones y
antes de dibujar. Las transferencias utilizan interrupciones; no dibujan en ellas.

## Protocolo de esta etapa

El ESP32 solicita **exactamente un byte**, sin enviar un registro previamente:

| Bit | Significado |
| --- | --- |
| 0 | Primer parqueo local: 1 ocupado, 0 libre |
| 1 | Segundo parqueo local |
| 2 | Tercer parqueo local |
| 3 | Cuarto parqueo local |
| 4-7 | Siempre cero |

Se conserva la asignacion visual existente: los cuatro botones de esta STM
controlan los espacios **1-4**. El numero de esclavo identifica la placa en el bus;
no cambia automaticamente la numeracion de los parqueos. Los espacios **5-8**
siguen temporalmente verdes y se cuentan como libres, segun lo acordado.

Ejemplos: `0x00` todos locales libres, `0x01` primero ocupado, `0x05` primero y
tercero ocupados, `0x0F` los cuatro ocupados. No se transmite el conteo de la TFT;
el maestro calcula sus totales a partir de las mascaras de cada placa.

Las escrituras de un byte se descartan; aun no se implementan comandos ni
recepcion del estado de la otra STM. Las sondas de direccion sin datos permiten
usar un escaner I2C. No solicitar varios bytes ni enviar tramas de varios bytes.

## Prueba con el ESP32

En el futuro programa del maestro, despues de inicializar `Wire` con los pines
fisicamente conectados:

```cpp
Wire.setClock(100000);
if (Wire.requestFrom((uint8_t)0x20, (uint8_t)1) == 1) {
  uint8_t ocupados = Wire.read();
  // bitRead(ocupados, 0..3): estados de los cuatro parqueos locales.
}
```

1. Escanear el bus: esta placa debe aparecer en `0x20`.
2. Sin botones presionados, comprobar `0x00` y `LIBRES: 8 / 8` en TFT.
3. Presionar cada boton mas de 30 ms: comprobar `0x01`, `0x02`, `0x04`, `0x08`.
4. Presionar primero y tercero: comprobar `0x05`, dos carros y `LIBRES: 6 / 8`.
5. Presionar los cuatro: comprobar `0x0F` y `LIBRES: 4 / 8`.
6. Repetir lecturas y el escaneo: la placa debe seguir respondiendo sin reinicio.
7. Una consulta a la direccion de la otra STM no debe activar esta placa.

En Live Expressions: `i2c_mascara_ocupados`, `i2c_lecturas`, `i2c_errores`,
`i2c_ultimo_error`. El NACK que termina una lectura no incrementa los errores.
`i2c_lecturas` cuenta buffers enviados por HAL, no es una confirmacion de que la
aplicacion del ESP32 haya procesado el dato. Los errores reales solicitan
reinicializar solo I2C1 desde el ciclo principal, sin detener la GUI ni generar
pulsos de recuperacion que afecten al resto del bus.

La compilacion y las comprobaciones de software no sustituyen esta prueba fisica.
Al agregar estos archivos a una sesion ya abierta de CubeIDE, refrescar el proyecto
y recompilar para regenerar los makefiles e incluir el modulo y los drivers I2C.
