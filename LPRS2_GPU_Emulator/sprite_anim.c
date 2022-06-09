#include <stdint.h>
#include "system.h"
#include <stdio.h>
#include "sprites_rgb333.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define WAIT_UNITL_0(x) while(x != 0){}
#define WAIT_UNITL_1(x) while(x != 1){}

#define SCREEN_IDX1_W 640
#define SCREEN_IDX1_H 480
#define SCREEN_IDX4_W 320
#define SCREEN_IDX4_H 240
#define SCREEN_RGB333_W 160
#define SCREEN_RGB333_H 120

#define SCREEN_IDX4_W8 (SCREEN_IDX4_W/8)

#define gpu_p32 ((volatile uint32_t*)LPRS2_GPU_BASE)
#define palette_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x1000))
#define unpack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x400000))
#define pack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x600000))
#define unpack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x800000))
#define pack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xa00000))
#define unpack_rgb333_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xc00000))
#define joypad_p32 ((volatile uint32_t*)LPRS2_JOYPAD_BASE)

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

typedef struct {
	uint32_t m[SCREEN_IDX1_H][SCREEN_IDX1_W];
} bf_unpack_idx1;
#define unpack_idx1 (*((volatile bf_unpack_idx1*)unpack_idx1_p32))

#define STEP 1

#define PACMAN_ANIM_DELAY 3

//Structures
typedef struct {
	uint16_t x;
	uint16_t y;
} point_t;

//Structs for pacman
typedef enum {
	PACMAN_IDLE,
	PACMAN_OPENING_MOUTH,
	PACMAN_WITH_OPEN_MOUTH,
	PACMAN_CLOSING_MOUTH,
	PACMAN_WITH_CLOSED_MOUTH
} pacman_anim_states_t;

typedef struct {
	pacman_anim_states_t state;
	uint8_t delay_cnt;
} pacman_anim_t;

typedef struct {
	point_t pos;
	pacman_anim_t anim;
} pacman_t;

//Structs for food
typedef enum {
    FOOD_PRESENT,
    FOOD_EATEN
} food_states_t;

typedef struct{
    point_t pos;
    food_states_t state;
}food_t;

//Structs for enemies
typedef struct{
    point_t pos;
}enemies_t;

//Struct for all players
typedef struct {
	pacman_t pacman;
    food_t strawberry;
    food_t mushroom;
    food_t cherry;
    food_t watermelon;
    enemies_t turquoise_enemy;
    enemies_t cream_enemy;
} game_state_t;

void draw_sprite_from_atlas(uint16_t src_x, uint16_t src_y, uint16_t w, uint16_t h, uint16_t dst_x, uint16_t dst_y) {
	for(uint16_t y = 0; y < h; y++){
		for(uint16_t x = 0; x < w; x++){
			uint32_t src_idx = (src_y+y)*Pacman_Sprite_Map__w + (src_x+x);
			uint32_t dst_idx = (dst_y+y)*SCREEN_RGB333_W + (dst_x+x);
			uint16_t pixel = Pacman_Sprite_Map__p[src_idx];
			unpack_rgb333_p32[dst_idx] = pixel;
		}
	}
}

// Game
int main(void) {
    srand(time(0));
    int strawberries_eaten = 0;
    int mushrooms_eaten = 0;
    int cherries_eaten = 0;
    int watermelons_eaten = 0;

	gpu_p32[0] = 3;                 // RGB333 mode.
	gpu_p32[0x800] = 0x00ff00ff;    // Magenta for HUD.

	// Game state.
	game_state_t gs;
	gs.pacman.pos.x = 0;
	gs.pacman.pos.y = 0;
	gs.pacman.anim.state = PACMAN_IDLE;
	gs.pacman.anim.delay_cnt = 0;

    //TODO random generisanje pozicija hrane i neprijatelja
    //160x120
    gs.strawberry.pos.x = rand() % 145;
    gs.strawberry.pos.y = rand() % 105;
    gs.strawberry.state = FOOD_PRESENT;

    gs.mushroom.pos.x = rand() % 145;
    gs.mushroom.pos.y = rand() % 105;
    gs.mushroom.state = FOOD_PRESENT;

    gs.cherry.pos.x = rand() % 145;
    gs.cherry.pos.y = rand() % 105;
    gs.cherry.state = FOOD_PRESENT;

    gs.watermelon.pos.x = rand() % 145;
    gs.watermelon.pos.y = rand() % 105;
    gs.watermelon.state = FOOD_PRESENT;

    gs.turquoise_enemy.pos.x = rand() % 145;
    gs.turquoise_enemy.pos.y = rand() % 105;

    gs.cream_enemy.pos.x = rand() % 145;
    gs.cream_enemy.pos.y = rand() % 105;

    char pravac = 'x';

	while(1){
        //Uslov za kraj igre
        if(strawberries_eaten == 1 && mushrooms_eaten == 1 && cherries_eaten == 1 && watermelons_eaten == 1){
            printf("pacman has passed level 1 :)\n");
            for(;;){}
            //TODO hteo sam staviti ovde return ali ne radi ovde???
        }

		int mov_x = 0;
		int mov_y = 0;

        //TODO kretanje u svim pravcima
        if(joypad.up){
            mov_y = -1;
            pravac = 'w';
        }
        if(joypad.left){
            mov_x = -1;
            pravac = 'a';
        }
        if(joypad.down){
            mov_y = +1;
            pravac = 's';
        }
        if(joypad.right){
            mov_x = +1;
            pravac = 'd';
        }

        //TODO ogranicenje izlaska van opsega
        if(gs.pacman.pos.x + mov_x*STEP > SCREEN_RGB333_W - 16){
            gs.pacman.pos.x = SCREEN_RGB333_W - 16;
        }else if(gs.pacman.pos.x + mov_x*STEP < 0){
            gs.pacman.pos.x = 0;
        }else{
            gs.pacman.pos.x += mov_x*STEP;
        }

        if(gs.pacman.pos.y + mov_y*STEP > SCREEN_RGB333_H - 16){
            gs.pacman.pos.y = SCREEN_RGB333_H - 16;
        }else if(gs.pacman.pos.y + mov_y*STEP < 0){
            gs.pacman.pos.y = 0;
        }else{
            gs.pacman.pos.y += mov_y*STEP;
        }

        //TODO funkcionalnost koja obavlja pacmanovu ishranu
        int x1;
        int y1;
        double d;

        //Enemies
        x1 = gs.pacman.pos.x - gs.turquoise_enemy.pos.x;
        y1 = gs.pacman.pos.y - gs.turquoise_enemy.pos.y;
        d = sqrt(pow(x1, 2) + pow(y1, 2));

        if(d < 10){
            printf("pacman has died :(\n");
            for(;;){}
        }

        x1 = gs.pacman.pos.x - gs.cream_enemy.pos.x;
        y1 = gs.pacman.pos.y - gs.cream_enemy.pos.y;
        d = sqrt(pow(x1, 2) + pow(y1, 2));

        if(d < 10){
            printf("pacman has died :(\n");
            for(;;){}
        }

        //Strawberry
        if(gs.strawberry.state == FOOD_PRESENT){
            x1 = gs.pacman.pos.x - gs.strawberry.pos.x;
            y1 = gs.pacman.pos.y - gs.strawberry.pos.y;
            d = sqrt(pow(x1, 2) + pow(y1, 2));

            if(d < 10){
                gs.strawberry.state = FOOD_EATEN;
                strawberries_eaten++;

                if(strawberries_eaten == 1) {
                    printf("pacman has eaten a strawberry\n");
                }
            }
        }

        //Mushroom
        if(gs.mushroom.state == FOOD_PRESENT){
            x1 = gs.pacman.pos.x - gs.mushroom.pos.x;
            y1 = gs.pacman.pos.y - gs.mushroom.pos.y;
            d = sqrt(pow(x1, 2) + pow(y1, 2));

            if(d < 10){
                gs.mushroom.state = FOOD_EATEN;

                mushrooms_eaten++;

                if(mushrooms_eaten == 1) {
                    printf("pacman has eaten a mushroom\n");
                }
            }
        }

        //Cherry
        if(gs.cherry.state == FOOD_PRESENT){
            x1 = gs.pacman.pos.x - gs.cherry.pos.x;
            y1 = gs.pacman.pos.y - gs.cherry.pos.y;
            d = sqrt(pow(x1, 2) + pow(y1, 2));

            if(d < 10){
                gs.cherry.state = FOOD_EATEN;

                cherries_eaten++;

                if(cherries_eaten == 1) {
                    printf("pacman has eaten a cherry\n");
                }
            }
        }

        //Watermelon
        if(gs.watermelon.state == FOOD_PRESENT){
            x1 = gs.pacman.pos.x - gs.watermelon.pos.x;
            y1 = gs.pacman.pos.y - gs.watermelon.pos.y;
            d = sqrt(pow(x1, 2) + pow(y1, 2));

            if(d < 10){
                gs.watermelon.state = FOOD_EATEN;

                watermelons_eaten++;

                if(watermelons_eaten == 1) {
                    printf("pacman has eaten a watermelon\n");
                }
            }
        }

		switch(gs.pacman.anim.state){
		case PACMAN_IDLE:
			if(mov_x != 0 || mov_y != 0){
				gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
				gs.pacman.anim.state = PACMAN_WITH_OPEN_MOUTH;
			}
			break;
		case PACMAN_OPENING_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
				gs.pacman.anim.delay_cnt--;
			}else{
				gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
				gs.pacman.anim.state = PACMAN_WITH_OPEN_MOUTH;
			}
			break;
		case PACMAN_WITH_OPEN_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
				gs.pacman.anim.delay_cnt--;
			}else{
				if(mov_x != 0 || mov_y != 0){
					gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
					gs.pacman.anim.state = PACMAN_CLOSING_MOUTH;
				}else{
					gs.pacman.anim.state = PACMAN_IDLE;
				}
			}
			break;
		case PACMAN_CLOSING_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
				gs.pacman.anim.delay_cnt--;
			}else{
				gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
				gs.pacman.anim.state = PACMAN_WITH_CLOSED_MOUTH;
			}
			break;
		case PACMAN_WITH_CLOSED_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
				gs.pacman.anim.delay_cnt--;
			}else{
				if(mov_x != 0 || mov_y != 0){
					gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
					gs.pacman.anim.state = PACMAN_OPENING_MOUTH;
				}else{
					gs.pacman.anim.state = PACMAN_IDLE;
				}
			}
			break;
		}

		// Drawing.
		// Detecting rising edge of VSync.
		WAIT_UNITL_0(gpu_p32[2]);
		WAIT_UNITL_1(gpu_p32[2]);
		// Draw in buffer while it is in VSync.

		// Black background.
		for(uint16_t r = 0; r < SCREEN_RGB333_H; r++){
			for(uint16_t c = 0; c < SCREEN_RGB333_W; c++){
				unpack_rgb333_p32[r*SCREEN_RGB333_W + c] = 0000;
			}
		}

		// Draw pacman.
		switch(gs.pacman.anim.state){
		case PACMAN_IDLE:
		case PACMAN_OPENING_MOUTH:
		case PACMAN_CLOSING_MOUTH:
			// Half open mouth.
            switch(pravac) {
                case 'w':
                    draw_sprite_from_atlas(16, 32, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;

                case 'a':
                    draw_sprite_from_atlas(16, 16, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;

                case 's':
                    draw_sprite_from_atlas(16, 48, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;

                case 'd':
                    draw_sprite_from_atlas(16, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;
                default:
                    draw_sprite_from_atlas(16, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;
            }
            break;
		case PACMAN_WITH_OPEN_MOUTH:
			// Full open mouth.
            switch(pravac) {
                case 'w':
                    draw_sprite_from_atlas(0, 32, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;

                case 'a':
                    draw_sprite_from_atlas(0, 16, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;

                case 's':
                    draw_sprite_from_atlas(0, 48, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;

                case 'd':
                    draw_sprite_from_atlas(0, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;
                default:
                    draw_sprite_from_atlas(0, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
                    break;
            }
            break;
		case PACMAN_WITH_CLOSED_MOUTH:
			// Close mouth.
			draw_sprite_from_atlas(32, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
			break;
		}

        //Draw Enemies
        draw_sprite_from_atlas(66, 98, 16, 14, gs.turquoise_enemy.pos.x, gs.turquoise_enemy.pos.y);

        draw_sprite_from_atlas(98, 114, 16, 14, gs.cream_enemy.pos.x, gs.cream_enemy.pos.y);

        //Draw food
        //Strawberry
        if(gs.strawberry.state == FOOD_PRESENT){
            draw_sprite_from_atlas(16, 176, 16, 14, gs.strawberry.pos.x, gs.strawberry.pos.y);
        }

        //Mushroom
        if(gs.mushroom.state == FOOD_PRESENT){
            draw_sprite_from_atlas(96, 224, 16, 16, gs.mushroom.pos.x, gs.mushroom.pos.y);
        }

        //Cherry
        if(gs.cherry.state == FOOD_PRESENT){
            draw_sprite_from_atlas(0, 176, 16, 16, gs.cherry.pos.x, gs.cherry.pos.y);
        }

        //Watermelon
        if(gs.watermelon.state == FOOD_PRESENT){
            draw_sprite_from_atlas(0, 224, 18, 14, gs.watermelon.pos.x, gs.watermelon.pos.y);
        }
	}

	return 0;
}
