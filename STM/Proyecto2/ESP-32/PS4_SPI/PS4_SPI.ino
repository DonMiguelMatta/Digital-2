#include <Arduino.h>
#include <Bluepad32.h>
#include <uni.h>
#include <driver/spi_slave.h>
#include <soc/soc.h>
#include <soc/gpio_reg.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"
#include "gamepad_protocol.h"

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error Este firmware requiere ESP32 original (WROOM-32), con Bluetooth Classic.
#endif
static_assert(PLAYER_ID == 1 || PLAYER_ID == 2, "PLAYER_ID debe ser 1 o 2");
static_assert(GP_FRAME_SIZE == 4, "El protocolo SPI actual debe ser de 4 bytes");
static_assert(PIN_READY < 32, "Callbacks READY requieren GPIO menor a 32");
ControllerPtr pad = nullptr;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
struct PadState {
    uint16_t buttons, jumpCount, dashCount, startCount;
    int16_t x, y;
    uint8_t connected;
};
PadState state = {};
QueueHandle_t spiSnapshots = nullptr;
uint32_t lastReportMs = 0;
uint32_t spiCompleted = 0, spiBadLength = 0, spiLastBits = 0;

// Guardar estados de pulsacion y liberacion mientras SPI espera al maestro.
void publishState() {
    if (!spiSnapshots) return;
    PadState snapshot;
    portENTER_CRITICAL(&stateMux); snapshot = state; portEXIT_CRITICAL(&stateMux);
    if (xQueueSend(spiSnapshots, &snapshot, 0) != pdTRUE) {
        // Cola acotada: priorizar estados recientes; conservar contadores.
        PadState discarded;
        xQueueReceive(spiSnapshots, &discarded, 0);
        xQueueSend(spiSnapshots, &snapshot, 0);
    }
}

// Estos callbacks corren en la ISR: solo modificar el GPIO, sin Serial ni BT.
void IRAM_ATTR spiReady(spi_slave_transaction_t *) {
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << PIN_READY);
}
void IRAM_ATTR spiDone(spi_slave_transaction_t *) {
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << PIN_READY);
}
void neutral() {
    portENTER_CRITICAL(&stateMux);
    bool changed = state.connected != 0 || state.buttons != 0;
    state.buttons = 0; state.x = 0; state.y = 0; state.connected = 0;
    portEXIT_CRITICAL(&stateMux);
    if (changed) publishState();
}
void onConnected(ControllerPtr ctl) {
    auto p = ctl->getProperties();
    Serial.printf("Control: %s MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
        ctl->getModelName().c_str(), p.btaddr[0], p.btaddr[1], p.btaddr[2],
        p.btaddr[3], p.btaddr[4], p.btaddr[5]);
    // Aceptar el control; su clase HID se comprueba al recibir datos.
    if (pad == ctl) return;
    if (pad != nullptr ||
        (FILTER_CONTROLLER_MAC && memcmp(p.btaddr, CONTROLLER_MAC, 6) != 0)) {
        ctl->disconnect(); return;
    }
    pad = ctl; lastReportMs = millis();
    ctl->setColorLED(PLAYER_ID == 1 ? 0 : 255, 0, PLAYER_ID == 1 ? 255 : 0);
    // connected se publica cuando llega el primer informe HID.
}
void onDisconnected(ControllerPtr ctl) {
    if (ctl == pad) { pad = nullptr; neutral(); Serial.println("Control desconectado"); }
}
void readPad() {
    int16_t x = (int16_t)pad->axisX(), y = (int16_t)pad->axisY();
    uint8_t d = pad->dpad();
    uint16_t b = 0;
    // Direcciones exclusivamente de la cruceta fisica del DualShock 4.
    if (d & 0x08) b |= GP_LEFT;
    if (d & 0x04) b |= GP_RIGHT;
    if (d & 0x01) b |= GP_UP;
    if (d & 0x02) b |= GP_DOWN;
    if (pad->a()) b |= GP_JUMP;   // Cruz
    if (pad->b()) b |= GP_DASH;   // Circulo = segundo salto del main actual
    if (pad->miscStart()) b |= GP_START; // Options
    // Direcciones opuestas se neutralizan.
    if ((b & (GP_LEFT|GP_RIGHT)) == (GP_LEFT|GP_RIGHT)) b &= ~(GP_LEFT|GP_RIGHT);
    if ((b & (GP_UP|GP_DOWN)) == (GP_UP|GP_DOWN)) b &= ~(GP_UP|GP_DOWN);
    portENTER_CRITICAL(&stateMux);
    bool changed = b != state.buttons || state.connected != GP_CONNECTED;
    uint16_t rises = b & ~state.buttons;
    // Contadores para el diagnostico UART; no forman parte de los 4 bytes SPI.
    if (rises & GP_JUMP) ++state.jumpCount;
    if (rises & GP_DASH) ++state.dashCount;
    if (rises & GP_START) ++state.startCount;
    state.buttons = b; state.x = x; state.y = y; state.connected = GP_CONNECTED;
    portEXIT_CRITICAL(&stateMux);
    // Generar eventos solo por botones de la cruceta/acciones y conexion.
    if (changed) publishState();
    lastReportMs = millis();
}
void spiTask(void *) {
    spi_bus_config_t bus = {};
    bus.sclk_io_num = PIN_SCK; bus.miso_io_num = PIN_MISO;
    bus.mosi_io_num = PIN_MOSI; bus.quadwp_io_num = -1; bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 8;
    spi_slave_interface_config_t slave = {};
    slave.spics_io_num = PIN_CS; slave.mode = 0; slave.queue_size = 1;
    slave.post_setup_cb = spiReady; slave.post_trans_cb = spiDone;
    // Paquete de 4 bytes sin DMA, modo 0.
    ESP_ERROR_CHECK(spi_slave_initialize(SPI3_HOST, &bus, &slave, 0));
    Serial.printf("SPI J%d: modo=0 SCK=%d MISO=%d MOSI=%d CS=%d READY=%d; lectura=4 bytes\n",
        PLAYER_ID, PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS, PIN_READY);
    for (;;) {
        PadState copy;
        // Repetir mientras hay una accion mantenida; enviar tambien cambios
        // de conexion y liberacion que llegan por la cola.
        if (xQueueReceive(spiSnapshots, &copy, pdMS_TO_TICKS(20)) != pdTRUE) {
            portENTER_CRITICAL(&stateMux); copy = state; portEXIT_CRITICAL(&stateMux);
            if (copy.buttons == 0 || !copy.connected) continue;
        }
        // 8 bytes alineados de almacenamiento permiten detectar lecturas
        // mayores a 32 bits; el paquete en el cable es de 4 bytes.
        uint32_t txWords[2] = {}, rxWords[2] = {};
        uint8_t *tx = reinterpret_cast<uint8_t *>(txWords);
        gp_pack(tx, PLAYER_ID, (uint8_t)copy.buttons, copy.connected);
        spi_slave_transaction_t transaction = {};
        transaction.length = sizeof(txWords) * 8;
        transaction.tx_buffer = tx;
        transaction.rx_buffer = rxWords;
        // Bloquea esta tarea, nunca el loop Bluetooth. El buffer sigue vivo.
        ESP_ERROR_CHECK(spi_slave_transmit(SPI3_HOST, &transaction, portMAX_DELAY));
        portENTER_CRITICAL(&stateMux);
        ++spiCompleted; spiLastBits = transaction.trans_len;
        if (transaction.trans_len != GP_FRAME_SIZE * 8) ++spiBadLength;
        portEXIT_CRITICAL(&stateMux);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void setup() {
    Serial.begin(115200);
    pinMode(PIN_READY, OUTPUT); digitalWrite(PIN_READY, LOW);
    pinMode(PIN_CS, INPUT_PULLUP);
    if (ENABLE_SPI) {
        spiSnapshots = xQueueCreate(16, sizeof(PadState));
        if (!spiSnapshots) abort();
        publishState(); // Estado inicial una vez, aun sin control conectado.
    }
    BP32.setup(&onConnected, &onDisconnected);
    BP32.enableVirtualDevice(false);
    if (ERASE_PAIRING_KEYS) BP32.forgetBluetoothKeys();
    if (FILTER_CONTROLLER_MAC) {
        bd_addr_t addr; memcpy(addr, CONTROLLER_MAC, 6);
        uni_bt_allowlist_remove_all();
        uni_bt_allowlist_add_addr(addr); uni_bt_allowlist_set_enabled(true);
    } else {
        uni_bt_allowlist_set_enabled(false);
    }
    if (ENABLE_SPI && xTaskCreatePinnedToCore(spiTask, "spi_pad", 4096, nullptr, 1, nullptr, 1) != pdPASS)
        abort();
    Serial.printf("Jugador %d listo. Emparejar con SHARE + PS.\n", PLAYER_ID);
}
void loop() {
    bool updated = BP32.update();
    if (pad && pad->isConnected()) {
        if (updated && pad->hasData() && pad->isGamepad()) readPad();
        if ((uint32_t)(millis() - lastReportMs) > BT_REPORT_TIMEOUT_MS) neutral();
    } else neutral();
    if (SERIAL_DIAGNOSTICS) {
        static bool printed = false;
        static PadState previous = {};
            PadState copy;
            uint32_t completed, badLength, bits;
            portENTER_CRITICAL(&stateMux);
            copy = state; completed = spiCompleted; badLength = spiBadLength; bits = spiLastBits;
            portEXIT_CRITICAL(&stateMux);
        // Comparar acciones digitales, no ruido analogico ni contadores SPI.
        bool changed = !printed || copy.connected != previous.connected ||
            copy.buttons != previous.buttons || copy.jumpCount != previous.jumpCount ||
            copy.dashCount != previous.dashCount || copy.startCount != previous.startCount;
        if (changed) {
            printed = true; previous = copy;
            Serial.printf("J%d conectado=%u botones=0x%02X X=%d Y=%d salto=%u secundario=%u inicio=%u READY=%d SPI_trans=%lu bits=%lu longitud_mal=%lu\n",
                PLAYER_ID, (unsigned)copy.connected, (unsigned)copy.buttons,
                (int)copy.x, (int)copy.y, (unsigned)copy.jumpCount,
                (unsigned)copy.dashCount, (unsigned)copy.startCount, digitalRead(PIN_READY),
                (unsigned long)completed, (unsigned long)bits, (unsigned long)badLength);
        }
    }
    delay(1);
}
