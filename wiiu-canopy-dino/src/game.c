// Canopy Dino: game logic, drawing and sound. See game.h.
#include "game.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define T 16                 // tile size in pixels
#define MAXW 160
#define MAXH 40

// ============================================================ sprites
// Each sprite is 16 rows of 16 characters. '.' is see-through; other letters are palette colours.
static uint32_t pal(char c) {
  switch (c) {
    case 'k': return 0xFF2B1A12;  // outline
    case 'b': return 0xFF8B5A2B;  // fur
    case 'd': return 0xFF6B3F1D;  // dark fur
    case 't': return 0xFFF2C99A;  // face / belly
    case 'p': return 0xFFFF9EB5;  // pink
    case 'w': return 0xFFFFFFFF;
    case 'e': return 0xFF1A1A1A;
    case 'r': return 0xFFD8463C;
    case 'y': return 0xFFF7D038;
    case 'Y': return 0xFFC9A227;
    case 'g': return 0xFF3FA34D;
    case 'G': return 0xFF24723A;
    case 'h': return 0xFF9BE37A;
    case 'z': return 0xFF222222;
    case 'l': return 0xFFCFE8FF;
    case 's': return 0xFFB0B4C0;
    default: return 0;
  }
}

static const char* SPR_BEETLE[2][16] = { {
  "................", "................", "................", "................",
  "................", "................", "................",
  ".....kkkkkk.....",
  "...kkghhgggkk...",
  "..kgghhgggggGk..",
  ".kggggggggggGGk.",
  ".kGgggggggggGGk.",
  "kwekGGGGGGGGGGk.",
  "kkkkkkkkkkkkkkk.",
  "..k.k...k.k..k..",
  ".k...k.k...k.k..",
}, {
  "................", "................", "................", "................",
  "................", "................", "................",
  ".....kkkkkk.....",
  "...kkghhgggkk...",
  "..kgghhgggggGk..",
  ".kggggggggggGGk.",
  ".kGgggggggggGGk.",
  "kwekGGGGGGGGGGk.",
  "kkkkkkkkkkkkkkk.",
  "..k.k..k.k..k...",
  "...k.k...k.k.k..",
} };
static const char* SPR_BEE[2][16] = { {
  "................", "................", "................", "................",
  "................",
  "......ll.ll.....",
  ".....lwllwl.....",
  "...kkkkkkkkk....",
  "..kyyzyyzyyyk...",
  ".kweyzyyzyyyyk..",
  ".kkyyzyyzyyyyk..",
  "..kyyzyyzyyykk..",
  "...kkkkkkkkk.kk.",
  "................", "................", "................",
}, {
  "................", "................", "................", "................",
  "................",
  "................",
  "....llwllwll....",
  "...kkkkkkkkk....",
  "..kyyzyyzyyyk...",
  ".kweyzyyzyyyyk..",
  ".kkyyzyyzyyyyk..",
  "..kyyzyyzyyykk..",
  "...kkkkkkkkk.kk.",
  "................", "................", "................",
} };
static const char* SPR_WING[16] = {
  "................", "................", "................",
  "..........kk....",
  "........kkwwk...",
  "......kkwwwwk...",
  "....kkwwwwwwk...",
  "...kwwwwwwwwk...",
  "..kwwwwwwwwk....",
  "..kwwlwwlwk.....",
  "...kwlwwlk......",
  "....kwlwk.......",
  ".....kkk........",
  "................", "................", "................",
};
static const char* SPR_SWORD[16] = {
  "................",
  ".............kk.",
  "............kwsk",
  "...........kwsk.",
  "..........kwsk..",
  ".........kwsk...",
  "........kwsk....",
  ".......kwsk.....",
  "..kk..kwsk......",
  "..kykkwsk.......",
  "...kyysk........",
  "....kyyk........",
  "...kkkyyk.......",
  "..krrk.kyk......",
  "..krk...k.......",
  "...k............",
};
static const char* SPR_BANANA[16] = {
  "................", "................", "................",
  "..........kk....",
  ".........kYk....",
  "........kyyk....",
  ".......kyyyk....",
  "......kyyyk.....",
  "....kkyyyyk.....",
  "..kkyyyyyk......",
  ".kYyyyyykk......",
  "..kkkkkk........",
  "................", "................", "................", "................",
};

#include "dino_sprites.h"

static uint32_t* FB;
static void put(int x, int y, uint32_t c) {
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) FB[y * SCR_W + x] = c;
}
static void fill(int x, int y, int w, int h, uint32_t c) {
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) put(i, j, c);
}
static uint32_t blend(uint32_t a, uint32_t b, float t) {
  int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
  int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
  return 0xFF000000 | ((int)(ar + (br - ar) * t) << 16) | ((int)(ag + (bg - ag) * t) << 8) | (int)(ab + (bb - ab) * t);
}
static void darken(int x, int y, int w, int h, float t) {
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++)
    if ((unsigned)i < SCR_W && (unsigned)j < SCR_H) FB[j * SCR_W + i] = blend(FB[j * SCR_W + i], 0xFF000000, t);
}
static void sprite(const char* const* s, int x, int y, int flip) {
  for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) {
    char c = s[j][flip ? 15 - i : i];
    if (c != '.') put(x + i, y + j, pal(c));
  }
}
static void sprite_n(const char* const* s, int n, int x, int y, int flip) {
  for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
    char c = s[j][flip ? n - 1 - i : i];
    if (c != '.') put(x + i, y + j, pal(c));
  }
}
static void sprite_flat(const char* const* s, int x, int y, int flip, uint32_t col) {
  for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++)
    if (s[j][flip ? 15 - i : i] != '.') put(x + i, y + j, col);
}
static uint32_t hash2(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// ============================================================ font (3x5)
static const struct { char c; const char* r; } FONT[] = {
  {'A', ".X.X.XXXXX.XX.X"}, {'B', "XX.X.XXX.X.XXX."}, {'C', ".XXX..X..X...XX"}, {'D', "XX.X.XX.XX.XXX."},
  {'E', "XXXX..XX.X..XXX"}, {'F', "XXXX..XX.X..X.."}, {'G', ".XXX..X.XX.X.XX"}, {'H', "X.XX.XXXXX.XX.X"},
  {'I', "XXX.X..X..X.XXX"}, {'J', "..X..X..XX.X.X."}, {'K', "X.XX.XXX.X.XX.X"}, {'L', "X..X..X..X..XXX"},
  {'M', "X.XXXXXXXX.XX.X"}, {'N', "XX.X.XX.XX.XX.X"}, {'O', ".X.X.XX.XX.X.X."}, {'P', "XX.X.XXX.X..X.."},
  {'Q', ".X.X.XX.XXX..XX"}, {'R', "XX.X.XXX.X.XX.X"}, {'S', ".XXX...X...XXX."}, {'T', "XXX.X..X..X..X."},
  {'U', "X.XX.XX.XX.XXXX"}, {'V', "X.XX.XX.XX.X.X."}, {'W', "X.XX.XXXXXXXX.X"}, {'X', "X.XX.X.X.X.XX.X"},
  {'Y', "X.XX.X.X..X..X."}, {'Z', "XXX..X.X.X..XXX"},
  {'0', "XXXX.XX.XX.XXXX"}, {'1', ".X.XX..X..X.XXX"}, {'2', "XX...X.X.X..XXX"}, {'3', "XX...X.X...XXX."},
  {'4', "X.XX.XXXX..X..X"}, {'5', "XXXX..XX...XXX."}, {'6', ".XXX..XXXX.XXXX"}, {'7', "XXX..X.X..X..X."},
  {'8', "XXXX.XXXXX.XXXX"}, {'9', "XXXX.XXXX..XXX."},
  {':', "....X.....X...."}, {'!', ".X..X..X.....X."}, {'-', "......XXX......"}, {'.', ".............X."},
  {'x', "...X.X.X.X.X..."}, {'/', "..X..X.X.X..X.."}, {'\'', ".X..X.........."}, {'+', "....X.XXX.X...."},
  {',', "..........X.X.."}, {'?', "XX...X.X.....X."},
};
static const char* glyph(char c) {
  if (c >= 'a' && c <= 'z' && c != 'x') c -= 32;
  for (unsigned i = 0; i < sizeof FONT / sizeof FONT[0]; i++) if (FONT[i].c == c) return FONT[i].r;
  return NULL;
}
static int text_w(const char* s, int sc) { return (int)strlen(s) * 4 * sc - sc; }
static void text_raw(const char* s, int x, int y, int sc, uint32_t col) {
  for (; *s; s++, x += 4 * sc) {
    const char* g = glyph(*s);
    if (!g) continue;
    for (int j = 0; j < 5; j++) for (int i = 0; i < 3; i++)
      if (g[j * 3 + i] == 'X') fill(x + i * sc, y + j * sc, sc, sc, col);
  }
}
static void text(const char* s, int x, int y, int sc, uint32_t col) {
  int o = sc > 1 ? sc / 2 + (sc > 3) : 1;
  text_raw(s, x, y + o, sc, 0xFF1B1020);
  if (sc == 1) text_raw(s, x + 1, y, sc, 0xFF1B1020);
  text_raw(s, x, y, sc, col);
}
static void text_c(const char* s, int y, int sc, uint32_t col) { text(s, (SCR_W - text_w(s, sc)) / 2, y, sc, col); }

// ============================================================ levels
typedef struct { char op; short x, y, w, h; } Op;
typedef struct {
  const char* name;
  int w, h;
  uint32_t sky0, sky1, far, nearc;
  int theme;   // 0 jungle, 1 temple, 2 cave, 3 sky, 4 waterfalls
  const Op* ops;
  const char* signs[4];
} Level;

// ops: g ground (dirt to the bottom)  G ground (stone to the bottom)  x stone block w*h
//      = one-way plank  ^ spikes  o bananas  b beetle  z bee  s spring  c checkpoint
//      F goal flag  P start  i sign (w = which text)
static const Op L1[] = {
  {'B', 12, 7, 0}, {'B', 25, 7, 1}, {'B', 44, 5, 0}, {'B', 59, 7, 0}, {'B', 88, 7, 2}, {'B', 113, 7, 0},
  {'g', 0, 11, 30}, {'P', 3, 10}, {'i', 6, 10, 0}, {'o', 9, 8, 3}, {'b', 20, 10}, {'i', 16, 10, 1},
  {'o', 29, 8, 5},
  {'g', 33, 11, 18}, {'=', 36, 8, 4}, {'o', 36, 7, 4}, {'g', 42, 9, 4}, {'o', 42, 7, 4}, {'b', 48, 10},
  {'g', 51, 12, 3}, {'^', 51, 11, 3},
  {'g', 54, 11, 26}, {'c', 57, 10}, {'x', 62, 9, 2, 2}, {'b', 67, 10}, {'=', 69, 8, 5}, {'o', 69, 7, 5},
  {'s', 77, 10}, {'i', 74, 10, 2}, {'o', 80, 4, 4},
  {'g', 84, 11, 20}, {'b', 89, 10}, {'=', 92, 8, 4}, {'o', 92, 7, 4}, {'z', 97, 6},
  {'g', 104, 9, 6}, {'o', 105, 7, 4},
  {'g', 110, 11, 30}, {'b', 116, 10}, {'o', 120, 8, 6}, {'F', 132, 10},
  {0}
};
static const Op L2[] = {
  {'B', 4, 19, 0}, {'B', 9, 19, 2}, {'B', 33, 6, 0}, {'B', 83, 14, 1},
  {'g', 0, 23, 22}, {'P', 2, 22}, {'i', 6, 22, 0},
  {'x', 15, 12, 1, 9}, {'x', 20, 12, 2, 11},
  {'o', 18, 20, 1}, {'o', 17, 17, 1}, {'o', 18, 14, 1},
  {'=', 24, 12, 5}, {'b', 26, 11},
  {'=', 31, 10, 4}, {'o', 31, 9, 4},
  {'=', 37, 12, 6}, {'z', 40, 9},
  {'=', 45, 14, 3},
  {'x', 49, 13, 3, 1}, {'s', 50, 12}, {'o', 49, 5, 3},
  {'=', 54, 8, 6}, {'b', 57, 7},
  {'=', 62, 10, 5}, {'c', 64, 9},
  {'=', 69, 12, 5}, {'z', 71, 9},
  {'x', 76, 13, 4, 1}, {'o', 76, 12, 4},
  {'g', 80, 18, 30}, {'b', 85, 17}, {'^', 88, 17, 2},
  {'x', 92, 6, 1, 10}, {'x', 96, 8, 4, 10}, {'o', 94, 14, 1}, {'o', 94, 11, 1}, {'o', 94, 8, 1},
  {'i', 90, 17, 1},
  {'F', 98, 7},
  {0}
};
static const Op L3[] = {
  {'B', 6, 9, 2}, {'B', 35, 9, 0}, {'B', 62, 9, 1}, {'B', 108, 9, 0},
  {'G', 0, 13, 16}, {'P', 2, 12}, {'b', 10, 12},
  {'G', 18, 11, 5}, {'o', 18, 9, 5},
  {'G', 23, 13, 10}, {'^', 25, 12, 3}, {'z', 28, 9},
  {'G', 33, 13, 14}, {'x', 38, 10, 2, 3}, {'=', 41, 9, 4}, {'o', 41, 8, 4}, {'b', 44, 12},
  {'G', 50, 13, 3}, {'s', 51, 12}, {'o', 53, 5, 5},
  {'G', 58, 13, 20}, {'c', 60, 12}, {'x', 66, 0, 10, 10}, {'b', 71, 12}, {'o', 67, 12, 8},
  {'G', 80, 12, 3},
  {'G', 83, 13, 12}, {'^', 85, 12, 2}, {'^', 90, 12, 2}, {'=', 84, 9, 3}, {'=', 89, 8, 3}, {'o', 89, 7, 3}, {'z', 87, 6},
  {'G', 95, 13, 25}, {'x', 96, 2, 1, 8}, {'x', 100, 4, 2, 9}, {'o', 98, 9, 1}, {'o', 98, 6, 1},
  {'b', 106, 12}, {'b', 112, 12}, {'z', 109, 8}, {'=', 114, 9, 4}, {'o', 114, 8, 4},
  {'G', 123, 11, 4}, {'o', 123, 9, 4},
  {'G', 129, 13, 21}, {'b', 134, 12}, {'F', 142, 12},
  {0}
};
static const Op L4[] = {   // Misty Falls: climb up and up
  {'B', 5, 29, 1}, {'B', 34, 19, 0},
  {'g', 0, 33, 40}, {'P', 3, 32}, {'i', 6, 32, 0},
  {'=', 8, 31, 4}, {'=', 14, 29, 4}, {'o', 14, 28, 4}, {'=', 20, 27, 4},
  {'=', 26, 25, 4}, {'b', 27, 24}, {'=', 32, 23, 5}, {'o', 32, 22, 5},
  {'=', 26, 21, 4}, {'z', 23, 18}, {'=', 20, 19, 4}, {'c', 21, 18},
  {'=', 13, 17, 4}, {'x', 7, 16, 3, 1}, {'s', 8, 15}, {'o', 8, 12, 1}, {'o', 8, 9, 1},
  {'=', 2, 10, 5}, {'o', 2, 9, 5}, {'=', 9, 8, 4}, {'b', 10, 7}, {'=', 15, 6, 4},
  {'=', 21, 4, 4}, {'o', 21, 3, 4}, {'x', 27, 4, 13, 1}, {'z', 31, 1}, {'F', 36, 3},
  {0}
};
static const Op L5[] = {   // Beetle Cavern: lots of beetles under a low roof
  {'B', 5, 9, 2}, {'B', 36, 9, 0}, {'B', 88, 9, 1}, {'B', 115, 9, 0},
  {'x', 0, 0, 130, 2},
  {'G', 0, 13, 20}, {'P', 2, 12}, {'b', 8, 12}, {'b', 14, 12},
  {'x', 20, 2, 4, 6}, {'G', 20, 13, 10}, {'^', 24, 12, 2}, {'b', 27, 12},
  {'G', 33, 13, 6}, {'z', 35, 9}, {'o', 30, 10, 3},
  {'G', 39, 11, 4}, {'o', 39, 9, 4},
  {'G', 43, 13, 15}, {'=', 45, 10, 4}, {'o', 45, 9, 4}, {'b', 50, 12}, {'b', 54, 12}, {'x', 47, 2, 6, 5},
  {'c', 56, 12},
  {'G', 61, 13, 4}, {'s', 62, 12}, {'o', 65, 6, 5},
  {'G', 70, 13, 20}, {'^', 74, 12, 3}, {'^', 80, 12, 3}, {'=', 73, 9, 5}, {'=', 79, 9, 5}, {'b', 75, 8}, {'z', 84, 8}, {'o', 78, 8, 1},
  {'G', 90, 10, 4}, {'o', 90, 8, 4},
  {'G', 94, 13, 15}, {'b', 97, 12}, {'b', 101, 12}, {'b', 105, 12}, {'=', 97, 9, 10}, {'o', 98, 8, 8},
  {'G', 112, 13, 18}, {'b', 116, 12}, {'F', 124, 12},
  {0}
};
static const Op L6[] = {   // Sky Bridge: planks and floating islands high above the clouds
  {'B', 4, 6, 1}, {'B', 30, 6, 0}, {'B', 52, 6, 2}, {'B', 108, 6, 0}, {'B', 140, 6, 0},
  {'x', 0, 10, 8, 2}, {'P', 2, 9},
  {'=', 10, 10, 4}, {'=', 16, 9, 4}, {'z', 18, 6}, {'=', 22, 10, 3},
  {'x', 27, 10, 5, 2}, {'b', 29, 9}, {'o', 27, 8, 5},
  {'=', 34, 8, 3}, {'=', 39, 6, 3}, {'o', 39, 5, 3}, {'=', 44, 8, 3}, {'z', 42, 3},
  {'x', 49, 10, 6, 2}, {'c', 51, 9},
  {'x', 57, 11, 2, 1}, {'s', 58, 10}, {'=', 64, 4, 5}, {'o', 64, 3, 5},
  {'=', 71, 6, 4}, {'b', 72, 5}, {'=', 77, 8, 4}, {'z', 80, 5},
  {'x', 84, 10, 4, 2}, {'=', 90, 9, 3}, {'=', 95, 7, 3}, {'o', 95, 6, 3}, {'=', 100, 9, 3}, {'z', 98, 4},
  {'x', 105, 10, 8, 2}, {'b', 107, 9}, {'b', 110, 9},
  {'=', 115, 8, 4}, {'=', 121, 10, 4}, {'z', 123, 7}, {'=', 127, 8, 4}, {'o', 127, 7, 4},
  {'x', 133, 10, 17, 2}, {'b', 137, 9}, {'F', 144, 9},
  {0}
};
static const Level LEVELS[] = {
  { "1  JUNGLE FLOOR", 140, 14, 0xFF7EC8F0, 0xFFD4F1F9, 0xFF6FB07E, 0xFF3F8452, 0, L1,
    { "A: JUMP. HOLD IT TO JUMP HIGHER", "X OR Y: HEADBUTT. JUMP UNDER BOXES TO OPEN THEM", "SPRINGS BOUNCE YOU UP HIGH" } },
  { "2  CANOPY CLIMB", 110, 26, 0xFFF7A35C, 0xFFFDE2B0, 0xFFC97B5A, 0xFF7A4B3A, 0, L2,
    { "JUMP AT A WALL, THEN JUMP AGAIN TO WALL-JUMP", "CLIMB THE TOWER TO REACH THE FLAG" } },
  { "3  MISTY FALLS", 40, 36, 0xFF5FB4C9, 0xFFC8EEF0, 0xFF5E9C8C, 0xFF2F6B5E, 4, L4,
    { "THE FLAG IS AT THE VERY TOP. KEEP CLIMBING!" } },
  { "4  OLD TEMPLE", 150, 16, 0xFF3B2F6B, 0xFF9C6FA8, 0xFF5A4A86, 0xFF3A2F5C, 1, L3, { 0 } },
  { "5  BEETLE CAVERN", 130, 16, 0xFF1E1A24, 0xFF4A3B3A, 0xFF3A302E, 0xFF2A2220, 2, L5, { 0 } },
  { "6  SKY BRIDGE", 150, 16, 0xFF4A90E2, 0xFFBFE3FF, 0xFFFFFFFF, 0xFFE3F1FF, 3, L6, { 0 } },
};
#define NLEVELS (int)(sizeof LEVELS / sizeof LEVELS[0])

// ============================================================ state
enum { ST_TITLE, ST_PLAY, ST_PAUSE, ST_CLEAR, ST_OVER, ST_WIN, ST_MAP };
enum { E_BEETLE, E_BEE, E_POW };   // E_POW: a power-up (e->vx holds which: 1 double jump, 2 sword)
enum { SFX_JUMP, SFX_BANANA, SFX_STOMP, SFX_HURT, SFX_SPRING, SFX_CHECK, SFX_CLEAR, SFX_LIFE, SFX_SELECT, SFX_WALL, SFX_DASH, SFX_SWORD, SFX_JUMP2 };

typedef struct { int type, alive, dying, t, k; float x, y, vx, vy, by; } Ent;
typedef struct { float x, y, vx, vy; int life; uint32_t c; } Part;

static char G[MAXH][MAXW];
static char BOXK[MAXH][MAXW];   // what each ? box holds: 0 bananas, 1 double jump, 2 sword
static int LW, LH, lvl;
static const Level* L;
static Ent ents[64]; static int nents;
static Part parts[160];
static int sign_x[4], sign_y[4], nsigns;
static int goal_x, goal_y;

static int state, frame, lives, bananas, total_bananas, state_t;
static float px, py, pvx, pvy, camx, camy;
static int spring_up, power, inv_t, atk_t, atk_cd, air_jumps, prev_atk;
static int face, on_ground, coyote, jbuf, wall_lock, wall_dir, dead_t, run_t, land_t;
static int start_x, start_y, check_x, check_y, prev_jump, prev_start, prev_down, prev_up, prev_left, prev_right;
static int ncheck;
#define PW 10
#define PH 13

static int lvl_frames; int g_no_hud;
// world map: node 0 is Dino's house, node n is level n
static int cleared[8], map_node, map_from, map_t;
static const short MAP_XY[7][2] = { {38, 146}, {84, 134}, {130, 108}, {96, 62}, {166, 50}, {226, 92}, {282, 44} };
#ifndef SAVE_PATH
#define SAVE_PATH "canopydino-save.txt"
#endif
#ifdef __WIIU__
#include <sys/stat.h>
#endif
static void save_progress(void) {
#ifdef __WIIU__
  mkdir("fs:/vol/external01/wiiu/apps/CanopyDino", 0777);
#endif
  FILE* f = fopen(SAVE_PATH, "w");
  if (!f) return;
  for (int i = 1; i <= NLEVELS; i++) fputc(cleared[i] ? '1' : '0', f);
  fprintf(f, "\n%d\n", total_bananas);
  fclose(f);
}
static void load_progress(void) {
  FILE* f = fopen(SAVE_PATH, "r");
  if (!f) return;
  char b[32] = { 0 };
  if (fgets(b, sizeof b, f)) for (int i = 1; i <= NLEVELS && b[i - 1]; i++) cleared[i] = b[i - 1] == '1';
  if (fscanf(f, "%d", &total_bananas) != 1) total_bananas = 0;
  fclose(f);
}
static int node_open(int n) { return n == 0 || n == 1 || (n <= NLEVELS && cleared[n - 1]); }


static char tile(int tx, int ty) {
  if (tx < 0 || tx >= LW) return 'X';
  if (ty < 0 || ty >= LH) return ' ';
  return G[ty][tx];
}
static int solid_c(char c) { return c == '#' || c == 'X' || c == '?' || c == 'u'; }
static int box_solid(float x, float y, float w, float h) {
  int x0 = (int)floorf(x / T), x1 = (int)floorf((x + w - 0.01f) / T);
  int y0 = (int)floorf(y / T), y1 = (int)floorf((y + h - 0.01f) / T);
  for (int ty = y0; ty <= y1; ty++) for (int tx = x0; tx <= x1; tx++) if (solid_c(tile(tx, ty))) return 1;
  return 0;
}

static void spawn_parts(float x, float y, int n, uint32_t c, float spd, float up) {
  for (int k = 0; k < n; k++) {
    for (int i = 0; i < 160; i++) if (parts[i].life <= 0) {
      uint32_t h = hash2(frame * 7 + k, i);
      float a = (h % 628) / 100.0f, s = spd * (0.4f + (h >> 10) % 100 / 160.0f);
      parts[i] = (Part){ x, y, cosf(a) * s, sinf(a) * s - up, 18 + (int)(h >> 20) % 14, c };
      break;
    }
  }
}

static void load_level(int n) {
  lvl = n; L = &LEVELS[n]; LW = L->w; LH = L->h;
  memset(G, ' ', sizeof G); memset(BOXK, 0, sizeof BOXK);
  nents = 0; nsigns = 0; ncheck = 0;
  memset(parts, 0, sizeof parts);
  for (const Op* o = L->ops; o->op; o++) {
    int w = o->w ? o->w : 1, h = o->h ? o->h : 1;
    switch (o->op) {
      case 'g': case 'G':
        for (int x = o->x; x < o->x + w; x++) for (int y = o->y; y < LH; y++) G[y][x] = o->op == 'g' ? '#' : 'X';
        break;
      case 'x': for (int y = o->y; y < o->y + h; y++) for (int x = o->x; x < o->x + w; x++) G[y][x] = 'X'; break;
      case '=': case '^': case 'o':
        for (int x = o->x; x < o->x + w; x++) G[o->y][x] = o->op;
        break;
      case 's': G[o->y][o->x] = 's'; break;
      case 'B': G[o->y][o->x] = '?'; BOXK[o->y][o->x] = (char)o->w; break;
      case 'c': G[o->y][o->x] = 'c'; break;
      case 'F': goal_x = o->x; goal_y = o->y; break;
      case 'P': start_x = o->x * T + 3; start_y = o->y * T + T - PH; break;
      case 'i': if (nsigns < 4) { sign_x[o->w] = o->x; sign_y[o->w] = o->y; nsigns++; } break;
      case 'b': case 'z':
        if (nents < 64) {
          Ent* e = &ents[nents++];
          memset(e, 0, sizeof *e);
          e->type = o->op == 'b' ? E_BEETLE : E_BEE; e->alive = 1;
          e->x = o->x * T + 2; e->y = o->op == 'b' ? o->y * T + 6 : o->y * T; e->by = e->y;
          e->vx = -0.5f; e->t = o->x * 13;
          break;
        }
    }
  }
  for (int i = 0; i < nsigns; i++) if (!L->signs[i]) nsigns = i;
  check_x = start_x; check_y = start_y;
  lvl_frames = 0;
}

static void respawn(void) {
  px = check_x; py = check_y; pvx = pvy = 0; face = 1; dead_t = 0; wall_lock = 0; jbuf = 0; atk_t = 0; inv_t = 0;
  camx = px - SCR_W / 2; camy = py - SCR_H / 2;
  // beetles and bees come back when you respawn
  for (int i = 0; i < nents; i++) {
    Ent* e = &ents[i];
    if (e->type == E_POW) continue;
    if (!e->alive || e->dying) { e->alive = 1; e->dying = 0; e->y = e->by; }
  }
}

static void start_level(int n) {
  load_level(n);
  // reset enemy spawn positions to the level's original ones
  respawn();
  state = ST_PLAY; state_t = 0;
}

void game_init(void) {
  state = ST_TITLE; state_t = 0; frame = 0;
  load_progress();
  load_level(0); respawn();
}

static void hurt_ex(int pit) {
  if (dead_t) return;
  if (!pit && inv_t) return;
  if (!pit && power) {
    // losing the power-up instead of a life
#ifdef TRACE
    printf("lost power at x=%.0f\n", px / 16);
#endif
    power = 0; inv_t = 100; pvy = -3.5f; pvx = -face * 2.0f; wall_lock = 10;
    plat_sfx(SFX_HURT);
    spawn_parts(px + PW / 2, py + 4, 12, 0xFFFFFFFF, 1.8f, 0.4f);
    return;
  }
  dead_t = 1; pvy = -5.0f; pvx = 0; power = 0;
  plat_sfx(SFX_HURT);
}
static void hurt(void) { hurt_ex(0); }

static void open_box(int tx, int ty) {
  if (tile(tx, ty) != '?') return;
  G[ty][tx] = 'u';
  int k = BOXK[ty][tx];
  if (k == 0) {
    for (int i = 0; i < 5; i++) { bananas++; total_bananas++; }
    if (bananas >= 50) { bananas -= 50; lives++; plat_sfx(SFX_LIFE); } else plat_sfx(SFX_BANANA);
    spawn_parts(tx * T + 8, ty * T - 2, 14, 0xFFF7D038, 2.0f, 1.2f);
  } else if (nents < 64) {
    Ent* e = &ents[nents++];
    memset(e, 0, sizeof *e);
    e->type = E_POW; e->alive = 1; e->k = k; e->vx = 0.8f;
    e->x = tx * T + 2; e->by = (ty - 1) * T + 2; e->y = ty * T; e->t = 0;
    plat_sfx(SFX_CHECK);
  }
  spawn_parts(tx * T + 8, ty * T, 6, 0xFFD69A55, 1.2f, 0.8f);
}

// the area Dino's attack hits right now (headbutt, or the sword's longer reach)
static int attack_box(float* ax, float* ay, float* aw, float* ah) {
  if (atk_t <= 0 || dead_t) return 0;
  if (power == 2) {
    if (atk_t < 4) return 0;
    *aw = 22; *ah = PH + 8; *ay = py - 6;
    *ax = face > 0 ? px + PW - 2 : px - 20;
  } else {
    if (atk_t < 6) return 0;
    *aw = 8; *ah = PH - 2; *ay = py + 1;
    *ax = face > 0 ? px + PW - 2 : px - 6;
  }
  return 1;
}

static void collect(int tx, int ty) {
  G[ty][tx] = ' ';
  bananas++; total_bananas++;
  spawn_parts(tx * T + 8, ty * T + 8, 6, 0xFFF7D038, 1.5f, 0.5f);
  if (bananas >= 50) { bananas -= 50; lives++; plat_sfx(SFX_LIFE); }
  else plat_sfx(SFX_BANANA);
}

// ============================================================ player
static void update_player(const Input* in) {
  int jp = in->jump && !prev_jump;
  if (dead_t) {
    dead_t++;
    pvy += 0.3f; py += pvy;
    if (dead_t > 80) {
      lives--;
      if (lives < 0) { state = ST_OVER; state_t = 0; lives = 3; return; }
      respawn();
    }
    return;
  }

  int dir = (in->right ? 1 : 0) - (in->left ? 1 : 0);
  if (wall_lock > 0) { wall_lock--; dir = 0; }
  if (inv_t > 0) inv_t--;
  if (atk_cd > 0) atk_cd--;
  int ap = in->attack && !prev_atk;
  prev_atk = in->attack;
  if (ap && atk_cd == 0 && atk_t == 0) {
    if (power == 2) { atk_t = 16; atk_cd = 22; pvx += face * 0.8f; plat_sfx(SFX_SWORD); }
    else { atk_t = 18; atk_cd = 30; pvx = face * 4.2f; if (pvy > 0.5f) pvy = 0.5f; plat_sfx(SFX_DASH); }
  }
  if (atk_t > 0) {
    atk_t--;
    if (power != 2) { dir = 0; if (atk_t > 6) { pvx = face * 4.2f; if (pvy > 0.6f) pvy = 0.6f; if (frame % 2 == 0) spawn_parts(px + (face > 0 ? 0 : PW), py + PH - 3, 1, 0xFFE8E0C8, 0.5f, 0.1f); } }
    float ax, ay, aw, ah;
    if (attack_box(&ax, &ay, &aw, &ah)) {
      int tx0 = (int)floorf(ax / T), tx1 = (int)floorf((ax + aw) / T), ty0 = (int)floorf(ay / T), ty1 = (int)floorf((ay + ah) / T);
      for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) if (tile(tx, ty) == '?') { open_box(tx, ty); if (power != 2) { atk_t = 4; pvx = -face * 1.5f; } }
    }
  }
  float acc = on_ground ? 0.32f : 0.22f, maxv = 2.1f;
  if (atk_t > 6 && power != 2) maxv = 4.2f;
  if (dir) { pvx += dir * acc; face = dir; if (pvx > maxv) pvx = maxv; if (pvx < -maxv) pvx = -maxv; }
  else if (wall_lock <= 0) { float f = on_ground ? 0.3f : 0.08f; if (pvx > f) pvx -= f; else if (pvx < -f) pvx += f; else pvx = 0; }

  if (jp) jbuf = 7; else if (jbuf > 0) jbuf--;
  if (on_ground) { coyote = 6; air_jumps = 1; } else if (coyote > 0) coyote--;

  // walls next to the player
  wall_dir = 0;
  if (!on_ground) {
    if (box_solid(px + PW, py + 2, 1, PH - 4)) wall_dir = 1;
    else if (box_solid(px - 1, py + 2, 1, PH - 4)) wall_dir = -1;
  }

  if (jbuf && coyote) {
    pvy = -5.3f; jbuf = 0; coyote = 0; on_ground = 0;
    plat_sfx(SFX_JUMP);
    spawn_parts(px + PW / 2, py + PH, 4, 0xFFE8E0C8, 0.8f, 0.3f);
  } else if (jbuf && wall_dir) {
    pvy = -5.0f; pvx = -wall_dir * 2.3f; face = -wall_dir; wall_lock = 9; jbuf = 0;
    plat_sfx(SFX_WALL);
    spawn_parts(px + (wall_dir > 0 ? PW : 0), py + PH / 2, 5, 0xFFE8E0C8, 1.0f, 0.2f);
  } else if (jbuf && power == 1 && air_jumps > 0) {
    pvy = -4.9f; jbuf = 0; air_jumps = 0;
    plat_sfx(SFX_JUMP2);
    spawn_parts(px + PW / 2, py + PH, 10, 0xFFFFFFFF, 1.4f, -0.4f);
  }

  if (pvy >= 0) spring_up = 0;
  float g = ((in->jump || spring_up) && pvy < 0) ? 0.28f : 0.55f;
  pvy += g;
  float maxfall = 5.5f;
  if (wall_dir && dir == wall_dir && pvy > 0) { maxfall = 1.1f; if (frame % 6 == 0) spawn_parts(px + (wall_dir > 0 ? PW : 0), py + 4, 1, 0xFFE8E0C8, 0.4f, 0); }
  if (pvy > maxfall) pvy = maxfall;

  // move x
  float nx = px + pvx;
  if (box_solid(nx, py, PW, PH)) {
    if (pvx > 0) nx = floorf((nx + PW) / T) * T - PW;
    else if (pvx < 0) nx = (floorf(nx / T) + 1) * T;
    pvx = 0;
  }
  px = nx;
  // move y
  int was_ground = on_ground;
  on_ground = 0;
  float ny = py + pvy;
  if (pvy > 0) {
    float bot0 = py + PH, bot1 = ny + PH;
    int x0 = (int)floorf(px / T), x1 = (int)floorf((px + PW - 0.01f) / T);
    int ty = (int)floorf((bot1 - 0.01f) / T);
    int hit = 0;
    for (int tx = x0; tx <= x1; tx++) {
      char c = tile(tx, ty);
      if (solid_c(c) || (c == '=' && bot0 <= ty * T + 0.01f)) hit = 1;
    }
    if (hit) { ny = ty * T - PH; pvy = 0; on_ground = 1; }
  } else if (pvy < 0) {
    if (box_solid(px, ny, PW, PH)) {
      int ty = (int)floorf(ny / T);
      int x0 = (int)floorf((px + 1) / T), x1 = (int)floorf((px + PW - 1.01f) / T);
      for (int tx = x0; tx <= x1; tx++) open_box(tx, ty);
      ny = (ty + 1) * T; pvy = 0;
    }
  }
  py = ny;
  if (on_ground && !was_ground) { land_t = 6; spawn_parts(px + PW / 2, py + PH, 3, 0xFFE8E0C8, 0.6f, 0.2f); }
  if (land_t) land_t--;

  // tiles the player touches
  int x0 = (int)floorf(px / T), x1 = (int)floorf((px + PW - 0.01f) / T);
  int y0 = (int)floorf(py / T), y1 = (int)floorf((py + PH - 0.01f) / T);
  for (int ty = y0; ty <= y1; ty++) for (int tx = x0; tx <= x1; tx++) {
    char c = tile(tx, ty);
    if (c == 'o') collect(tx, ty);
    else if (c == '^' && py + PH > ty * T + 9) hurt();
    else if (c == 's' && pvy >= 0 && py + PH > ty * T + 8) {
      pvy = -8.6f; on_ground = 0; coyote = 0; spring_up = 1; plat_sfx(SFX_SPRING);
      spawn_parts(tx * T + 8, ty * T + 10, 6, 0xFFFFFFFF, 1.2f, 0.4f);
    } else if (c == 'c' && (check_x != tx * T + 3 || check_y != ty * T + T - PH)) {
      check_x = tx * T + 3; check_y = ty * T + T - PH;
      plat_sfx(SFX_CHECK);
      spawn_parts(tx * T + 8, ty * T, 10, 0xFF9BE37A, 1.5f, 0.6f);
    }
  }
  // goal: a flag on a pole, touch anywhere on the pole
  if (px + PW > goal_x * T + 5 && px < goal_x * T + 11 && py + PH > goal_y * T - 3 * T && py < goal_y * T + T) {
    state = ST_CLEAR; state_t = 0; plat_sfx(SFX_CLEAR);
    spawn_parts(goal_x * T + 8, goal_y * T - 32, 24, 0xFFF7D038, 2.5f, 0.5f);
  }
  if (py > LH * T + 24) hurt_ex(1);

  if (on_ground && pvx != 0) run_t++; else run_t = 0;
}

static void update_ents(void) {
  for (int i = 0; i < nents; i++) {
    Ent* e = &ents[i];
    if (!e->alive) continue;
    e->t++;
    if (e->dying) { if (++e->dying > 24) e->alive = 0; continue; }
    float ew = 12, eh = 10;
    if (e->type == E_POW) {
      if (e->t <= 16) e->y -= 1.0f;          // pop up out of the box
      else {                                  // then slide off and land
        if (e->t < 70 && !box_solid(e->x + e->vx, e->y, 12, 12)) e->x += e->vx;
        e->vy += 0.3f; if (e->vy > 4) e->vy = 4;
        float ny = e->y + e->vy;
        int ty = (int)floorf((ny + 12) / T);
        int x0 = (int)floorf(e->x / T), x1 = (int)floorf((e->x + 11) / T), land = 0;
        for (int tx = x0; tx <= x1; tx++) { char c = tile(tx, ty); if (solid_c(c) || (c == '=' && e->y + 12 <= ty * T + 0.5f)) land = 1; }
        if (land && e->vy > 0) { e->y = ty * T - 12; e->vy = 0; } else e->y = ny;
        if (e->y > LH * T + 40) e->alive = 0;
      }
      if (!dead_t && state == ST_PLAY && px + PW > e->x && px < e->x + 12 && py + PH > e->y && py < e->y + 12) {
        power = e->k; e->alive = 0; air_jumps = 1;
#ifdef TRACE
        printf("got power %d at x=%.0f\n", power, px / 16);
#endif
        plat_sfx(SFX_LIFE);
        spawn_parts(e->x + 6, e->y + 6, 16, power == 2 ? 0xFFD8DCE6 : 0xFFFFFFFF, 2.0f, 0.5f);
      }
      continue;
    }
    if (e->type == E_BEETLE) {
      // walk, turn at walls and ledges
      float nx = e->x + e->vx;
      int ahead = e->vx < 0 ? (int)floorf(nx / T) : (int)floorf((nx + ew) / T);
      int below = (int)floorf((e->y + eh + 1) / T);
      int wall = box_solid(nx, e->y, ew, eh);
      char fl = tile(ahead, below);
      int ledge = !(solid_c(fl) || fl == '=');
      if (wall || ledge) e->vx = -e->vx; else e->x = nx;
    } else {
      // bees bob up and down and patrol a few tiles each way
      e->vx = ((e->t / 110) % 2) ? 0.6f : -0.6f;
      if (!box_solid(e->x + e->vx, e->y, ew, eh)) e->x += e->vx;
      e->y = e->by + sinf(e->t * 0.07f) * 14;
    }
    // touch the player
    if (!dead_t && state == ST_PLAY) {
      float ex = e->x + 1, ey = e->y + (e->type == E_BEETLE ? 0 : 2);
      float ax, ay, aw, ah;
      if (attack_box(&ax, &ay, &aw, &ah) && ax + aw > ex && ax < ex + ew - 2 && ay + ah > ey && ay < ey + eh) {
        e->dying = 1; plat_sfx(SFX_STOMP);
        spawn_parts(e->x + 6, e->y + 4, 10, e->type == E_BEETLE ? 0xFF3FA34D : 0xFFF7D038, 2.0f, 0.5f);
        continue;
      }
      if (px + PW > ex && px < ex + ew - 2 && py + PH > ey && py < ey + eh) {
        if (pvy > 0.5f && py + PH - ey < 9) {
          e->dying = 1; pvy = -4.6f; plat_sfx(SFX_STOMP);
          spawn_parts(e->x + 6, e->y + 4, 8, e->type == E_BEETLE ? 0xFF3FA34D : 0xFFF7D038, 1.5f, 0.3f);
        } else hurt();
      }
    }
  }
}

static void update_parts(void) {
  for (int i = 0; i < 160; i++) if (parts[i].life > 0) {
    Part* p = &parts[i];
    p->x += p->vx; p->y += p->vy; p->vy += 0.12f; p->vx *= 0.96f; p->life--;
  }
}

static void update_camera(int snap) {
  float tx = px + PW / 2 - SCR_W / 2 + face * 28, ty = py + PH / 2 - SCR_H / 2 - 10;
  if (tx < 0) tx = 0;
  if (tx > LW * T - SCR_W) tx = LW * T - SCR_W;
  if (ty > LH * T - SCR_H) ty = LH * T - SCR_H;
  if (ty < 0) ty = 0;
  if (snap) { camx = tx; camy = ty; }
  else { camx += (tx - camx) * 0.12f; camy += (ty - camy) * 0.12f; }
}

void game_update(const Input* in) {
  frame++; state_t++;
  int jp = in->jump && !prev_jump, sp = in->start && !prev_start;
  switch (state) {
    case ST_TITLE:
      if ((jp || sp) && state_t > 20) { lives = 3; bananas = 0; state = ST_MAP; state_t = 0; map_from = map_node; map_t = 99; plat_sfx(SFX_SELECT); }
      break;
    case ST_PLAY:
      if (sp) { state = ST_PAUSE; plat_sfx(SFX_SELECT); break; }
      lvl_frames++;
      update_player(in);
      if (state == ST_PLAY || state == ST_CLEAR) update_ents();
      update_parts();
      if (!dead_t) update_camera(0);
      break;
    case ST_PAUSE:
      if (sp) { state = ST_PLAY; plat_sfx(SFX_SELECT); }
      else if (in->down && !prev_down) { state = ST_MAP; state_t = 0; map_t = 99; }
      break;
    case ST_CLEAR:
      update_parts();
      if (state_t > 150) {
        int first_all = !cleared[lvl + 1];
        cleared[lvl + 1] = 1;
        for (int i = 1; i <= NLEVELS; i++) if (!cleared[i]) first_all = 0;
        save_progress();
        if (first_all) { state = ST_WIN; state_t = 0; }
        else { state = ST_MAP; state_t = 0; map_t = 99; }
      }
      break;
    case ST_OVER:
      if ((jp || sp) && state_t > 60) { state = ST_TITLE; state_t = 0; save_progress(); }
      break;
    case ST_WIN:
      if ((jp || sp) && state_t > 60) { state = ST_MAP; state_t = 0; map_t = 99; }
      break;
    case ST_MAP:
      if (map_t < 24) { map_t++; break; }
      {
        int fwd = (in->right && !prev_right) || (in->up && !prev_up);
        int back = (in->left && !prev_left) || (in->down && !prev_down);
        if (fwd && map_node < NLEVELS && node_open(map_node + 1)) { map_from = map_node; map_node++; map_t = 0; plat_sfx(SFX_WALL); }
        else if (back && map_node > 0) { map_from = map_node; map_node--; map_t = 0; plat_sfx(SFX_WALL); }
        else if (jp && map_node > 0) { start_level(map_node - 1); update_camera(1); plat_sfx(SFX_SELECT); }
      }
      break;
  }
  prev_jump = in->jump; prev_start = in->start; prev_down = in->down;
  prev_up = in->up; prev_left = in->left; prev_right = in->right;
}

// ============================================================ drawing
static void draw_background(void) {
  for (int y = 0; y < SCR_H; y++) {
    uint32_t c = blend(L->sky0, L->sky1, y / (float)SCR_H);
    for (int x = 0; x < SCR_W; x++) FB[y * SCR_W + x] = c;
  }
  float vy = camy / (float)(LH * T - SCR_H + 1);   // 0..1 how far down the level the camera is
  int th = L->theme;
  if (th == 2) {
    // cave roof: hanging rocks
    for (int x = 0; x < SCR_W; x++) {
      float wx = x + camx * 0.3f;
      int h = 26 + (int)(fabsf(sinf(wx * 0.05f)) * 30 * fabsf(sinf(wx * 0.013f)));
      for (int y = 0; y < h; y++) put(x, y, L->far);
    }
  } else if (th == 1) {
    // stars and a big moon
    for (int i = 0; i < 40; i++) {
      uint32_t h = hash2(i, 99);
      int sx = (int)((h % 400) - camx * 0.05f) % 400, sy = (h >> 12) % 90;
      if (sx < 0) sx += 400;
      put(sx, sy, (frame / 20 + i) % 7 ? 0xFFE8E0F8 : 0xFF9C8FC8);
    }
    for (int j = -14; j <= 14; j++) for (int i = -14; i <= 14; i++) if (i * i + j * j <= 196) put(250 + i, 36 + j, (i + 4) * (i + 4) + (j - 3) * (j - 3) < 60 ? 0xFFE6DFC8 : 0xFFF6F0DC);
  } else {
    for (int j = -10; j <= 10; j++) for (int i = -10; i <= 10; i++) if (i * i + j * j <= 100) put(260 + i, 30 + j - (int)(vy * 10), 0xFFFFF4C2);
    // clouds
    for (int c = 0; c < 6; c++) {
      int cx = (int)(c * 97 - camx * 0.1f - frame * 0.05f) % 480; if (cx < -60) cx += 480;
      int cy = 20 + (c * 37) % 50;
      for (int k = 0; k < 4; k++) for (int j = -6; j <= 6; j++) for (int i = -9; i <= 9; i++)
        if (i * i / 81.0f + j * j / 36.0f <= 1 && j < 4) put(cx + k * 9 + i, cy - (k % 2) * 4 + j, 0xFFFFFFFF);
    }
  }
  // far hills
  int base_far = 120 - (int)(vy * 30);
  for (int x = 0; x < SCR_W; x++) {
    float wx = x + camx * 0.25f;
    int h = base_far - (int)(sinf(wx * 0.021f) * 16 + sinf(wx * 0.053f + 1) * 7);
    if (th == 1 || th == 2) h = base_far - (int)(fabsf(sinf(wx * 0.012f)) * 34);
    if (th == 3) break;
    for (int y = h; y < SCR_H; y++) put(x, y, L->far);
  }
  if (th == 3) {
    // a sea of clouds far below
    for (int layer = 0; layer < 2; layer++) {
      int base = 140 - (int)(vy * 20) + layer * 16;
      uint32_t c = layer ? 0xFFFFFFFF : 0xFFE3F1FF;
      for (int x = 0; x < SCR_W; x++) {
        float wx = x + camx * (0.2f + layer * 0.2f) + frame * 0.1f;
        int h = base - (int)(fabsf(sinf(wx * 0.04f)) * 12 + fabsf(sinf(wx * 0.11f)) * 5);
        for (int y = h; y < SCR_H; y++) put(x, y, c);
      }
    }
    return;
  }
  if (th == 4) {
    // waterfalls pouring down the cliffs
    for (int k = 0; k < 4; k++) {
      int wx = (int)(k * 110 + 40 - camx * 0.25f) % 440; if (wx < -30) wx += 440;
      for (int y = 10; y < SCR_H; y++) for (int i = 0; i < 16; i++) {
        int streak = ((y - frame * 2 + i * 7) % 11 + 11) % 11 < 3;
        put(wx + i, y, streak ? 0xFFEAF8FF : (i == 0 || i == 15 ? 0xFF7FC4D8 : 0xFFAEE0EE));
      }
    }
  }
  // near layer: jungle trees, or temple ruins
  int base_near = 150 - (int)(vy * 50);
  if (th == 1 || th == 2) {
    for (int x = 0; x < SCR_W; x++) {
      float wx = x + camx * 0.5f;
      int m = ((int)wx % 90 + 90) % 90;
      int h = m < 22 ? base_near - 60 + (m < 2 || m > 19 ? 6 : 0) : base_near - 8;
      for (int y = h; y < SCR_H; y++) put(x, y, L->nearc);
    }
  } else {
    uint32_t leaf = blend(L->nearc, L->far, 0.35f), lite = blend(L->nearc, 0xFFFFFFFF, 0.12f);
    float off = camx * 0.5f;
    int k0 = (int)floorf((off - 60) / 80), k1 = (int)floorf((off + SCR_W + 60) / 80);
    for (int k = k0; k <= k1; k++) {
      uint32_t h = hash2(k, 3);
      int tx = k * 80 + (int)(h % 30) - (int)off, top = base_near - 70 - (int)((h >> 8) % 25);
      fill(tx - 2, top, 5, SCR_H - top, L->nearc);
      for (int b = 0; b < 5; b++) {
        int bx = tx + (int)(hash2(k, b) % 36) - 18, by = top + (int)(hash2(b, k) % 14) - 6, r = 11 + (int)(hash2(k + b, 9) % 6);
        for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r)
          put(bx + i, by + j, (i + j < -r / 2) ? lite : leaf);
      }
    }
    for (int x = 0; x < SCR_W; x++) {
      float wx = x + off;
      int h = base_near - 18 - (int)(fabsf(sinf(wx * 0.06f)) * 10 + sinf(wx * 0.21f) * 3);
      for (int y = h; y < SCR_H; y++) put(x, y, L->nearc);
    }
  }
}

static void draw_tile(char c, int tx, int ty, int sx, int sy) {
  switch (c) {
    case '#': {
      int top = tile(tx, ty - 1) != '#';
      for (int j = 0; j < T; j++) for (int i = 0; i < T; i++) {
        uint32_t h = hash2(tx * T + i, ty * T + j);
        uint32_t col = (h % 23 == 0) ? 0xFF6E4520 : (h % 31 == 0 ? 0xFFA0703A : 0xFF8A5A2B);
        if (top) {
          int gh = 4 + (int)(hash2(tx * T + i, 7) % 3);
          if (j < gh) col = j == 0 ? 0xFF9BE37A : (j < 2 ? 0xFF6CC24A : 0xFF4CAF50);
          else if (j == gh) col = 0xFF2E7D32;
        }
        if (tile(tx - 1, ty) != '#' && i == 0) col = blend(col, 0xFF000000, 0.35f);
        if (tile(tx + 1, ty) != '#' && i == T - 1) col = blend(col, 0xFF000000, 0.35f);
        put(sx + i, sy + j, col);
      }
      break;
    }
    case 'X': {
      int top = !solid_c(tile(tx, ty - 1));
      int temple = L->theme == 1 || L->theme == 2;
      uint32_t base = 0xFF8C8F9C, dark = 0xFF575A66, lite = 0xFFB6BAC6;
      if (L->theme == 1) { base = 0xFF8A7BA8; dark = 0xFF4E4370; lite = 0xFFB8A8D8; }
      if (L->theme == 2) { base = 0xFF6D6259; dark = 0xFF3E3630; lite = 0xFF8F8378; }
      for (int j = 0; j < T; j++) for (int i = 0; i < T; i++) {
        int row = j / 8, off = (row + ty) % 2 ? 8 : 0;
        int mx = (i + off) % 16 == 0, my = j % 8 == 0;
        uint32_t col = (mx || my) ? dark : base;
        if (!mx && !my && (j % 8 == 1 || (i + off) % 16 == 1)) col = lite;
        uint32_t h = hash2(tx * T + i, ty * T + j);
        if (col == base && h % 17 == 0) col = blend(base, dark, 0.4f);
        if (top && j < 2 && !temple) col = j == 0 ? 0xFF9BE37A : 0xFF4CAF50;
        put(sx + i, sy + j, col);
      }
      break;
    }
    case '=':
      for (int i = 0; i < T; i++) {
        put(sx + i, sy, 0xFF3B2414);
        put(sx + i, sy + 1, 0xFFD69A55);
        for (int j = 2; j < 5; j++) put(sx + i, sy + j, (i % 8 == 7) ? 0xFF6B4220 : 0xFFB5793A);
        put(sx + i, sy + 5, 0xFF3B2414);
      }
      for (int j = 6; j < 10; j++) { put(sx + 3, sy + j, 0xFF6B4220); put(sx + 12, sy + j, 0xFF6B4220); }
      break;
    case '^':
      for (int k = 0; k < 2; k++) for (int j = 0; j < 8; j++) {
        int hw = j / 2;
        for (int i = -hw; i <= hw; i++) put(sx + k * 8 + 4 + i, sy + 8 + j, i == -hw ? 0xFF5A5E6A : (i == hw ? 0xFF8C8F9C : 0xFFD8DCE6));
      }
      fill(sx, sy + 15, 16, 1, 0xFF3B3E48);
      break;
    case 'o': {
      int bob = (int)(sinf((frame + tx * 9) * 0.08f) * 2);
      sprite(SPR_BANANA, sx, sy + bob, 0);
      break;
    }
    case 's': {
      int squash = 0;
      fill(sx + 3, sy + 11, 10, 5, 0xFF575A66);
      fill(sx + 4, sy + 12, 8, 3, 0xFF8C8F9C);
      for (int j = 0; j < 5; j++) for (int i = -7 + j / 2; i <= 7 - j / 2; i++)
        put(sx + 8 + i, sy + 6 + j + squash, j == 0 ? 0xFFFF8A80 : ((i + 8) % 5 == 0 ? 0xFFFFFFFF : 0xFFD8463C));
      fill(sx + 1, sy + 11, 14, 1, 0xFF2B1A12);
      break;
    }
    case '?': case 'u': {
      int used = c == 'u';
      uint32_t base = used ? 0xFF8A5A2B : 0xFFE0A030, lite = used ? 0xFFA0703A : 0xFFFFD36A, dark = used ? 0xFF5A3A1E : 0xFFA0661A;
      fill(sx, sy, 16, 16, 0xFF2B1A12);
      fill(sx + 1, sy + 1, 14, 14, base);
      fill(sx + 1, sy + 1, 14, 1, lite); fill(sx + 1, sy + 1, 1, 14, lite);
      fill(sx + 1, sy + 14, 14, 1, dark); fill(sx + 14, sy + 1, 1, 14, dark);
      put(sx + 3, sy + 3, dark); put(sx + 12, sy + 3, dark); put(sx + 3, sy + 12, dark); put(sx + 12, sy + 12, dark);
      if (!used) {
        int b = ((frame / 10) % 6 == 0) ? 1 : 0;
        static const char* Q[7] = { ".XXX.", "X...X", "...X.", "..X..", "..X..", ".....", "..X.." };
        for (int j = 0; j < 7; j++) for (int i = 0; i < 5; i++) if (Q[j][i] == 'X') { put(sx + 6 + i, sy + 4 + j + b, 0xFFFFFFFF); put(sx + 7 + i, sy + 5 + j + b, dark); }
        for (int j = 0; j < 7; j++) for (int i = 0; i < 5; i++) if (Q[j][i] == 'X') put(sx + 6 + i, sy + 4 + j + b, 0xFFFFFFFF);
      }
      break;
    }
    case 'c': {
      int on = check_x == tx * T + 3 && check_y == ty * T + T - PH;
      fill(sx + 7, sy + 1, 2, 15, 0xFF3B2414);
      uint32_t fc = on ? 0xFF4CAF50 : 0xFF9E9E9E;
      int wave = on ? (frame / 8) % 2 : 0;
      for (int j = 0; j < 6; j++) for (int i = 0; i < 7 - j / 2 + wave; i++) put(sx + 9 + i, sy + 2 + j, fc);
      fill(sx + 5, sy + 15, 6, 1, 0xFF2B1A12);
      break;
    }
  }
}

static void draw_world(void) {
  int cx = (int)floorf(camx), cy = (int)floorf(camy);
  int tx0 = cx / T, ty0 = cy / T;
  // signs (behind everything)
  for (int i = 0; i < nsigns; i++) {
    int sx = sign_x[i] * T - cx, sy = sign_y[i] * T - cy;
    fill(sx + 7, sy + 8, 2, 8, 0xFF5A3A1E);
    fill(sx + 1, sy + 1, 14, 9, 0xFF3B2414);
    fill(sx + 2, sy + 2, 12, 7, 0xFFC8955A);
    for (int k = 0; k < 3; k++) fill(sx + 4, sy + 3 + k * 2, 8 - (k == 2) * 3, 1, 0xFF7A5230);
  }
  // goal pole
  {
    int sx = goal_x * T - cx, sy = goal_y * T - cy;
    fill(sx + 7, sy - 3 * T, 2, 4 * T, 0xFFE0E0E0);
    fill(sx + 6, sy - 3 * T - 3, 4, 4, 0xFFF7D038);
    int wave = (frame / 6) % 3;
    int raised = state == ST_CLEAR ? (state_t < 40 ? state_t / 2 : 20) : 0;
    for (int j = 0; j < 10; j++) for (int i = 0; i < 14 - (j > 4 ? j - 4 : 4 - j) + wave; i++)
      put(sx + 9 + i, sy - 3 * T + 26 - raised + j, (i + j) % 6 < 3 ? 0xFFD8463C : 0xFFF7D038);
    fill(sx + 3, sy + 13, 10, 3, 0xFF575A66);
  }
  for (int ty = ty0; ty <= ty0 + SCR_H / T + 1; ty++)
    for (int tx = tx0; tx <= tx0 + SCR_W / T + 1; tx++) {
      char c = tile(tx, ty);
      if (c != ' ' && tx >= 0 && tx < LW) draw_tile(c, tx, ty, tx * T - cx, ty * T - cy);
    }
  for (int i = 0; i < nents; i++) {
    Ent* e = &ents[i];
    if (!e->alive) continue;
    int sx = (int)e->x - 2 - cx, sy = (int)e->y - 6 - cy;
    if (e->type == E_POW) {
      int bob = (int)(sinf(e->t * 0.1f) * 1.5f);
      sprite(e->k == 2 ? SPR_SWORD : SPR_WING, (int)e->x - 2 - cx, (int)e->y - 2 - cy + bob, 0);
      continue;
    }
    if (e->type == E_BEETLE) {
      if (e->dying) {
        // squashed flat
        for (int j = 0; j < 4; j++) fill(sx + 1, sy + 12 + j, 14, 1, j == 0 ? 0xFF2B1A12 : 0xFF24723A);
      } else sprite(SPR_BEETLE[(e->t / 8) % 2], sx, sy, e->vx > 0);
    } else {
      sy = (int)e->y - 4 - cy;
      if (e->dying) sprite_flat(SPR_BEE[0], sx, sy + e->dying, e->vx > 0, 0xFFFFFFFF);
      else sprite(SPR_BEE[(e->t / 3) % 2], sx, sy, e->vx > 0);
    }
  }
  // player
  {
    int sx = (int)floorf(px) - 3 - cx, sy = (int)floorf(py) - 3 - cy;
    const char* const* s = (frame % 200) < 7 ? SPR_DINO_BLINK : SPR_DINO_IDLE;
    static const char* const* RUN[4] = { SPR_DINO_RUN1, SPR_DINO_RUN2, SPR_DINO_RUN3, SPR_DINO_RUN2 };
    int walling = !on_ground && wall_dir && pvy > 0 && !dead_t;
    if (dead_t) s = SPR_DINO_HURT;
    else if (atk_t > 4 && power != 2) s = SPR_DINO_ATTACK;
    else if (walling) s = SPR_DINO_WALL;
    else if (!on_ground) s = pvy < 0 ? SPR_DINO_JUMP : SPR_DINO_FALL;
    else if (run_t) s = RUN[(run_t / 5) % 4];
    else if (land_t > 2) s = SPR_DINO_FALL;
    int flip = face < 0;
    if (walling) flip = wall_dir > 0;   // back to the wall
    sx = (int)floorf(px) - (flip ? 5 : 4) - cx; sy = (int)floorf(py) + PH - 19 - cy;
    int show = !(inv_t && (inv_t / 4) % 2);
    if (power == 1 && show) {
      // little wings on his back
      int wx = flip ? sx + 15 : sx - 1, wy = sy + 5 + ((frame / 8) % 2);
      for (int j = 0; j < 6; j++) for (int i = 0; i < 6 - j / 2; i++) {
        int x = flip ? wx + i : wx + 5 - i;
        put(x, wy + j, (i == 6 - j / 2 - 1 || j == 0 || j == 5) ? 0xFF2B1A12 : 0xFFFFFFFF);
      }
    }
    if (show) sprite_n(s, 20, sx, sy, flip);
    if (power == 2 && show && !dead_t) {
      // the sword: held up behind him, or swung in an arc
      float cxp = sx + (flip ? 6 : 13), cyp = sy + 11;
      float ang;
      if (atk_t > 0) { float k = (16 - atk_t) / 16.0f; ang = -2.2f + k * 3.0f; }
      else ang = -2.0f;
      if (flip) ang = 3.14159f - ang;
      float ca = cosf(ang), sa = sinf(ang);
      for (int d = 2; d < 17; d++) {
        int x = (int)(cxp + ca * d), y = (int)(cyp + sa * d);
        put(x - 1, y, 0xFF2B1A12); put(x + 1, y, 0xFF2B1A12); put(x, y - 1, 0xFF2B1A12); put(x, y + 1, 0xFF2B1A12);
      }
      for (int d = 2; d < 17; d++) {
        int x = (int)(cxp + ca * d), y = (int)(cyp + sa * d);
        put(x, y, d < 5 ? 0xFFC9A227 : (d == 5 ? 0xFF8B5A2B : 0xFFE8ECF4));
      }
      if (atk_t > 4) for (int d = 10; d < 19; d += 2) {   // swoosh
        float a2 = ang - (flip ? -0.5f : 0.5f);
        put((int)(cxp + cosf(a2) * d), (int)(cyp + sinf(a2) * d), 0xFFFFFFFF);
      }
    }
  }
  for (int i = 0; i < 160; i++) if (parts[i].life > 0) {
    int x = (int)parts[i].x - cx, y = (int)parts[i].y - cy;
    put(x, y, parts[i].c);
    if (parts[i].life > 10) put(x + 1, y, parts[i].c);
  }
  // sign text when you stand near a sign
  for (int i = 0; i < nsigns; i++) {
    float dx = px + PW / 2 - (sign_x[i] * T + 8), dy = py - sign_y[i] * T;
    if (dx * dx < 48 * 48 && dy * dy < 40 * 40) {
      const char* s = L->signs[i];
      int w = text_w(s, 1) + 8;
      int bx = SCR_W / 2 - w / 2, by = 30;
      fill(bx, by, w, 13, 0xFF1B1020);
      fill(bx + 1, by + 1, w - 2, 11, 0xFFFDF3DC);
      text_raw(s, bx + 4, by + 4, 1, 0xFF3B2414);
    }
  }
}

static void draw_hud(void) {
  char buf[64];
  sprite(SPR_BANANA, 2, -1, 0);
  snprintf(buf, sizeof buf, "%02d", bananas);
  text(buf, 18, 6, 2, 0xFFFFF4C2);
  // lives: Kip's head
  for (int j = 0; j < 14; j++) for (int i = 0; i < 20; i++) { char c = SPR_DINO_IDLE[j][i]; if (c != '.') put(SCR_W - 56 + i, 1 + j, pal(c)); }
  snprintf(buf, sizeof buf, "x%d", lives < 0 ? 0 : lives);
  text(buf, SCR_W - 34, 6, 2, 0xFFFFFFFF);
  if (power) {
    fill(SCR_W - 80, 1, 18, 18, 0xFF1B1020); fill(SCR_W - 79, 2, 16, 16, 0xFF3B2F6B);
    sprite(power == 2 ? SPR_SWORD : SPR_WING, SCR_W - 79, 2, 0);
  }
}

static void draw_kip_big(int x, int y, int sc, const char* const* s, int flip) {
  for (int j = 0; j < 20; j++) for (int i = 0; i < 20; i++) {
    char c = s[j][flip ? 19 - i : i];
    if (c != '.') fill(x + i * sc, y + j * sc, sc, sc, pal(c));
  }
}

static void disc(int cx, int cy, int r, uint32_t c) {
  for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r) put(cx + i, cy + j, c);
}
static void draw_map(void) {
  char buf[64];
  // sea
  for (int y = 0; y < SCR_H; y++) {
    uint32_t c = blend(0xFF2F7FC1, 0xFF5BB3E6, y / (float)SCR_H);
    for (int x = 0; x < SCR_W; x++) FB[y * SCR_W + x] = c;
  }
  for (int i = 0; i < 50; i++) {
    uint32_t h = hash2(i, 5);
    int x = (int)(h % SCR_W + frame / 6) % SCR_W, y = (h >> 9) % SCR_H;
    if ((frame / 15 + i) % 4) fill(x, y, 3, 1, 0xFF9ED6F5);
  }
  // the island: a blob of circles, sand round the edge
  static const short BLOB[][3] = { {40, 140, 30}, {84, 130, 34}, {130, 110, 34}, {100, 66, 34}, {166, 56, 34},
    {226, 92, 38}, {280, 52, 32}, {150, 90, 30}, {190, 130, 28}, {60, 100, 24}, {250, 140, 22} };
  int nb = sizeof BLOB / sizeof BLOB[0];
  for (int b = 0; b < nb; b++) disc(BLOB[b][0], BLOB[b][1], BLOB[b][2] + 5, 0xFFF0D9A0);
  for (int b = 0; b < nb; b++) disc(BLOB[b][0], BLOB[b][1], BLOB[b][2], 0xFF5DB85A);
  for (int y = 0; y < SCR_H; y++) for (int x = 0; x < SCR_W; x++)
    if (FB[y * SCR_W + x] == 0xFF5DB85A && hash2(x, y) % 9 == 0) put(x, y, 0xFF4FA64C);
  // mountain with the beetle cave (level 5)
  for (int j = 0; j < 46; j++) for (int i = -j; i <= j; i++) put(226 + i, 50 + j, j < 10 ? 0xFFF4F4F4 : ((i + j) % 9 ? 0xFF8C8F9C : 0xFF757884));
  disc(226, 92, 9, 0xFF2A2220); fill(217, 92, 19, 9, 0xFF2A2220);
  // cliff and cloud for the sky bridge (level 6)
  fill(270, 30, 24, 26, 0xFF8A5A2B); fill(270, 30, 24, 3, 0xFF5DB85A);
  for (int k = 0; k < 3; k++) disc(270 + k * 12, 24 - (k % 2) * 4, 9, 0xFFFFFFFF);
  // waterfall (level 3)
  for (int y = 10; y < 66; y++) for (int i = -5; i <= 5; i++) {
    int streak = ((y - frame + i * 5) % 9 + 9) % 9 < 2;
    put(96 + i, y, streak ? 0xFFEAF8FF : 0xFF8ED0EA);
  }
  fill(84, 4, 24, 8, 0xFF757884);
  // temple (level 4)
  for (int k = 0; k < 4; k++) fill(150 + k * 4, 20 + k * 6, 32 - k * 8, 7, k % 2 ? 0xFF8A7BA8 : 0xFF7A6B98);
  fill(163, 38, 6, 6, 0xFF3A2F5C);
  // trees
  static const short TREES[][2] = { {20, 120}, {60, 116}, {110, 128}, {150, 120}, {180, 138}, {200, 118}, {70, 80}, {128, 78}, {256, 130}, {62, 152}, {196, 72} };
  for (unsigned t = 0; t < sizeof TREES / sizeof TREES[0]; t++) {
    fill(TREES[t][0] - 1, TREES[t][1], 3, 6, 0xFF6B3F1D);
    disc(TREES[t][0], TREES[t][1] - 3, 6, 0xFF2E7D32); disc(TREES[t][0] - 2, TREES[t][1] - 5, 2, 0xFF4CAF50);
  }
  // Dino's house
  fill(26, 136, 24, 14, 0xFFE8D5B0); fill(35, 142, 6, 8, 0xFF6B3F1D);
  for (int j = 0; j < 11; j++) fill(38 - j - 3, 126 + j, 2 * j + 7, 1, 0xFF4C8C2B);
  // path between the stops
  for (int n = 0; n < NLEVELS; n++) {
    int open = node_open(n + 1);
    float x0 = MAP_XY[n][0], y0 = MAP_XY[n][1], x1 = MAP_XY[n + 1][0], y1 = MAP_XY[n + 1][1];
    float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    for (float d = 6; d < len - 5; d += 6) {
      int x = (int)(x0 + (x1 - x0) * d / len), y = (int)(y0 + (y1 - y0) * d / len);
      fill(x - 1, y - 1, 3, 3, open ? 0xFFFFF4C2 : 0xFF3E7D3B);
    }
  }
  for (int n = 1; n <= NLEVELS; n++) {
    int x = MAP_XY[n][0], y = MAP_XY[n][1];
    uint32_t c = cleared[n] ? 0xFFF7D038 : (node_open(n) ? 0xFFD8463C : 0xFF8C8F9C);
    disc(x, y, 6, 0xFF2B1A12); disc(x, y, 5, c); disc(x - 1, y - 2, 2, blend(c, 0xFFFFFFFF, 0.5f));
    snprintf(buf, sizeof buf, "%d", n);
    text_raw(buf, x - 1, y - 2, 1, 0xFF2B1A12);
  }
  // Dino walking between stops
  float t = map_t >= 24 ? 1 : map_t / 24.0f;
  float dx = MAP_XY[map_from][0] + (MAP_XY[map_node][0] - MAP_XY[map_from][0]) * t;
  float dy = MAP_XY[map_from][1] + (MAP_XY[map_node][1] - MAP_XY[map_from][1]) * t;
  int moving = map_t < 24;
  const char* const* sp = moving ? (((map_t / 5) % 2) ? SPR_DINO_RUN1 : SPR_DINO_RUN2) : ((frame % 200) < 7 ? SPR_DINO_BLINK : SPR_DINO_IDLE);
  sprite_n(sp, 20, (int)dx - 10, (int)dy - 21, MAP_XY[map_node][0] < MAP_XY[map_from][0]);
  // labels
  fill(0, 0, SCR_W, 16, 0xFF1B1020);
  if (map_node == 0) text_c("DINO'S HOUSE", 3, 2, 0xFFFFFFFF);
  else {
    text(LEVELS[map_node - 1].name, 4, 3, 2, 0xFFFFFFFF);
    const char* hint = cleared[map_node] ? "CLEARED!  A: PLAY" : "A: PLAY";
    text(hint, SCR_W - text_w(hint, 1) - 4, 6, 1, cleared[map_node] ? 0xFFF7D038 : 0xFFFFF4C2);
  }
  int nc = 0; for (int i = 1; i <= NLEVELS; i++) nc += cleared[i];
  fill(0, SCR_H - 11, SCR_W, 11, 0xFF1B1020);
  snprintf(buf, sizeof buf, "CLEARED %d/%d   BANANAS %d   LIVES %d", nc, NLEVELS, total_bananas, lives);
  text(buf, 4, SCR_H - 8, 1, 0xFFFFF4C2);
  text("WALK: STICK OR D-PAD", SCR_W - text_w("WALK: STICK OR D-PAD", 1) - 4, SCR_H - 8, 1, 0xFFFFF4C2);
}

void game_draw(uint32_t* fb) {
  FB = fb;
  char buf[64];
  if (state == ST_TITLE) {
    draw_background();
    // a strip of ground to stand on
    for (int x = 0; x < SCR_W; x++) for (int y = 150; y < SCR_H; y++) {
      uint32_t h = hash2(x, y);
      put(x, y, y < 155 ? (y == 150 ? 0xFF9BE37A : 0xFF4CAF50) : (h % 23 ? 0xFF8A5A2B : 0xFF6E4520));
    }
    int bob = (state_t / 20) % 2;
    draw_kip_big(SCR_W / 2 - 30, 92 + bob, 3, (state_t % 200) < 8 ? SPR_DINO_BLINK : (bob ? SPR_DINO_IDLE : SPR_DINO_RUN2), 0);
    sprite(SPR_BANANA, 40, 128 + (int)(sinf(frame * 0.08f) * 2), 0);
    sprite(SPR_BANANA, 264, 128 + (int)(sinf(frame * 0.08f + 2) * 2), 1);
    text_c("CANOPY DINO", 24, 5, 0xFFF7D038);
    text_c("A LITTLE JUNGLE ADVENTURE", 58, 1, 0xFF3B2414);
    if ((state_t / 30) % 2 == 0) text_c("PRESS A TO START", 80, 2, 0xFFFFFFFF);
    text_c("MOVE: STICK   JUMP: A OR B   HEADBUTT: X OR Y   PAUSE: +", 167, 1, 0xFFFFF4C2);
    return;
  }
  if (state == ST_MAP) { draw_map(); return; }
  draw_background();
  draw_world();
  if (!g_no_hud) draw_hud();
  if (state == ST_PLAY && state_t < 120) {
    text_c(L->name, 60, 3, 0xFFFFFFFF);
  }
  if (state == ST_PAUSE) {
    darken(0, 0, SCR_W, SCR_H, 0.5f);
    text_c("PAUSED", 60, 4, 0xFFFFFFFF);
    text_c("+ TO KEEP PLAYING", 95, 1, 0xFFFFF4C2);
    text_c("DOWN TO GO BACK TO THE MAP", 105, 1, 0xFFFFF4C2);
  }
  if (state == ST_CLEAR) {
    text_c("LEVEL CLEAR!", 50, 4, 0xFFF7D038);
    snprintf(buf, sizeof buf, "TIME %d.%d", lvl_frames / 60, (lvl_frames % 60) / 6);
    text_c(buf, 82, 2, 0xFFFFFFFF);
  }
  if (state == ST_OVER) {
    darken(0, 0, SCR_W, SCR_H, 0.6f);
    text_c("GAME OVER", 60, 4, 0xFFD8463C);
    if (state_t > 60) text_c("PRESS A", 100, 2, 0xFFFFFFFF);
  }
  if (state == ST_WIN) {
    darken(0, 0, SCR_W, SCR_H, 0.55f);
    text_c("YOU WIN!", 36, 5, 0xFFF7D038);
    draw_kip_big(SCR_W / 2 - 30, 64, 3, (state_t / 15) % 2 ? SPR_DINO_JUMP : SPR_DINO_IDLE, 0);
    snprintf(buf, sizeof buf, "BANANAS: %d", total_bananas);
    text_c(buf, 128, 2, 0xFFFFFFFF);
    if (state_t > 60) text_c("PRESS A", 150, 2, 0xFFFFF4C2);
  }
}

// ============================================================ sound
typedef struct { float f0, f1, t, dur, vol, phase; int wave; uint32_t lfsr; } Voice;
static Voice voices[8];
static float mus_t; static int mus_step; static float lead_ph, bass_ph, lead_env;
static int lead_note, bass_note;

void synth_trigger(int id) {
  Voice v = { 0 };
  v.wave = 0; v.vol = 0.18f; v.lfsr = 1;
  switch (id) {
    case SFX_JUMP: v.f0 = 330; v.f1 = 720; v.dur = 0.12f; break;
    case SFX_WALL: v.f0 = 420; v.f1 = 820; v.dur = 0.10f; break;
    case SFX_BANANA: v.f0 = 1320; v.f1 = 1760; v.dur = 0.09f; v.vol = 0.12f; break;
    case SFX_STOMP: v.f0 = 500; v.f1 = 90; v.dur = 0.15f; v.wave = 2; v.vol = 0.22f; break;
    case SFX_HURT: v.f0 = 600; v.f1 = 80; v.dur = 0.6f; v.wave = 1; v.vol = 0.2f; break;
    case SFX_SPRING: v.f0 = 200; v.f1 = 1200; v.dur = 0.3f; v.wave = 1; break;
    case SFX_CHECK: v.f0 = 660; v.f1 = 1320; v.dur = 0.25f; v.wave = 3; v.vol = 0.15f; break;
    case SFX_CLEAR: v.f0 = 523; v.f1 = 1046; v.dur = 1.2f; v.wave = 3; v.vol = 0.18f; break;
    case SFX_LIFE: v.f0 = 784; v.f1 = 1568; v.dur = 0.5f; v.wave = 3; v.vol = 0.16f; break;
    case SFX_SELECT: v.f0 = 880; v.f1 = 880; v.dur = 0.08f; v.vol = 0.12f; break;
    case SFX_DASH: v.f0 = 900; v.f1 = 200; v.dur = 0.18f; v.wave = 2; v.vol = 0.16f; break;
    case SFX_SWORD: v.f0 = 2400; v.f1 = 600; v.dur = 0.14f; v.wave = 2; v.vol = 0.14f; break;
    case SFX_JUMP2: v.f0 = 500; v.f1 = 1100; v.dur = 0.14f; v.wave = 1; v.vol = 0.2f; break;
    default: return;
  }
  for (int i = 0; i < 8; i++) if (voices[i].t >= voices[i].dur) { voices[i] = v; return; }
  voices[0] = v;
}

// background tune: 32 sixteenth-note steps, lead and bass (MIDI note numbers, 0 = rest)
static const uint8_t LEAD[64] = {
  76, 0, 79, 81, 0, 79, 76, 0, 74, 0, 72, 74, 76, 0, 79, 0,
  81, 0, 79, 76, 0, 74, 72, 0, 74, 0, 76, 0, 74, 0, 0, 0,
  76, 0, 79, 81, 0, 84, 83, 0, 81, 0, 79, 81, 79, 0, 76, 0,
  74, 0, 76, 79, 0, 76, 74, 0, 72, 0, 0, 0, 0, 0, 0, 0,
};
static const uint8_t BASS[64] = {
  48, 0, 55, 0, 48, 0, 55, 0, 45, 0, 52, 0, 45, 0, 52, 0,
  41, 0, 48, 0, 41, 0, 48, 0, 43, 0, 50, 0, 43, 0, 47, 0,
  48, 0, 55, 0, 48, 0, 55, 0, 45, 0, 52, 0, 45, 0, 52, 0,
  41, 0, 48, 0, 43, 0, 50, 0, 48, 0, 43, 0, 48, 0, 0, 0,
};
static float midi_hz(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

void synth_render(int16_t* out, int frames, int rate) {
  const float step_len = 0.115f;
  int music = state == ST_PLAY || state == ST_TITLE || state == ST_MAP;
  for (int f = 0; f < frames; f++) {
    float s = 0, dt = 1.0f / rate;
    if (music) {
      mus_t += dt;
      if (mus_t >= step_len) {
        mus_t -= step_len; mus_step = (mus_step + 1) % 64;
        if (LEAD[mus_step]) { lead_note = LEAD[mus_step]; lead_env = 1; }
        if (BASS[mus_step]) bass_note = BASS[mus_step];
      }
      if (lead_note) {
        lead_ph += midi_hz(lead_note) * dt; lead_ph -= (int)lead_ph;
        s += (lead_ph < 0.25f ? 1 : -1) * 0.045f * lead_env;
        lead_env *= 0.99985f;
      }
      if (bass_note) {
        bass_ph += midi_hz(bass_note) * dt; bass_ph -= (int)bass_ph;
        s += (4 * fabsf(bass_ph - 0.5f) - 1) * 0.10f;
      }
    }
    for (int i = 0; i < 8; i++) {
      Voice* v = &voices[i];
      if (v->t >= v->dur) continue;
      float k = v->t / v->dur, fr;
      if (v->wave == 3) { int n = (int)(k * 4); fr = v->f0 * (n == 0 ? 1 : n == 1 ? 1.26f : n == 2 ? 1.5f : 2.0f); }
      else fr = v->f0 + (v->f1 - v->f0) * k;
      v->phase += fr * dt; v->phase -= (int)v->phase;
      float w;
      if (v->wave == 2) {
        if (v->phase < fr * dt) { uint32_t b = ((v->lfsr >> 0) ^ (v->lfsr >> 1)) & 1; v->lfsr = (v->lfsr >> 1) | (b << 14); }
        w = (v->lfsr & 1) ? 1 : -1;
      } else if (v->wave == 1) w = 4 * fabsf(v->phase - 0.5f) - 1;
      else w = v->phase < 0.5f ? 1 : -1;
      s += w * v->vol * (1 - k);
      v->t += dt;
    }
    s = s / (1 + 0.6f * fabsf(s)) * 1.3f;
    if (s > 1) s = 1;
    if (s < -1) s = -1;
    int16_t o = (int16_t)(s * 26000);
    out[f * 2] = o; out[f * 2 + 1] = o;
  }
}
