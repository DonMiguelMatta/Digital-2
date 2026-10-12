#ifndef GAMEPAD_PROTOCOL_H
#define GAMEPAD_PROTOCOL_H
#include <stdint.h>
#define GP_SYNC 0xA5u
#define GP_FRAME_SIZE 5u
#define GP_CONNECTED 1u
#define GP_STATUS_TAG 0xA0u
#define GP_LEFT  (1u << 0)
#define GP_RIGHT (1u << 1)
#define GP_UP    (1u << 2)
#define GP_DOWN  (1u << 3)
#define GP_JUMP  (1u << 4)
#define GP_DASH  (1u << 5)
#define GP_START (1u << 6)
// [0] sincronizacion, [1] jugador, [2] botones, [3] estado,
// [4] CRC-8/SMBUS de bytes 0..3: poly 07, init 00, sin reflexion/XOR final.
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
    frame[0] = GP_SYNC;
    frame[1] = player;
    frame[2] = buttons & 0x7f;
    frame[3] = GP_STATUS_TAG | (connected ? GP_CONNECTED : 0);
    frame[4] = gp_crc8(frame, 4);
}
static inline uint8_t gp_valid(const uint8_t *frame, uint8_t player) {
    return frame[0] == GP_SYNC && frame[1] == player &&
        (frame[2] & 0x80) == 0 && (frame[3] & 0xfe) == GP_STATUS_TAG &&
        frame[4] == gp_crc8(frame, 4);
}
#endif
