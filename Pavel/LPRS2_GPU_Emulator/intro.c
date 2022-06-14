#include <stdint.h>
#include "system.h"
#include <stdio.h>
#include <math.h>

#define WAIT_UNITL_0(x) while(x != 0){}
#define WAIT_UNITL_1(x) while(x != 1){}

#define SCREEN_W 640
#define SCREEN_H 480

//Pointers
#define gpu_p32 ((volatile uint32_t*)LPRS2_GPU_BASE)
#define palette_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x1000))
#define unpack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x400000))
#define pack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x600000))
#define joypad_p32 ((volatile uint32_t*)LPRS2_JOYPAD_BASE)

//Bit field - joypad
typedef struct {
	unsigned a      : 1;
	unsigned b      : 1;
	unsigned z      : 1;
	unsigned start  : 1;
	unsigned up     : 1;
	unsigned down   : 1;
	unsigned left   : 1;
	unsigned right  : 1;
} bf_joypad;
#define joypad (*((volatile bf_joypad*)LPRS2_JOYPAD_BASE))

//Bit field - unpacked
typedef struct {
	uint32_t m[SCREEN_H][SCREEN_W];
} bf_unpack_idx1;
#define unpack_idx1 (*((volatile bf_unpack_idx1*)unpack_idx1_p32))

//Defines
#define STEP 32
#define RECT_H 64
#define RECT_W 128
#define SQ_A 256
#define CIRCLE_R 32
#define TRIANGLE_T 64

#define UNPACKED_0_PACKED_1 0

//Struct for coordinates
typedef struct {
	uint16_t x;
	uint16_t y;
} point_t;
typedef enum {RECT, SQ, CIRCLE, TRIANGLE} player_t;

//Struct game stats
typedef struct {
	point_t rect;
	point_t sq;
    point_t circle;
    point_t triangle;

    player_t active;
} game_state_t;


int main(void) {

	// Setup.
	gpu_p32[0] = 1;						//IDX1
	gpu_p32[1] = UNPACKED_0_PACKED_1;	//Unpacked or packed
	palette_p32[0] = 0x00ff0000; 		//Background.
	palette_p32[1] = 0x000000ff; 		//Players.
	gpu_p32[0x800] = 0x0000ff00; 		//HUD - Header

	// Game state.
	game_state_t gs;
	gs.rect.x = 0;
	gs.rect.y = 0;

	gs.sq.x = 128;
	gs.sq.y = 128;

    gs.circle.x = 64;
    gs.circle.y = 64;

    gs.triangle.x = 256;
    gs.triangle.y = 256;

	gs.active = RECT;

	int toggle_pre = 0;
    int toggle_sad = 0;

	//Loop
	while(1){
		// Poll controls
		int mov_x = 0;
		int mov_y = 0;
		if(joypad.down){
			mov_y = +1;
		}
		if(joypad.up){
			mov_y = -1;
		}
		if(joypad.right){
			mov_x = +1;
		}
		if(joypad.left){
			mov_x = -1;
		}

		//TODO softversko diferenciranje za taster 'A'
        //Kad je taster pritisnut a == 1
        //Kad je otpusten         a == 0
        //Detektuj prelaz sa 0 na 1
        //Tek onda je toggle_active == 1, menja se igrac
        int toggle_active = 0;

        toggle_sad = joypad.a;
        if(toggle_pre == 0 && toggle_sad == 1){
            toggle_active = 1;
        }

        toggle_pre = toggle_sad;

        //TODO ogranicavanje izlaska van opsega
        //Gameplay
        switch(gs.active){
            case RECT:
                //x
                if(gs.rect.x + mov_x*STEP > SCREEN_W - RECT_W){
                    gs.rect.x = SCREEN_W - RECT_W;
                }else if(gs.rect.x + mov_x*STEP < 0){
                    gs.rect.x = 0;
                }else{
                    gs.rect.x += mov_x*STEP;
                }

                //y
                if(gs.rect.y + mov_y*STEP > SCREEN_H - RECT_H){
                    gs.rect.y = SCREEN_H - RECT_H;
                }else if(gs.rect.y + mov_y*STEP < 0){
                    gs.rect.y = 0;
                }else{
                    gs.rect.y += mov_y*STEP;
                }

                //toggle
                if(toggle_active){
                    gs.active = SQ;
                }
                break;
            case SQ:
                //x
                if(gs.sq.x + mov_x*STEP > SCREEN_W - SQ_A){
                    gs.sq.x = SCREEN_W - SQ_A;
                }else if(gs.sq.x + mov_x*STEP < 0){
                    gs.sq.x = 0;
                }else{
                    gs.sq.x += mov_x*STEP;
                }

                //y
                if(gs.sq.y + mov_y*STEP > SCREEN_H - SQ_A){
                    gs.sq.y = SCREEN_H - SQ_A;
                }else if(gs.sq.y + mov_y*STEP < 0){
                    gs.sq.y = 0;
                }else{
                    gs.sq.y += mov_y*STEP;
                }

                //toggle
                if(toggle_active){
                    gs.active = CIRCLE;
                }
                break;
            case CIRCLE:
                //x
                if(gs.circle.x + mov_x*STEP > SCREEN_W - 2*CIRCLE_R){
                    gs.circle.x = SCREEN_W - 2*CIRCLE_R;
                }else if(gs.circle.x + mov_x*STEP < 0){
                    gs.circle.x = 0;
                }else{
                    gs.circle.x += mov_x*STEP;
                }

                //y
                if(gs.circle.y + mov_y*STEP > SCREEN_H - 2*CIRCLE_R){
                    gs.circle.y = SCREEN_H - 2*CIRCLE_R;
                }else if(gs.circle.y + mov_y*STEP < 0){
                    gs.circle.y = 0;
                }else{
                    gs.circle.y += mov_y*STEP;
                }

                //toggle
                if(toggle_active){
                    gs.active = TRIANGLE;
                }
                break;
            case TRIANGLE:
                //x
                if(gs.triangle.x + mov_x*STEP > SCREEN_W - TRIANGLE_T){
                    gs.triangle.x = SCREEN_W - TRIANGLE_T;
                }else if(gs.triangle.x + mov_x*STEP < 0){
                    gs.triangle.x = 0;
                }else{
                    gs.triangle.x += mov_x*STEP;
                }

                //y
                if(gs.triangle.y + mov_y*STEP > SCREEN_H - TRIANGLE_T/2){
                    gs.triangle.y = SCREEN_H - TRIANGLE_T/2;
                }else if(gs.triangle.y + mov_y*STEP < 0){
                    gs.triangle.y = 0;
                }else{
                    gs.triangle.y += mov_y*STEP;
                }

                //toggle
                if(toggle_active){
                    gs.active = RECT;
                }
                break;
		}

		// Drawing.
		// Detecting rising edge of VSync.
        // Draw in buffer while it is in VSync.
		WAIT_UNITL_0(gpu_p32[2]);
		WAIT_UNITL_1(gpu_p32[2]);

#if !UNPACKED_0_PACKED_1
        // TODO dodaj crtanje za krug i trougao
        // Unpacked.

        // Clear to blue.
		for(int r = 0; r < SCREEN_H; r++){
			for(int c = 0; c < SCREEN_W; c++){
				unpack_idx1_p32[r*SCREEN_W + c] = 0;
			}
		}

		// Red rectangle.
		// Use array with 2D indexing.
		for(int r = gs.rect.y; r < gs.rect.y+RECT_H; r++){
			for(int c = gs.rect.x; c < gs.rect.x+RECT_W; c++){
				unpack_idx1_p32[r*SCREEN_W + c] = 1;
			}
		}

		// Red square.
		// Use struct with 2D matrix.
		for(int r = gs.sq.y; r < gs.sq.y+SQ_A; r++){
			for(int c = gs.sq.x; c < gs.sq.x+SQ_A; c++){
				unpack_idx1.m[r][c] = 1;
			}
		}

        //Red circle
        int centar_x = gs.circle.x + CIRCLE_R;
        int centar_y = gs.circle.y + CIRCLE_R;

        for(int r = gs.circle.y; r < gs.circle.y + 2*CIRCLE_R; r++){
            for(int c = gs.circle.x; c < gs.circle.x + 2*CIRCLE_R; c++){
                int d = sqrt(pow((r - centar_y), 2) + pow((c - centar_x), 2));

                if(d < CIRCLE_R){
                    unpack_idx1.m[r][c] = 1;
                }
            }
        }

        //Red Triangle

        int plavi = 31;
        int brojac = 0;

        //Left part
        for(int r = gs.triangle.y; r < gs.triangle.y + TRIANGLE_T/2; r++){
            for(int c = gs.triangle.x; c < gs.triangle.x + TRIANGLE_T/2; c++){
                brojac++;

                if(brojac > plavi){
                    unpack_idx1.m[r][c] = 1;
                }
            }
            plavi--;
            brojac = 0;
        }

        plavi = 31;
        brojac = 32;

        //Right part
        for(int r = gs.triangle.y; r < gs.triangle.y + TRIANGLE_T/2; r++){
            for(int c = gs.triangle.x + TRIANGLE_T/2; c < gs.triangle.x + TRIANGLE_T; c++){
                if(brojac > plavi){
                    unpack_idx1.m[r][c] = 1;
                }

                brojac--;
            }
            plavi--;
            brojac = 32;
        }

#else
        //TODO crtanje na efikasniji nacin
		// Packed.

		//Blue
		for(int r = 0; r < SCREEN_H; r++){
			for(int c = 0; c < SCREEN_W/32; c++){
				pack_idx1_p32[r*(SCREEN_W/32) + c] = 0x0;
			}
		}

		//Rectangle
		for(int r = gs.rect.y; r < gs.rect.y + RECT_H; r++){
			for(int c = gs.rect.x/32; c < gs.rect.x/32 + 4; c++){
				pack_idx1_p32[r*(SCREEN_W/32) + c] = 0xffffffff;
			}
		}

        //Square
        for(int r = gs.sq.y; r < gs.sq.y + SQ_A; r++){
            for(int c = gs.sq.x/32; c < gs.sq.x/32 + 8; c++){
                pack_idx1_p32[r*(SCREEN_W/32) + c] = 0xffffffff;
            }
        }

#endif
}//End of the loop

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
