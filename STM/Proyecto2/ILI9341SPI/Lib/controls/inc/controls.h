#ifndef CONTROLS_H
#define CONTROLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONTROLS_REMOTE_BUTTON_COUNT 6U

typedef uint32_t Controls_ActionMask;

enum
{
  CONTROLS_ACTION_NONE   = 0U,
  CONTROLS_ACTION_UP     = (1U << 0),
  CONTROLS_ACTION_DOWN   = (1U << 1),
  CONTROLS_ACTION_LEFT   = (1U << 2),
  CONTROLS_ACTION_RIGHT  = (1U << 3),
  CONTROLS_ACTION_A      = (1U << 4),
  CONTROLS_ACTION_B      = (1U << 5),
  CONTROLS_ACTION_START  = (1U << 6),
  CONTROLS_ACTION_SELECT = (1U << 7)
};

typedef struct
{
  uint16_t low_threshold;
  uint16_t high_threshold;
  Controls_ActionMask low_action;
  Controls_ActionMask high_action;
} Controls_AxisConfig;

typedef struct
{
  Controls_AxisConfig vertical_axis;
  Controls_AxisConfig horizontal_axis;
  Controls_ActionMask remote_button_actions[CONTROLS_REMOTE_BUTTON_COUNT];
} Controls_Config;

typedef struct
{
  Controls_Config config;
  uint16_t vertical_sample;
  uint16_t horizontal_sample;
  Controls_ActionMask local_button_actions;
  Controls_ActionMask local_held_actions;
  Controls_ActionMask remote_held_actions;
  Controls_ActionMask pressed_actions;
  uint8_t waiting_remote_button;
} Controls_t;

/* Creates a configuration compatible with the joystick calibration in Lab 6. */
Controls_Config Controls_ConfigDefault(void);

/* Copies the configuration and clears the input state. */
void Controls_Init(Controls_t *controls, const Controls_Config *config);

/* Supplies the most recent DMA/ADC samples. It does not access any peripheral. */
void Controls_SetAnalogSamples(Controls_t *controls,
                               uint16_t vertical,
                               uint16_t horizontal);

/* Supplies local digital buttons as an action mask. */
void Controls_SetLocalButtons(Controls_t *controls,
                              Controls_ActionMask actions);

/* Supplies held actions from a wireless protocol that includes press/release states. */
void Controls_SetRemoteHeld(Controls_t *controls,
                            Controls_ActionMask actions);

/* Updates local held and pressed actions. Call this once from the main loop. */
void Controls_Update(Controls_t *controls);

/*
 * Parses the b1 through b6 event protocol used by the external controller.
 * Call it from foreground code after removing received bytes from an RX queue.
 */
void Controls_ProcessWirelessByte(Controls_t *controls, uint8_t byte);

Controls_ActionMask Controls_GetHeld(const Controls_t *controls);

/* Returns and clears actions that changed from released to pressed. */
Controls_ActionMask Controls_TakePressed(Controls_t *controls);

void Controls_Reset(Controls_t *controls);

#ifdef __cplusplus
}
#endif

#endif /* CONTROLS_H */
