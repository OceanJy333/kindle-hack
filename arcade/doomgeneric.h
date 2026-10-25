// input: libretro button IDs; output: evdev adapter key names; pos: reuse the tested native multi-touch adapter.
#pragma once
#include <stdint.h>
#include "libretro.h"
uint32_t DG_GetTicksMs(void);
#define KEY_UPARROW (RETRO_DEVICE_ID_JOYPAD_UP+1)
#define KEY_DOWNARROW (RETRO_DEVICE_ID_JOYPAD_DOWN+1)
#define KEY_LEFTARROW (RETRO_DEVICE_ID_JOYPAD_LEFT+1)
#define KEY_RIGHTARROW (RETRO_DEVICE_ID_JOYPAD_RIGHT+1)
#define KEY_FIRE (RETRO_DEVICE_ID_JOYPAD_B+1)
#define KEY_USE (RETRO_DEVICE_ID_JOYPAD_A+1)
#define KEY_GRENADE (RETRO_DEVICE_ID_JOYPAD_Y+1)
#define KEY_ENTER (RETRO_DEVICE_ID_JOYPAD_START+1)
#define KEY_ESCAPE (RETRO_DEVICE_ID_JOYPAD_SELECT+1)
