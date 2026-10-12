#include <Arduino.h>
#include <Bluepad32.h>
#include <uni.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "gamepad_protocol.h"

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error Este firmware requiere ESP32 original (WROOM-32), con Bluetooth Classic.
#endif

static_assert(PLAYER_ID == 1 || PLAYER_ID == 2, "PLAYER_ID debe ser 1 o 2");
static_assert(GP_FRAME_SIZE == 5, "El protocolo UART debe tener 5 bytes");

ControllerPtr pad = nullptr;
HardwareSerial controlUart(2);

// Protege el estado porque Bluetooth y UART trabajan en tareas distintas.
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

struct PadState {
    uint16_t buttons;
    int16_t x;
    int16_t y;
    uint8_t connected;
};

PadState state = {};
uint32_t lastReportMs = 0;

// Deja todos los controles en reposo cuando se pierde la conexion.
void neutral() {
    portENTER_CRITICAL(&stateMux);
    state.buttons = 0;
    state.x = 0;
    state.y = 0;
    state.connected = 0;
    portEXIT_CRITICAL(&stateMux);
}

void onConnected(ControllerPtr ctl) {
    auto properties = ctl->getProperties();

    // Solo se acepta un control y, si esta activo el filtro, su MAC conocida.
    if (pad == ctl) return;
    if (pad != nullptr ||
        (FILTER_CONTROLLER_MAC &&
         memcmp(properties.btaddr, CONTROLLER_MAC, 6) != 0)) {
        ctl->disconnect();
        return;
    }

    pad = ctl;
    lastReportMs = millis();
    ctl->setColorLED(PLAYER_ID == 1 ? 0 : 255,
                     0,
                     PLAYER_ID == 1 ? 255 : 0);
}

void onDisconnected(ControllerPtr ctl) {
    if (ctl == pad) {
        pad = nullptr;
        neutral();
    }
}

void readPad() {
    int16_t x = (int16_t)pad->axisX();
    int16_t y = (int16_t)pad->axisY();
    uint8_t dpad = pad->dpad();
    uint16_t buttons = 0;

    // Convierte los botones de Bluepad32 a la mascara enviada a la STM32.
    if (dpad & 0x08) buttons |= GP_LEFT;
    if (dpad & 0x04) buttons |= GP_RIGHT;
    if (dpad & 0x01) buttons |= GP_UP;
    if (dpad & 0x02) buttons |= GP_DOWN;
    if (pad->a()) buttons |= GP_JUMP;
    if (pad->b()) buttons |= GP_DASH;
    if (pad->miscStart()) buttons |= GP_START;

    // Dos direcciones opuestas se cancelan entre si.
    if ((buttons & (GP_LEFT | GP_RIGHT)) == (GP_LEFT | GP_RIGHT))
        buttons &= ~(GP_LEFT | GP_RIGHT);
    if ((buttons & (GP_UP | GP_DOWN)) == (GP_UP | GP_DOWN))
        buttons &= ~(GP_UP | GP_DOWN);

    portENTER_CRITICAL(&stateMux);
    state.buttons = buttons;
    state.x = x;
    state.y = y;
    state.connected = GP_CONNECTED;
    portEXIT_CRITICAL(&stateMux);
    lastReportMs = millis();
}

void sendControlFrame(const PadState& snapshot) {
    uint8_t frame[GP_FRAME_SIZE];

    // Arma los cinco bytes: A5, jugador, botones, estado y CRC.
    gp_pack(frame,
            PLAYER_ID,
            (uint8_t)snapshot.buttons,
            snapshot.connected);
    controlUart.write(frame, sizeof(frame));
    controlUart.flush();  // Espera a que la trama termine de salir por TX2.
}

// Mantiene el enlace UART a 50 paquetes por segundo, aunque Bluetooth se demore.
void controlUartTask(void*) {
    TickType_t nextWake = xTaskGetTickCount();

    for (;;) {
        PadState snapshot;

        // Se copia rapido para no bloquear la lectura del control.
        portENTER_CRITICAL(&stateMux);
        snapshot = state;
        portEXIT_CRITICAL(&stateMux);

        sendControlFrame(snapshot);
        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(CONTROL_FRAME_PERIOD_MS));
    }
}

void setup() {
    // UART2 del ESP32: solamente transmite por GPIO17/TX2.
    if (ENABLE_CONTROL_UART) {
        controlUart.begin(CONTROL_UART_BAUD_RATE,
                          SERIAL_8N1,
                          -1,
                          CONTROL_UART_TX_PIN);
    }

    // Inicia Bluepad32 y registra los eventos de conexion.
    BP32.setup(&onConnected, &onDisconnected);
    BP32.enableVirtualDevice(false);
    if (ERASE_PAIRING_KEYS) BP32.forgetBluetoothKeys();

    // El filtro evita que otro control cercano se conecte a este jugador.
    if (FILTER_CONTROLLER_MAC) {
        bd_addr_t address;
        memcpy(address, CONTROLLER_MAC, 6);
        uni_bt_allowlist_remove_all();
        uni_bt_allowlist_add_addr(address);
        uni_bt_allowlist_set_enabled(true);
    } else {
        uni_bt_allowlist_set_enabled(false);
    }

    // Esta tarea mantiene el envio aunque el loop de Bluetooth se atrase.
    if (ENABLE_CONTROL_UART &&
        xTaskCreatePinnedToCore(controlUartTask,
                                "control_uart",
                                2048,
                                nullptr,
                                1,
                                nullptr,
                                1) != pdPASS) {
        abort();
    }
}

void loop() {
    bool updated = BP32.update();
    uint32_t now = millis();

    // Solo se vuelve a leer el control cuando Bluepad32 tiene datos nuevos.
    if (pad && pad->isConnected()) {
        if (updated && pad->hasData() && pad->isGamepad()) readPad();
        if ((uint32_t)(now - lastReportMs) > BT_REPORT_TIMEOUT_MS) neutral();
    } else {
        neutral();
    }

    delay(1);
}
