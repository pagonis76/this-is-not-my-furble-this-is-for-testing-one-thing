#include "../src/game.c"
void plat_sfx(int id) { (void)id; }
static int climb(int level, int sx, int sy, int top_row, int maxf) {
  load_level(level); respawn(); state = ST_PLAY; lives = 3;
  px = sx * T + 3; py = sy * T + T - PH; pvx = pvy = 0;
  Input in = {0}; int dir = 1, best = 9999; int prevj = 0;
  for (int f = 0; f < maxf; f++) {
    in.left = dir < 0; in.right = dir > 0;
    int want = (on_ground && f % 30 == 0) || (!on_ground && wall_dir == dir && pvy > -1);
    in.jump = want && !prevj ? 1 : (prevj && pvy < 0);
    if (want && !prevj && !on_ground && wall_dir == dir) dir = -dir;
    prevj = in.jump;
    game_update(&in);
    if (py < best) best = (int)py;
    if (state == ST_CLEAR) { printf("level %d reached the goal at frame %d\n", level, f); return 0; }
    if (dead_t) { printf("level %d died at x=%.1f y=%.1f f=%d\n", level, px/16, py/16, f); return -1; }
  }
  printf("level %d best y row %.1f (target %d), now x=%.0f y=%.0f\n", level, best / 16.0, top_row, px / 16, py / 16);
  return best;
}
int main(void) {
  climb(1, 17, 22, 12, 600);
  climb(1, 94, 17, 8, 600);
  climb(2, 98, 12, 4, 600);
  return 0;
}
