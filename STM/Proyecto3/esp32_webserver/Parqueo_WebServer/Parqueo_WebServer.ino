/* Basado en Contenido/webserverAP-1.zip y la prueba I2C validada en el laboratorio.
 * ESP32 maestro I2C y servidor HTTP; Nucleo 1 en 0x20 y Nucleo 2 en 0x21.
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "pagina_web.h"

WebServer server(80);
IPAddress ip(192, 168, 4, 1);
IPAddress mascaraRed(255, 255, 255, 0);

struct EstadoGrupo
{
  uint8_t mascaraOcupados = 0;
  bool hayDatos = false;
  uint32_t ultimaRespuesta = 0;
  uint32_t lecturasCorrectas = 0;
  uint32_t errores = 0;
  bool enlaceAnterior = false;
};
EstadoGrupo grupos[NUM_GRUPOS];
bool i2cListo = false;
bool servidorListo = false;
uint32_t ultimaConsulta = 0;
uint8_t siguienteGrupo = 0;

bool DatosVigentes(uint8_t indice)
{
  const EstadoGrupo &g = grupos[indice];
  return g.hayDatos && (uint32_t)(millis() - g.ultimaRespuesta) < DATOS_VENCIDOS_MS;
}

void EnviarIndicador(uint8_t grupo, uint8_t valor)
{
  Wire.beginTransmission(DIRECCIONES_NUCLEO[grupo]);
  Wire.write(valor);
  // Si falla, la siguiente ronda reintenta y el indicador vence localmente.
  (void)Wire.endTransmission();
}

void ActualizarI2C()
{
  const uint32_t ahora = millis();
  if (!i2cListo || (uint32_t)(ahora - ultimaConsulta) < PERIODO_I2C_MS / NUM_GRUPOS) return;
  ultimaConsulta = ahora;
  const uint8_t indice = siguienteGrupo;
  siguienteGrupo = (siguienteGrupo + 1U) % NUM_GRUPOS;
  EstadoGrupo &g = grupos[indice];

  const size_t recibidos = Wire.requestFrom((uint8_t)DIRECCIONES_NUCLEO[indice],
                                           (uint8_t)1, (uint8_t)true);
  if (recibidos == 1 && Wire.available() >= 1)
  {
    const uint8_t recibido = (uint8_t)Wire.read();
    if ((recibido & 0xF0U) == 0)
    {
      if (!DatosVigentes(indice) || recibido != g.mascaraOcupados)
        Serial.printf("I2C OK | Nucleo %u (0x%02X) | ocupacion 0x%02X\n",
                      indice + 1U, DIRECCIONES_NUCLEO[indice], recibido);
      g.mascaraOcupados = recibido;
      g.ultimaRespuesta = millis();
      g.hayDatos = true;
      ++g.lecturasCorrectas;
    }
    else ++g.errores;
  }
  else ++g.errores;
  while (Wire.available()) Wire.read();

  const bool enlace = DatosVigentes(indice);
  if (g.enlaceAnterior && !enlace) Serial.printf("Sin datos recientes de la Nucleo %u.\n", indice + 1U);
  g.enlaceAnterior = enlace;

  // Alternar la escritura hacia la otra placa para espaciar sus transacciones.
  if (indice == 0 && DatosVigentes(1))
  {
    EnviarIndicador(1, enlace ? g.mascaraOcupados : 0xFFU);
  }
  else if (indice == 1 && DatosVigentes(0))
  {
    uint8_t libres = NUM_ESPACIOS;
    for (uint8_t grupo = 0; grupo < NUM_GRUPOS; ++grupo)
    {
      if (!DatosVigentes(grupo))
      {
        libres = 0xFFU;
        break;
      }
      for (uint8_t espacio = 0; espacio < ESPACIOS_POR_GRUPO; ++espacio)
        libres -= (grupos[grupo].mascaraOcupados >> espacio) & 1U;
    }
    EnviarIndicador(0, libres);
  }
}

void handle_OnConnect()
{
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", PAGINA_HTML);
}

void handle_Estado()
{
  bool validos[NUM_GRUPOS];
  uint8_t ocupados = 0, libres = 0, sinDatos = 0, conectadas = 0;
  String espacios = "[";
  for (uint8_t g = 0; g < NUM_GRUPOS; ++g)
  {
    validos[g] = DatosVigentes(g);
    if (validos[g]) ++conectadas;
    for (uint8_t i = 0; i < ESPACIOS_POR_GRUPO; ++i)
    {
      if (g || i) espacios += ',';
      if (!validos[g]) { espacios += "null"; ++sinDatos; }
      else if (grupos[g].mascaraOcupados & (1U << i)) { espacios += "true"; ++ocupados; }
      else { espacios += "false"; ++libres; }
    }
  }
  espacios += ']';
  char cabecera[384];
  (void)snprintf(cabecera, sizeof(cabecera),
    "{\"conectado\":%s,\"completo\":%s,\"total\":8,\"libres_confirmados\":%u,"
    "\"ocupados_confirmados\":%u,\"sin_datos\":%u,\"nucleos_conectadas\":%u,\"espacios\":",
    conectadas ? "true" : "false", sinDatos == 0 ? "true" : "false", libres, ocupados, sinDatos, conectadas);
  String json;
  json.reserve(800);
  json = cabecera;
  json += espacios;
  json += ",\"grupos\":[";
  for (uint8_t g = 0; g < NUM_GRUPOS; ++g)
  {
    if (g) json += ',';
    (void)snprintf(cabecera, sizeof(cabecera),
      "{\"direccion\":%u,\"conectado\":%s,\"lecturas\":%lu,\"errores\":%lu}",
      DIRECCIONES_NUCLEO[g], validos[g] ? "true" : "false",
      (unsigned long)grupos[g].lecturasCorrectas, (unsigned long)grupos[g].errores);
    json += cabecera;
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store, max-age=0");
  server.send(200, "application/json; charset=utf-8", json);
}

void handle_NotFound()
{
  server.send(404, "text/plain; charset=utf-8", "Pagina no encontrada");
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("\nParqueo: ESP32 maestro I2C + WebServer AP");
  i2cListo = Wire.begin(PIN_SDA, PIN_SCL, 100000);
  if (i2cListo) Wire.setTimeOut(50);
  else Serial.println("ERROR al iniciar I2C; la pagina mostrara Sin datos.");

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAPConfig(ip, ip, mascaraRed) || !WiFi.softAP(WIFI_NOMBRE, WIFI_CLAVE))
  {
    Serial.println("ERROR al crear la red Wi-Fi. Reiniciar el ESP32.");
    return;
  }

  server.on("/", HTTP_GET, handle_OnConnect);
  server.on("/api/estado", HTTP_GET, handle_Estado);
  server.on("/favicon.ico", HTTP_GET, []() { server.send(204, "text/plain", ""); });
  server.onNotFound(handle_NotFound);
  server.begin();
  servidorListo = true;
  Serial.print("Red Wi-Fi: "); Serial.println(WIFI_NOMBRE);
  Serial.print("Abrir en el navegador: http://"); Serial.println(WiFi.softAPIP());
  Serial.println("Datos I2C cada 100 ms; pagina cada 500 ms. No requiere internet.");
}

void loop()
{
  ActualizarI2C();
  if (servidorListo) server.handleClient();
  delay(2);
}
