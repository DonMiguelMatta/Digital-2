#include "pantalla_parqueo.h"
#include "parqueo.h"
#include "ili9341.h"
#include <stdio.h>

#define FONDO 0x1082U
#define BLANCO 0xFFFFU
#define VERDE 0x0488U
#define ROJO 0xB8C3U
#define GRIS 0xA534U

static uint8_t ultimo_estado[PARQUEO_CANTIDAD];
static uint8_t ultimos_disponibles;

static void DibujarEspacio(uint8_t i)
{
  const unsigned int x = (i % 2U) ? 166U : 12U;
  const unsigned int y = (i / 2U) ? 124U : 48U;
  const uint8_t ocupado = parqueo_ocupado[i];
  const unsigned int color = ocupado ? ROJO : VERDE;
  char nombre[16];

  FillRect(x, y, 142, 66, color);
  (void)snprintf(nombre, sizeof(nombre), "ESPACIO %u", (unsigned int)i + 1U);
  LCD_Print(nombre, x + 10, y + 8, 1, BLANCO, color);
  LCD_Print(ocupado ? "OCUPADO" : "LIBRE", x + 10, y + 32, 2, BLANCO, color);
  ultimo_estado[i] = ocupado;
}

static void DibujarContador(void)
{
  char texto[24];
  FillRect(12, 203, 296, 21, FONDO);
  (void)snprintf(texto, sizeof(texto), "LIBRES: %u / %u", (unsigned int)espacios_disponibles,
                 (unsigned int)PARQUEO_CANTIDAD);
  LCD_Print(texto, 16, 204, 2, BLANCO, FONDO);
  ultimos_disponibles = espacios_disponibles;
}

void PantallaParqueo_Init(void)
{
  /* La pantalla almacena sus pixeles: no se usa SD ni un framebuffer en RAM. */
  LCD_Init();
  LCD_Clear(FONDO);
  LCD_Print("PARQUEO - GRUPO 1", 24, 12, 2, BLANCO, FONDO);
  LCD_Print("BOTON PRESIONADO = OCUPADO", 12, 228, 1, GRIS, FONDO);
  /* Fuente de 12 px de alto: el pie debe quedar dentro de las 240 filas. */
  for (uint8_t i = 0U; i < PARQUEO_CANTIDAD; ++i)
  {
    DibujarEspacio(i);
  }
  DibujarContador();
}

void PantallaParqueo_Update(void)
{
  /* Redibujar solo una tarjeta por vuelta para atender los botones entre cambios. */
  for (uint8_t i = 0U; i < PARQUEO_CANTIDAD; ++i)
  {
    if (ultimo_estado[i] != parqueo_ocupado[i])
    {
      DibujarEspacio(i);
      break;
    }
  }
  if (ultimos_disponibles != espacios_disponibles)
  {
    DibujarContador();
  }
}
