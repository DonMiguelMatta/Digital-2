#include "controls.h"

static Controls_ActionMask Controls_ReadAxis(uint16_t sample,
                                             const Controls_AxisConfig *axis)
{
  if (sample < axis->low_threshold)
  {
    return axis->low_action;
  }

  if (sample > axis->high_threshold)
  {
    return axis->high_action;
  }

  return CONTROLS_ACTION_NONE;
}

static void Controls_AddPressed(Controls_t *controls,
                                Controls_ActionMask previous,
                                Controls_ActionMask current)
{
  controls->pressed_actions |= current & ~previous;
}

Controls_Config Controls_ConfigDefault(void)
{
  Controls_Config config = {
      .vertical_axis = {
          .low_threshold = 2048U,
          .high_threshold = 2099U,
          .low_action = CONTROLS_ACTION_DOWN,
          .high_action = CONTROLS_ACTION_UP,
      },
      .horizontal_axis = {
          .low_threshold = 1960U,
          .high_threshold = 2100U,
          .low_action = CONTROLS_ACTION_RIGHT,
          .high_action = CONTROLS_ACTION_LEFT,
      },
      .remote_button_actions = {
          CONTROLS_ACTION_B,
          CONTROLS_ACTION_A,
          CONTROLS_ACTION_DOWN,
          CONTROLS_ACTION_UP,
          CONTROLS_ACTION_RIGHT,
          CONTROLS_ACTION_LEFT,
      },
  };

  return config;
}

void Controls_Init(Controls_t *controls, const Controls_Config *config)
{
  if (controls == 0 || config == 0)
  {
    return;
  }

  controls->config = *config;
  Controls_Reset(controls);
}

void Controls_SetAnalogSamples(Controls_t *controls,
                               uint16_t vertical,
                               uint16_t horizontal)
{
  if (controls == 0)
  {
    return;
  }

  controls->vertical_sample = vertical;
  controls->horizontal_sample = horizontal;
}

void Controls_SetLocalButtons(Controls_t *controls,
                              Controls_ActionMask actions)
{
  if (controls == 0)
  {
    return;
  }

  controls->local_button_actions = actions;
}

void Controls_SetRemoteHeld(Controls_t *controls,
                            Controls_ActionMask actions)
{
  Controls_ActionMask previous;

  if (controls == 0)
  {
    return;
  }

  previous = controls->remote_held_actions;
  controls->remote_held_actions = actions;
  Controls_AddPressed(controls, previous, actions);
}

void Controls_Update(Controls_t *controls)
{
  Controls_ActionMask actions;
  Controls_ActionMask previous;

  if (controls == 0)
  {
    return;
  }

  actions = Controls_ReadAxis(controls->vertical_sample,
                              &controls->config.vertical_axis);
  actions |= Controls_ReadAxis(controls->horizontal_sample,
                               &controls->config.horizontal_axis);
  actions |= controls->local_button_actions;

  previous = controls->local_held_actions;
  controls->local_held_actions = actions;
  Controls_AddPressed(controls, previous, actions);
}

void Controls_ProcessWirelessByte(Controls_t *controls, uint8_t byte)
{
  uint8_t button_index;

  if (controls == 0)
  {
    return;
  }

  if (byte == (uint8_t)'b')
  {
    controls->waiting_remote_button = 1U;
    return;
  }

  if (controls->waiting_remote_button != 0U &&
      byte >= (uint8_t)'1' &&
      byte <= (uint8_t)'6')
  {
    button_index = (uint8_t)(byte - (uint8_t)'1');
    controls->pressed_actions |=
        controls->config.remote_button_actions[button_index];
  }

  if (byte != (uint8_t)'\r' && byte != (uint8_t)'\n')
  {
    controls->waiting_remote_button = 0U;
  }
}

Controls_ActionMask Controls_GetHeld(const Controls_t *controls)
{
  if (controls == 0)
  {
    return CONTROLS_ACTION_NONE;
  }

  return controls->local_held_actions | controls->remote_held_actions;
}

Controls_ActionMask Controls_TakePressed(Controls_t *controls)
{
  Controls_ActionMask actions;

  if (controls == 0)
  {
    return CONTROLS_ACTION_NONE;
  }

  actions = controls->pressed_actions;
  controls->pressed_actions = CONTROLS_ACTION_NONE;
  return actions;
}

void Controls_Reset(Controls_t *controls)
{
  if (controls == 0)
  {
    return;
  }

  controls->vertical_sample = 0U;
  controls->horizontal_sample = 0U;
  controls->local_button_actions = CONTROLS_ACTION_NONE;
  controls->local_held_actions = CONTROLS_ACTION_NONE;
  controls->remote_held_actions = CONTROLS_ACTION_NONE;
  controls->pressed_actions = CONTROLS_ACTION_NONE;
  controls->waiting_remote_button = 0U;
}
