/*
 * Proyecto CarWash - Puente ESP32 con Adafruit IO
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Recibe telemetria del Master y envia ordenes desde Adafruit IO
 */
/****************************************/
// Encabezado (Libraries)

#include <Arduino.h>
#include <HardwareSerial.h>
#include <string.h>
#include "AdafruitIO_WiFi.h"

#define MASTER_UART_BAUD              9600
#define MASTER_UART_RX_PIN              16
#define MASTER_UART_TX_PIN              17
#define MASTER_LINE_SIZE                96
#define FEED_VALUE_SIZE                 40
#define IO_LOOP_DELAY                 2200UL
#define IO_CONNECT_WAIT_MS           20000UL
#define IO_USERNAME "Don22993"
#define IO_KEY      "aio_YLNB64MZRprknfEZf328xEP8BjxJ"
#define WIFI_SSID   "Redmi15"
#define WIFI_PASS   "12345678"



AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

enum FeedIndex
{
  FEED_DC = 0,
  FEED_HCR,
  FEED_IR,
  FEED_LCD,
  FEED_WATER,
  FEED_DOOR,
  FEED_STEPPER,
  FEED_VL53,
  FEED_COUNT
};

struct FeedState
{
  AdafruitIO_Feed *feed;
  const char *uartKey;
  char value[FEED_VALUE_SIZE];
  bool valid;
  bool dirty;
};

HardwareSerial MasterSerial(2);

AdafruitIO_Feed *canalDC = io.feed("digital-2.dc");
AdafruitIO_Feed *canalHCR = io.feed("digital-2.hcr");
AdafruitIO_Feed *canalIR = io.feed("digital-2.ir");
AdafruitIO_Feed *canalLCD = io.feed("digital-2.lcd");
AdafruitIO_Feed *canalServoAgua = io.feed("digital-2.servo-agua");
AdafruitIO_Feed *canalServoPuerta = io.feed("digital-2.servo-puerta");
AdafruitIO_Feed *canalStepper = io.feed("digital-2.stepper");
AdafruitIO_Feed *canalVL53L0X = io.feed("digital-2.vl53l0x");

FeedState feeds[FEED_COUNT] =
{
  {canalDC, "DC", "", false, false},
  {canalHCR, "HCR", "", false, false},
  {canalIR, "IR", "", false, false},
  {canalLCD, "LCD", "", false, false},
  {canalServoAgua, "WA", "", false, false},
  {canalServoPuerta, "DO", "", false, false},
  {canalStepper, "ST", "", false, false},
  {canalVL53L0X, "VL", "", false, false}
};

char masterLine[MASTER_LINE_SIZE];
size_t masterLineLength = 0U;
unsigned long lastUpdate = 0UL;
uint8_t nextFeed = 0U;
bool manualMode = false;
bool adafruitConnected = false;

/****************************************/
// Function prototypes

void handleDC(AdafruitIO_Data *data);
void handleWater(AdafruitIO_Data *data);
void handleDoor(AdafruitIO_Data *data);
void handleStepper(AdafruitIO_Data *data);
void handleLCD(AdafruitIO_Data *data);

/****************************************/
// NON-Interrupt subroutines

// Busca el canal asociado a una clave enviada por el Master.
static int8_t findFeed(const char *key)
{
  for (uint8_t index = 0U; index < FEED_COUNT; index++)
  {
    if (strcmp(key, feeds[index].uartKey) == 0)
    {
      return (int8_t)index;
    }
  }

  return -1;
}

// Conserva el valor mas reciente y lo marca para publicar.
static void cacheTelemetry(uint8_t index, const char *value)
{
  if ((index >= FEED_COUNT) || (value == nullptr))
  {
    return;
  }

  if ((!feeds[index].valid) ||
      (strncmp(feeds[index].value, value, FEED_VALUE_SIZE) != 0))
  {
    strncpy(feeds[index].value, value, FEED_VALUE_SIZE - 1U);
    feeds[index].value[FEED_VALUE_SIZE - 1U] = '\0';
    feeds[index].valid = true;
    feeds[index].dirty = true;
  }
}

// Interpreta una trama @CW,T,CLAVE,VALOR procedente del Master.
static void processMasterLine(char *line)
{
  char *key;
  char *value;
  char *separator;
  int8_t index;

  if (strncmp(line, "@CW,T,", 6U) != 0)
  {
    return;
  }

  key = &line[6];
  separator = strchr(key, ',');
  if (separator == nullptr)
  {
    return;
  }

  *separator = '\0';
  value = separator + 1;

  if (strcmp(key, "MODE") == 0)
  {
    manualMode = (atoi(value) != 0);
    Serial.print("Modo recibido: ");
    Serial.println(manualMode ? "MANUAL" : "AUTOMATICO");
    return;
  }

  index = findFeed(key);
  if (index >= 0)
  {
    cacheTelemetry((uint8_t)index, value);
  }
}

// Recibe lineas del Master por UART2 sin detener io.run().
static void serviceMasterUART(void)
{
  while (MasterSerial.available() > 0)
  {
    char data = (char)MasterSerial.read();

    if (data == '\r')
    {
      continue;
    }

    if (data == '\n')
    {
      masterLine[masterLineLength] = '\0';
      if (masterLineLength > 0U)
      {
        processMasterLine(masterLine);
      }
      masterLineLength = 0U;
      continue;
    }

    if (masterLineLength < (MASTER_LINE_SIZE - 1U))
    {
      masterLine[masterLineLength] = data;
      masterLineLength++;
    }
    else
    {
      masterLineLength = 0U;
    }
  }
}

// Envia una orden compacta al receptor D12 del Master.
static void sendMasterCommand(const char *device, long value)
{
  if (!manualMode)
  {
    Serial.println("Orden ignorada: Master en modo automatico.");
    return;
  }

  MasterSerial.print("@CW,C,");
  MasterSerial.print(device);
  MasterSerial.print(',');
  MasterSerial.println(value);
}

// Evita devolver al Master una telemetria publicada por el propio ESP32.
static bool isCurrentValue(uint8_t index, long value)
{
  if ((index >= FEED_COUNT) || !feeds[index].valid)
  {
    return false;
  }

  return strtol(feeds[index].value, nullptr, 10) == value;
}

// Publica un solo feed pendiente para respetar el limite gratuito.
static void publishNextTelemetry(void)
{
  unsigned long now = millis();

  if (!adafruitConnected ||
      ((now - lastUpdate) < IO_LOOP_DELAY))
  {
    return;
  }

  for (uint8_t checked = 0U; checked < FEED_COUNT; checked++)
  {
    uint8_t index = (uint8_t)((nextFeed + checked) % FEED_COUNT);

    if (feeds[index].dirty && feeds[index].valid)
    {
      Serial.print("sending -> ");
      Serial.print(feeds[index].uartKey);
      Serial.print(": ");
      Serial.println(feeds[index].value);

      if (feeds[index].feed->save(feeds[index].value))
      {
        feeds[index].dirty = false;
      }

      nextFeed = (uint8_t)((index + 1U) % FEED_COUNT);
      lastUpdate = now;
      return;
    }
  }
}

// Muestra el valor recibido desde un feed de Adafruit IO.
static void printReceived(const char *channel, AdafruitIO_Data *data)
{
  Serial.print("received <- ");
  Serial.print(channel);
  Serial.print(": ");
  Serial.println(data->value());
}

// Procesa el PWM firmado recibido desde Adafruit IO.
void handleDC(AdafruitIO_Data *data)
{
  long value = data->toLong();

  printReceived("DC", data);
  if ((value >= -255L) && (value <= 255L) &&
      !isCurrentValue(FEED_DC, value))
  {
    sendMasterCommand("DC", value);
  }
}

// Procesa el angulo solicitado para el servo de agua.
void handleWater(AdafruitIO_Data *data)
{
  long value = data->toLong();

  printReceived("Servo Agua", data);
  if ((value >= 0L) && (value <= 180L) &&
      !isCurrentValue(FEED_WATER, value))
  {
    sendMasterCommand("WA", value);
  }
}

// Procesa el angulo solicitado para el servo de puerta.
void handleDoor(AdafruitIO_Data *data)
{
  long value = data->toLong();

  printReceived("Servo puerta", data);
  if ((value >= 0L) && (value <= 180L) &&
      !isCurrentValue(FEED_DOOR, value))
  {
    sendMasterCommand("DO", value);
  }
}

// Procesa velocidad y direccion firmadas del stepper.
void handleStepper(AdafruitIO_Data *data)
{
  long value = data->toLong();

  printReceived("Stepper", data);
  if ((value >= -1000L) && (value <= 1000L) &&
      !isCurrentValue(FEED_STEPPER, value))
  {
    sendMasterCommand("ST", value);
  }
}

// Permite escribir STOP en el feed LCD para detener todo en manual.
void handleLCD(AdafruitIO_Data *data)
{
  printReceived("LCD", data);
  if (manualMode && (strcasecmp(data->value(), "STOP") == 0))
  {
    sendMasterCommand("STOP", 0L);
  }
}

// Solicita los valores actuales de los feeds que aceptan instrucciones.
static void getControlFeeds(void)
{
  canalDC->get();
  canalServoAgua->get();
  canalServoPuerta->get();
  canalStepper->get();
  canalLCD->get();
}

// Detecta conexiones nuevas y recupera nuevamente los controles.
static void serviceAdafruitConnection(void)
{
  bool connected = (io.status() >= AIO_CONNECTED);

  if (connected && !adafruitConnected)
  {
    adafruitConnected = true;
    Serial.println(io.statusText());
    getControlFeeds();
  }
  else if (!connected && adafruitConnected)
  {
    adafruitConnected = false;
    Serial.println("Adafruit IO desconectado; esperando reconexion.");
  }
}

/****************************************/
// Main Function

void setup()
{
  unsigned long startedMs;

  Serial.begin(115200);
  MasterSerial.begin(MASTER_UART_BAUD,
                     SERIAL_8N1,
                     MASTER_UART_RX_PIN,
                     MASTER_UART_TX_PIN);

  canalDC->onMessage(handleDC);
  canalServoAgua->onMessage(handleWater);
  canalServoPuerta->onMessage(handleDoor);
  canalStepper->onMessage(handleStepper);
  canalLCD->onMessage(handleLCD);

  Serial.print("Conectando a Adafruit IO");
  io.connect();
  startedMs = millis();
  while ((io.status() < AIO_CONNECTED) &&
         ((millis() - startedMs) < IO_CONNECT_WAIT_MS))
  {
    io.run();
    serviceMasterUART();
    Serial.print('.');
    delay(250);
  }

  Serial.println();
  if (io.status() >= AIO_CONNECTED)
  {
    serviceAdafruitConnection();
  }
  else
  {
    Serial.print(io.statusText());
    Serial.println("; la conexion continuara en loop().");
  }
}

void loop()
{
  io.run();
  serviceMasterUART();
  serviceAdafruitConnection();
  publishNextTelemetry();
  delay(2);
}

/****************************************/
// Interrupt routines
// Arduino y las bibliotecas administran las interrupciones del ESP32.
