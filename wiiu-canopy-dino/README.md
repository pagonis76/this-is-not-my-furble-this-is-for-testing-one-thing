# Canopy Dino (Wii U)

An original pixel-art platformer starring Dino (white egg body, green triangle spikes, floating
hands). Walk around the island map from Dino's house to 6 levels: Jungle Floor, Canopy Climb,
Misty Falls, Old Temple, Beetle Cavern and Sky Bridge. Clearing a level opens the next one.
Stomp or headbutt beetles and bees, grab bananas (50 bananas = 1 extra life).
? boxes (jump into them from below, or headbutt them) hold 5 bananas or a power-up:
wings (double jump) or a sword (X/Y swings it). Getting hit loses the power-up instead of a life.
Progress saves to `SD card/wiiu/apps/CanopyDino/save.txt`.

## Put it on the Wii U (Aroma)
Copy `CanopyDino.wuhb` to `SD card/wiiu/apps/`, put the card back and start "Canopy Dino" from the Wii U Menu.
In Cemu: File > Load and pick `CanopyDino.wuhb`.

## Controls
| | GamePad / Pro / Classic | Wii Remote sideways |
|---|---|---|
| Move | Left stick or D-pad | D-pad |
| Jump (hold = higher) | A or B | 2 |
| Headbutt dash (or sword swing) | X or Y | 1 |
| Wall-jump | Jump into a wall, then jump again | same |
| Map: walk / enter level | Stick or D-pad / A | D-pad / 2 |
| Pause | + | + |
| Back to the map | Pause, then Down | same |

Nunchuk works too (stick to move, A to jump, B to headbutt). HOME leaves the game.

## Build
Needs devkitPro with wut and wiiu-sdl2 (or the `devkitpro/devkitppc` Docker image):
`docker run --rm -v "$PWD":/src -w /src devkitpro/devkitppc make`

`src/game.c` is the whole game (art is drawn from text in the code, music and sounds are made
on the fly). `src/main.c` is the Wii U side. Levels are the `L1`/`L2`/`L3` lists near the top
of game.c, so adding a level is just adding another list.
`test/video.c` records a gameplay video with an autopilot. `test/test.c` runs the game on a PC without a screen and saves screenshots
(`gcc -o kiptest test/test.c -lm && ./kiptest .`).
