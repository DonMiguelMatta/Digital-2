#include "pantalla_parqueo.h"
#include "parqueo.h"
#include "parqueo_i2c.h"
#include "ili9341.h"
#include "carro_textura.h"
#include <stdio.h>

#define FONDO 0x1082U
#define BLANCO 0xFFFFU
#define VERDE 0x0488U
#define ROJO 0xB8C3U
#define GRIS 0x630CU
#define SIN_DATOS 2U

#define ESPACIOS_PANTALLA 8U
#define COLUMNAS 4U
#define MARGEN_X 12U
#define FILA_SUPERIOR_Y 32U
#define PASO_X 76U
#define PASO_Y 98U
#define ANCHO_ESPACIO 68U
#define ALTO_ESPACIO 81U

static uint8_t ultimo_estado[ESPACIOS_PANTALLA];
static uint8_t ultimos_disponibles;

static uint8_t EstadoEspacio(uint8_t i, uint8_t remota)
{
  if (i >= PARQUEO_CANTIDAD) return parqueo_ocupado[i - PARQUEO_CANTIDAD];
  if (remota == PARQUEO_GRUPO_SIN_DATOS) return SIN_DATOS;
  return (remota >> i) & 1U;
}

static uint8_t ContarDisponibles(uint8_t remota)
{
  if (remota == PARQUEO_GRUPO_SIN_DATOS) return PARQUEO_GRUPO_SIN_DATOS;
  uint8_t disponibles = 0U;
  for (uint8_t i = 0U; i < ESPACIOS_PANTALLA; ++i)
  {
    disponibles += (EstadoEspacio(i, remota) == 0U) ? 1U : 0U;
  }
  return disponibles;
}

static void DibujarEspacio(uint8_t i, uint8_t estado)
{
  const unsigned int x = MARGEN_X + (i % COLUMNAS) * PASO_X;
  const unsigned int y = FILA_SUPERIOR_Y + (i / COLUMNAS) * PASO_Y;
  const unsigned int color = estado == SIN_DATOS ? GRIS : (estado ? ROJO : VERDE);
  char numero[4];

  FillRect(x, y, ANCHO_ESPACIO, ALTO_ESPACIO, color);
  (void)snprintf(numero, sizeof(numero), "%u", (unsigned int)i + 1U);
  /* El numero queda arriba para no superponerse con el carro centrado. */
  LCD_Print(numero, x + (ANCHO_ESPACIO - fontXSizeSmal) / 2U,
            y + 2U, 1, BLANCO, color);
  if (estado == 1U)
  {
    LCD_BitmapTransparent(x + (ANCHO_ESPACIO - CARRO_ANCHO) / 2U,
                          y + (ALTO_ESPACIO - CARRO_ALTO) / 2U,
                          CARRO_ANCHO, CARRO_ALTO, carro_textura,
                          CARRO_COLOR_TRANSPARENTE);
  }
  ultimo_estado[i] = estado;
}

static void DibujarContador(uint8_t disponibles)
{
  char texto[24];
  FillRect(12, 212, 296, 20, FONDO);
  if (disponibles == PARQUEO_GRUPO_SIN_DATOS)
    (void)snprintf(texto, sizeof(texto), "LIBRES: - / 8");
  else
    (void)snprintf(texto, sizeof(texto), "LIBRES: %u / %u", (unsigned int)disponibles,
                   (unsigned int)ESPACIOS_PANTALLA);
  LCD_Print(texto, 56, 214, 2, BLANCO, FONDO);
  ultimos_disponibles = disponibles;
}

void PantallaParqueo_Init(void)
{
  /* La pantalla almacena sus pixeles: no se usa SD ni un framebuffer en RAM. */
  LCD_Init();
  LCD_Clear(FONDO);
  /* Disposicion horizontal de 320 x 240: cuatro espacios por fila. */
  LCD_Print("PARQUEO", 104, 10, 2, BLANCO, FONDO);
  const uint8_t remota = ParqueoI2C_MascaraRemota();
  for (uint8_t i = 0U; i < ESPACIOS_PANTALLA; ++i)
  {
    DibujarEspacio(i, EstadoEspacio(i, remota));
  }
  DibujarContador(ContarDisponibles(remota));
}

void PantallaParqueo_Update(void)
{
  const uint8_t remota = ParqueoI2C_MascaraRemota();
  /* Redibujar solo una tarjeta por vuelta para atender los botones entre cambios. */
  for (uint8_t i = 0U; i < ESPACIOS_PANTALLA; ++i)
  {
    const uint8_t estado = EstadoEspacio(i, remota);
    if (ultimo_estado[i] != estado)
    {
      DibujarEspacio(i, estado);
      break;
    }
  }
  const uint8_t disponibles = ContarDisponibles(remota);
  if (ultimos_disponibles != disponibles)
  {
    DibujarContador(disponibles);
  }
}
