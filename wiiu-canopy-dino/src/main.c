// Canopy Dino: Wii U (and desktop SDL2) front end. Opens the window (on the Wii U SDL shows it
// on both the TV and the GamePad), reads the controllers, runs the game at 60 ticks a second
// and plays its sound.
#include <SDL2/SDL.h>
#include <string.h>
#include <math.h>
#include "game.h"

#ifdef __WIIU__
#include <vpad/input.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#endif

static SDL_AudioDeviceID g_audio;
static int g_rate = 48000;

void plat_sfx(int id) {
  if (g_audio) SDL_LockAudioDevice(g_audio);
  synth_trigger(id);
  if (g_audio) SDL_UnlockAudioDevice(g_audio);
}
static void audio_cb(void* u, Uint8* stream, int len) {
  (void)u;
  synth_render((int16_t*)stream, len / 4, g_rate);
}

#ifdef __WIIU__
// GamePad, plus up to four Pro Controllers / Classic Controllers / Wii Remotes.
static void wiiu_read(Input* in) {
  VPADStatus v; VPADReadError err;
  if (VPADRead(VPAD_CHAN_0, &v, 1, &err) == 1 && err == VPAD_READ_SUCCESS) {
    uint32_t h = v.hold;
    if (h & (VPAD_BUTTON_LEFT | VPAD_STICK_L_EMULATION_LEFT)) in->left = 1;
    if (h & (VPAD_BUTTON_RIGHT | VPAD_STICK_L_EMULATION_RIGHT)) in->right = 1;
    if (h & (VPAD_BUTTON_UP | VPAD_STICK_L_EMULATION_UP)) in->up = 1;
    if (h & (VPAD_BUTTON_DOWN | VPAD_STICK_L_EMULATION_DOWN)) in->down = 1;
    if (h & (VPAD_BUTTON_A | VPAD_BUTTON_B)) in->jump = 1;
    if (h & VPAD_BUTTON_PLUS) in->start = 1;
    if (h & (VPAD_BUTTON_X | VPAD_BUTTON_Y)) in->attack = 1;
    if (v.leftStick.x < -0.35f) in->left = 1;
    if (v.leftStick.x > 0.35f) in->right = 1;
    if (v.leftStick.y < -0.5f) in->down = 1;
  }
  for (int ch = 0; ch < 4; ch++) {
    WPADExtensionType ext;
    if (WPADProbe((WPADChan)ch, &ext) != 0) continue;
    KPADStatus k; KPADError kerr;
    memset(&k, 0, sizeof k);
    if (KPADReadEx((KPADChan)ch, &k, 1, &kerr) < 1) continue;
    float sx = 0, sy = 0;
    if (k.extensionType == WPAD_EXT_PRO_CONTROLLER) {
      uint32_t b = k.pro.hold;
      if (b & WPAD_PRO_BUTTON_LEFT) in->left = 1;
      if (b & WPAD_PRO_BUTTON_RIGHT) in->right = 1;
      if (b & WPAD_PRO_BUTTON_UP) in->up = 1;
      if (b & WPAD_PRO_BUTTON_DOWN) in->down = 1;
      if (b & (WPAD_PRO_BUTTON_A | WPAD_PRO_BUTTON_B)) in->jump = 1;
      if (b & WPAD_PRO_BUTTON_PLUS) in->start = 1;
      if (b & (WPAD_PRO_BUTTON_X | WPAD_PRO_BUTTON_Y)) in->attack = 1;
      sx = k.pro.leftStick.x; sy = k.pro.leftStick.y;
    } else if (k.extensionType == WPAD_EXT_CLASSIC || k.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
      uint32_t b = k.classic.hold;
      if (b & WPAD_CLASSIC_BUTTON_LEFT) in->left = 1;
      if (b & WPAD_CLASSIC_BUTTON_RIGHT) in->right = 1;
      if (b & WPAD_CLASSIC_BUTTON_UP) in->up = 1;
      if (b & WPAD_CLASSIC_BUTTON_DOWN) in->down = 1;
      if (b & (WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_B)) in->jump = 1;
      if (b & WPAD_CLASSIC_BUTTON_PLUS) in->start = 1;
      if (b & (WPAD_CLASSIC_BUTTON_X | WPAD_CLASSIC_BUTTON_Y)) in->attack = 1;
      sx = k.classic.leftStick.x; sy = k.classic.leftStick.y;
    } else if (k.extensionType == WPAD_EXT_NUNCHUK || k.extensionType == WPAD_EXT_MPLUS_NUNCHUK) {
      if (k.hold & WPAD_BUTTON_A) in->jump = 1;
      if (k.hold & WPAD_BUTTON_B) in->attack = 1;
      if (k.hold & WPAD_BUTTON_PLUS) in->start = 1;
      sx = k.nunchuk.stick.x; sy = k.nunchuk.stick.y;
    } else {
      // Wii Remote on its own, held sideways (D-pad on the left)
      uint32_t b = k.hold;
      if (b & WPAD_BUTTON_UP) in->left = 1;
      if (b & WPAD_BUTTON_DOWN) in->right = 1;
      if (b & WPAD_BUTTON_RIGHT) in->up = 1;
      if (b & WPAD_BUTTON_LEFT) in->down = 1;
      if (b & WPAD_BUTTON_2) in->jump = 1;
      if (b & WPAD_BUTTON_1) in->attack = 1;
      if (b & WPAD_BUTTON_PLUS) in->start = 1;
    }
    if (sx < -0.35f) in->left = 1;
    if (sx > 0.35f) in->right = 1;
    if (sy < -0.5f) in->down = 1;
  }
}
#endif

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO;
#ifndef __WIIU__
  flags |= SDL_INIT_GAMECONTROLLER;
#endif
  if (SDL_Init(flags) != 0) return 1;
#ifdef __WIIU__
  KPADInit();
  WPADEnableURCC(1);
  WPADEnableWiiRemote(1);
#endif
  SDL_Window* win = SDL_CreateWindow("Canopy Dino", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
  SDL_Renderer* r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(r, 1280, 720);
  SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCR_W, SCR_H);

  game_init();

  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
  g_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (g_audio) { g_rate = have.freq; SDL_PauseAudioDevice(g_audio, 0); }

#ifndef __WIIU__
  SDL_GameController* pad = NULL;
  for (int i = 0; i < SDL_NumJoysticks() && !pad; i++) if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);
#endif

  static uint32_t fb[SCR_W * SCR_H];
  Uint64 freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
  double acc = 0, tick = 1.0 / 60.0;
  int running = 1;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = 0;
#ifndef __WIIU__
      if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
#endif
    }
    Input in;
    memset(&in, 0, sizeof in);
#ifdef __WIIU__
    wiiu_read(&in);
#else
    const Uint8* ks = SDL_GetKeyboardState(NULL);
    in.left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
    in.right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
    in.up = ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W];
    in.down = ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S];
    in.jump = ks[SDL_SCANCODE_SPACE] || ks[SDL_SCANCODE_Z];
    in.start = ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_ESCAPE];
    in.attack = ks[SDL_SCANCODE_X] || ks[SDL_SCANCODE_LSHIFT];
    if (pad) {
      in.left |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) < -12000;
      in.right |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) > 12000;
      in.down |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
      in.jump |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B);
      in.start |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
      in.attack |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X) || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y);
    }
#endif
    Uint64 now = SDL_GetPerformanceCounter();
    acc += (double)(now - last) / freq; last = now;
    if (acc > 0.1) acc = 0.1;
    int steps = 0;
    while (acc >= tick && steps < 4) {
      if (g_audio) SDL_LockAudioDevice(g_audio);   // the music reads game state
      game_update(&in);
      if (g_audio) SDL_UnlockAudioDevice(g_audio);
      acc -= tick; steps++;
    }
    game_draw(fb);
    SDL_UpdateTexture(tex, NULL, fb, SCR_W * 4);
    SDL_RenderClear(r);
    SDL_RenderCopy(r, tex, NULL, NULL);
    SDL_RenderPresent(r);
  }
  if (g_audio) SDL_CloseAudioDevice(g_audio);
  SDL_Quit();
  return 0;
}
