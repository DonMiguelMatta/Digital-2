# Musica DAC para Celeste

## Proyecto completo para descargar

Descargar [Lab07_CELESTE_TEST_completo.zip](Lab07_CELESTE_TEST_completo.zip).
Incluye el proyecto completo importable en STM32CubeIDE y el firmware ELF
compilado para la Nucleo-F446RE dedicada a audio.


Descomprimir `Lab07_CELESTE_TEST_completo.zip`. El ZIP incluye fuentes, drivers
HAL/CMSIS, startup, linker, .ioc, metadatos de CubeIDE y firmware compilado.

1. En STM32CubeIDE: File > Import > General > Existing Projects into Workspace.
2. Seleccionar la carpeta descomprimida que contiene Lab07_CELESTE_TEST.
3. Marcar el proyecto Lab07_CELESTE_TEST y finalizar la importacion.
4. Compilar con Project > Build Project.
5. Conectar la Nucleo-F446RE por el USB del ST-LINK y usar Run o Debug para
   programarla. Si se usa Debug, pulsar Resume para que empiece el programa.

Alternativa sin compilar: el archivo Firmware/Lab07_CELESTE_TEST.elf ya esta
compilado para STM32F446RE. Se puede programar con STM32CubeProgrammer por
ST-LINK usando ese ELF. No es firmware para la Nucleo que ejecuta el juego:
se carga en la Nucleo dedicada a audio.

El arranque queda en silencio. USART2 recibe ASCII `1` para main_theme en loop,
`2` para main_menu en loop y `0` para detener, a 115200, 8N1. TX de la Nucleo
del juego a PA3 de la Nucleo de audio y GND comun. Salida DAC en PA4.
No se requieren las herramientas de Python usadas para preparar esta entrega.
Verificacion de compilacion y enlace completa; prueba fisica pendiente.


Archivos de referencia para integrar el reproductor al proyecto del juego. Los .c corresponden a Core/Src y los .h a Core/Inc. No reemplazar el main.c del juego completo: trasladar las secciones USER CODE indicadas abajo.
Placa STM32 Nucleo-F446RE, HAL. No se ha probado fisicamente esta version.

## Comandos de la otra Nucleo

USART2, 115200 baudios, 8 bits, sin paridad, 1 bit de parada.
Enviar caracteres ASCII, no numeros binarios:

| Byte ASCII | Accion |
| --- | --- |
| `1` (0x31) | main_theme en loop |
| `2` (0x32) | main_menu en loop |
| `0` (0x30) | Detener y dejar DAC en punto medio |

Se inicia en silencio. Ambas canciones repiten automaticamente hasta recibir
otro comando. Repetir el comando de la cancion activa no la reinicia.
Cambiar de cancion comienza desde su primera nota. Despues de detener,
seleccionar una cancion tambien la comienza desde el principio.
CR/LF y otros bytes se ignoran. Si llegan varios comandos antes de atenderlos,
se conserva el ultimo comando valido: son ordenes de estado, no una cola de eventos.

Conectar TX de la Nucleo maestra a PA3 (USART2_RX) de la Nucleo de audio y GND
comun. Ambas UART deben tener la misma configuracion. No hay respuesta UART
ni confirmacion implementada. Evitar enviar simultaneamente desde el puerto
virtual del ST-LINK y la otra Nucleo hacia esta misma entrada RX.
Salida de audio: PA4 (DAC_OUT1), con la etapa de audio que ya funcionaba.

## Canciones

- `Core/Inc/main_theme.h`: original de 502 entradas, 104767 ms.
- `Core/Inc/main_menu.h`: Track1 del RTTTL adjunto, 534 entradas, 215639 ms.

La conversion redondea frecuencias a Hz enteros y duraciones a ms enteros.
Silencios RTTTL `p` son frecuencia cero. El reproductor usa solo Track1,
una frecuencia por vez. No mezcla las cuatro pistas.

## Cambios por seccion USER CODE

En `Core/Src/main.c`:

1. `Includes`: reemplazar pwm_theme por main_menu; conservar math.h.
2. `PV`: estado de cancion, nota, tiempo de inicio y comando UART pendiente.
3. `PFP`: prototipos del reproductor no bloqueante.
4. `0`: generar seno y funciones Music_Start, Music_Stop, Music_Update,
   Music_Command. TIM6/DMA generan el audio; HAL_GetTick decide cuando avanzar.
5. `2`: generar tabla, iniciar en silencio, desactivar TIM1 y PA11 de PWM,
   activar interrupcion USART2 y recibir un byte con HAL_UART_Receive_IT.
6. `WHILE`: llamar Music_Command y Music_Update en cada vuelta.
7. `4`: callbacks UART de recepcion y recuperacion de errores.

En `Core/Src/stm32f4xx_it.c`:

1. `EV`: declarar extern huart2.
2. `1`: USART2_IRQHandler llama HAL_UART_IRQHandler.

Las regiones generadas fuera de USER CODE se conservaron exactamente.
TIM1 sigue presente en el codigo generado, pero se desinicializa y PA11 se
libera. Para eliminar tambien su configuracion generada, desactivar TIM1_CH4
desde CubeMX y regenerar; no se borro manualmente codigo generado.
El antiguo pwm_theme.h se reemplaza por main_menu.h.

## Integrar al juego del companero

Los archivos sueltos sirven como referencia de integracion. El ZIP contiene
el proyecto completo para la Nucleo de audio. Si se integra en otro proyecto,
trasladar las secciones anteriores y ambos encabezados sin sustituir el main.c del juego.
Conservar DAC canal 1 con trigger TIM6 TRGO, TIM6 con TRGO UPDATE y DMA1
Stream5/canal7 circular con alineacion word y su IRQ HAL. Tabla de 128 muestras.
El reloj TIM6 se calcula desde APB1 y el prescaler configurado.

Llamar Music_Command y Music_Update con frecuencia, sin HAL_Delay largos ni
esperas de pantalla, botones o comunicaciones que retengan el bucle. La nota
se prolonga si el juego tarda demasiado en volver a Music_Update.
No llamar el reproductor desde interrupciones: el callback solo recoge comandos.

Si el juego ya tiene callbacks UART, integrar estas ramas USART2 dentro de
ellos; no definir dos veces las mismas funciones HAL. Si CubeMX genera el
USART2_IRQHandler despues, conservar un solo handler. TIM6, DAC1, DMA1 Stream5
y PA4 deben reservarse para audio; revisar conflictos con otros perifericos.

## Verificacion

Compilacion completa y enlace con arm-none-eabi-gcc de STM32CubeIDE 2.2.0,
STM32F446xx/HAL, sin errores ni advertencias. Verificado: 29440 bytes de text, 12 de data y 2436 de bss.
Prueba fisica pendiente: enviar 2, luego 1 durante una nota, luego 0; verificar
silencio, reinicio despues de detener, repeticion de ambas canciones y que
repetir 1/2 no reinicie la cancion activa.

