// Records a gameplay video: an autopilot plays the title, the map and level 1.
// Writes raw RGB frames to stdout-ish files: out/video.rgb (320x180, 60 fps) and out/video.pcm (s16 stereo 48k).
#include "../src/game.c"
#include <stdlib.h>
void plat_sfx(int id) { synth_trigger(id); }
static uint32_t fb[SCR_W * SCR_H];
static FILE *fv, *fa;
static void tick(Input* in) {
  game_update(in);
  game_draw(fb);
  static unsigned char rgb[SCR_W * SCR_H * 3];
  for (int i = 0; i < SCR_W * SCR_H; i++) { rgb[i * 3] = fb[i] >> 16; rgb[i * 3 + 1] = fb[i] >> 8; rgb[i * 3 + 2] = fb[i]; }
  fwrite(rgb, 1, sizeof rgb, fv);
  static int16_t snd[800 * 2];
  synth_render(snd, 800, 48000);
  fwrite(snd, 2, 1600, fa);
}
static int floor_at(int tx, int ty) { char c = tile(tx, ty); return solid_c(c) || c == '=' || c == 's'; }
int main(int argc, char** argv) {
  int level = argc > 1 ? atoi(argv[1]) : 1;
  fv = fopen("out/video.rgb", "wb"); fa = fopen("out/video.pcm", "wb");
  game_init();
  for (int i = 1; i < level; i++) cleared[i] = 1;
  Input in = {0};
  for (int i = 0; i < 90; i++) tick(&in);
  in.jump = 1; tick(&in); in.jump = 0;
  for (int i = 0; i < 40; i++) tick(&in);
  // walk on the map to the level
  for (int n = 0; n < level; n++) { in.right = 1; tick(&in); in.right = 0; for (int i = 0; i < 34; i++) tick(&in); }
  for (int i = 0; i < 30; i++) tick(&in);
  in.jump = 1; tick(&in); in.jump = 0; tick(&in);
  int hold = 0, rest = 0, f = 0;
  while (state == ST_PLAY && f < 60 * 120) {
    f++;
    memset(&in, 0, sizeof in);
    in.right = 1;
    for (int k = 0; k < nents; k++) {   // go back for a power-up that popped out behind
      Ent* e = &ents[k];
      if (e->alive && e->type == E_POW && e->x + 12 < px && px - e->x < 90 && fabsf(e->y - py) < 40) { in.right = 0; in.left = 1; }
    }
    if (!dead_t) {
      float fx = px + PW / 2;
      int dirx = in.left ? -1 : 1;
      int txf = dirx > 0 ? (int)floorf((px + PW + 4) / T) : (int)floorf((px - 4) / T), txf2 = txf + dirx;
      int tyb = (int)floorf((py + PH / 2) / T), tyf = (int)floorf((py + PH + 2) / T);
      int want = 0;
      if (on_ground) {
        if (!floor_at(txf, tyf) && !floor_at(txf, tyf + 1)) want = 1;              // pit
        if (solid_c(tile(txf, tyb)) || solid_c(tile(txf, tyb - 1))) want = 1;        // step / wall
        if (tile(txf, tyb) == '^' || tile(txf2, tyb) == '^' || tile(txf, tyf) == '^' || tile(txf2, tyf) == '^') want = 1;
        for (int r = 1; r <= 4; r++) for (int c = (int)(fx / T) - 3; c <= (int)(fx / T) + 3; c++) if (tile(c, tyb - r) == '?') { float d = (c * T + 8 - fx) * dirx; if (d > 14 && d < 30) want = 1; }
        for (int k = 0; k < nents; k++) {
          Ent* e = &ents[k];
          if (!e->alive || e->dying || e->type == E_POW) continue;
          float d = e->x - (px + PW);
          if (d > -4 && d < (power == 2 ? 22 : 34) && fabsf(e->y - py) < 22 && !atk_cd) in.attack = 1;
        }
      } else {
        // double jump over a pit when falling with nothing below
        int below = 0;
        for (int r = 0; r < 6; r++) if (floor_at((int)floorf(fx / T), tyf + r) || floor_at(txf, tyf + r)) below = 1;
        if (!below && pvy > 0 && power == 1 && air_jumps) want = 2;
        for (int k = 0; k < nents; k++) {
          Ent* e = &ents[k];
          if (!e->alive || e->dying || e->type == E_POW) continue;
          float d = e->x - (px + PW);
          if (d > -4 && d < 26 && fabsf(e->y - py) < 18 && !atk_cd && pvy < 1) in.attack = 1;
        }
      }
      if (want && !hold && !rest) hold = want == 2 ? 14 : 20;
    }
    if (hold) { in.jump = 1; if (--hold == 0) rest = 3; } else if (rest) rest--;
    tick(&in);
  }
  for (int y = 0; y < LH; y++) for (int x = 0; x < LW; x++) if (G[y][x] == '?') printf("unopened box at %d,%d\n", x, y);
  printf("level %d: state=%d after %d frames, deaths=%d x=%.0f/%d power=%d\n", level, state, f, 3 - lives, px / 16, LW, power);
  for (int i = 0; i < 160; i++) { memset(&in, 0, sizeof in); tick(&in); }
  fclose(fv); fclose(fa);
  return 0;
}
