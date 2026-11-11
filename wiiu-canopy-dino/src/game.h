// Canopy Kip: an original little pixel platformer. The game itself is plain C with no
// platform code: it draws into a 320x180 pixel buffer and makes its own sound, so the same
// code runs on the Wii U and in the desktop test harness.
#pragma once
#include <stdint.h>

#define SCR_W 320
#define SCR_H 180

typedef struct {
  int left, right, up, down;
  int jump;   // held
  int start;  // held
  int attack; // held
} Input;

void game_init(void);
void game_update(const Input* in);       // one 60 Hz tick
void game_draw(uint32_t* fb);            // fb is SCR_W*SCR_H, 0xAARRGGBB

// sound: the platform calls synth_render from its audio callback. The game calls plat_sfx,
// which the platform implements (taking its audio lock, then calling synth_trigger).
void synth_render(int16_t* out, int frames, int rate);   // interleaved stereo
void synth_trigger(int id);
void plat_sfx(int id);
