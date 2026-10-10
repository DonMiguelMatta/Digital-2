#ifndef GAMEPAD_PROTOCOL_H
#define GAMEPAD_PROTOCOL_H
#include <stdint.h>
#define GP_FRAME_SIZE 4u
#define GP_CONNECTED 1u
#define GP_STATUS_TAG 0xA0u
#define GP_LEFT  (1u << 0)
#define GP_RIGHT (1u << 1)
#define GP_UP    (1u << 2)
#define GP_DOWN  (1u << 3)
#define GP_JUMP  (1u << 4)
#define GP_DASH  (1u << 5)
#define GP_START (1u << 6)
// [0] jugador, [1] mascara de botones, [2] A0/desconectado o A1/conectado,
// [3] CRC-8/SMBUS de bytes 0..2: poly 07, init 00, sin reflexion/XOR final.
static inline uint8_t gp_crc8(const uint8_t *p, unsigned n) {
    uint8_t crc = 0;
    while (n--) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (uint8_t)((crc << 1) ^ ((crc & 0x80) ? 0x07 : 0));
    }
    return crc;
}
static inline void gp_pack(uint8_t *frame, uint8_t player,
                           uint8_t buttons, uint8_t connected) {
    frame[0] = player;
    frame[1] = buttons & 0x7f;
    frame[2] = GP_STATUS_TAG | (connected ? GP_CONNECTED : 0);
    frame[3] = gp_crc8(frame, 3);
}
static inline uint8_t gp_valid(const uint8_t *frame, uint8_t player) {
    return frame[0] == player && (frame[1] & 0x80) == 0 &&
        (frame[2] & 0xfe) == GP_STATUS_TAG && frame[3] == gp_crc8(frame, 3);
}
#endif
