//Includes
#include <stdint.h>
#include "system.h"
#include <stdio.h>
#include "worms_rgb333.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>

//Defines
#define WAIT_UNITL_0(x) while(x != 0){}
#define WAIT_UNITL_1(x) while(x != 1){}

#define SCREEN_IDX1_W 640
#define SCREEN_IDX1_H 480
#define SCREEN_IDX4_W 320
#define SCREEN_IDX4_H 240
#define SCREEN_IDX4_W8 (SCREEN_IDX4_W/8)
//Dimensions of interest
#define SCREEN_RGB333_W 480
#define SCREEN_RGB333_H 256

#define gpu_p32 ((volatile uint32_t*)LPRS2_GPU_BASE)
#define palette_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x1000))
#define unpack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x400000))
#define pack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x600000))
#define unpack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x800000))
#define pack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xa00000))
#define unpack_rgb333_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xc00000))
#define joypad_p32 ((volatile uint32_t*)LPRS2_JOYPAD_BASE)

#define STEP 1
#define WORM_ANIM_DELAY 7

#define X_SIZE 17
#define Y_SIZE 9
#define BLOCK_SIZE 25

//Structures
//TODO ovde treba uvezati Nintendo kontroler
//Struct for controller
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

//Struct for registers
typedef struct {
    uint32_t m[SCREEN_IDX1_H][SCREEN_IDX1_W];
} bf_unpack_idx1;
#define unpack_idx1 (*((volatile bf_unpack_idx1*)unpack_idx1_p32))

//Struct for coordinates
typedef struct {
    uint16_t x;
    uint16_t y;
} point_t;

typedef enum {
    MENU_PHASE,
    GAME_PHASE,
} game_states_t;

//Structs for worm
typedef enum {
    WORM_IDLE,
    WORM_WALK_1,
    WORM_WALK_2,
    WORM_WALK_3,
    WORM_WALK_4,
    WORM_WALK_5,
    WORM_WALK_6,
    WORM_WALK_7,
} worm_anim_states_t;

typedef struct {
    worm_anim_states_t state;
    uint8_t delay_cnt;
} worm_anim_t;

typedef struct {
    point_t pos;
    worm_anim_t anim;
} worm_t;

typedef enum {
    BLOCK_PRESENT,
    BLOCK_DESTROYED
} block_state_t;

typedef enum {
    BLOCK_EMPTY,
    BLOCK_FIXED,
    BLOCK_DESTROYABLE
} block_type_t;

typedef struct {
    point_t pos;
    block_state_t state;
    block_type_t type;
} block_t;

//Struct for all elements
typedef struct {
    worm_t worm;
    game_states_t state;
    block_t matrix_of_blocks[Y_SIZE][X_SIZE];
} game_state_t;

//Functions
void draw_sprite_from_atlas_worms(uint16_t src_x, uint16_t src_y, uint16_t w, uint16_t h, uint16_t dst_x, uint16_t dst_y, char flag) {
    for(uint16_t y = 0; y < h; y++){
        for(uint16_t x = 0; x < w; x++){
            uint32_t src_idx;
            uint32_t dst_idx;
            uint16_t pixel;

            switch(flag){
                case 'w':
                    src_idx = (src_y+y)*worms_1_up__w + (src_x+x);
                    pixel = worms_1_up__p[src_idx];
                    break;
                case 'a':
                    src_idx = (src_y+y)*worms_1_left__w + (src_x+x);
                    pixel = worms_1_left__p[src_idx];
                    break;
                case 's':
                    src_idx = (src_y+y)*worms_1_down__w + (src_x+x);
                    pixel = worms_1_down__p[src_idx];
                    break;
                case 'd':
                    src_idx = (src_y+y)*worms_1_right__w + (src_x+x);
                    pixel = worms_1_right__p[src_idx];
                    break;
                default:
                    break;
            }

            dst_idx = (dst_y+y)*SCREEN_RGB333_W + (dst_x+x);
            unpack_rgb333_p32[dst_idx] = pixel;
        }
    }
}

void draw_sprite_from_atlas_walls(uint16_t src_x, uint16_t src_y, uint16_t w, uint16_t h, uint16_t dst_x, uint16_t dst_y) {
    for(uint16_t y = 0; y < h; y++){
        for(uint16_t x = 0; x < w; x++){
            uint32_t src_idx = (src_y+y)*walls__w + (src_x+x);
            uint32_t dst_idx = (dst_y+y)*SCREEN_RGB333_W + (dst_x+x);
            uint16_t pixel = walls__p[src_idx];
            unpack_rgb333_p32[dst_idx] = pixel;
        }
    }
}

//TODO sredi ovu funkciju da ispisuje bas lepe blokove
void draw_map(uint16_t src_x, uint16_t src_y){
    //Upper and lower bound
    uint16_t dst_x_upper = 0;
    uint16_t dst_y_upper = 0;
    uint16_t dst_x_lower = 0;
    uint16_t dst_y_lower = 241;

    for(uint16_t z = 0; z < 20; z++){
        for(uint16_t y = 0; y < 16; y++){
            for(uint16_t x = 0; x < 25; x++){
                uint32_t src_idx = (src_y+y)*walls__w + (src_x+x);
                uint16_t pixel = walls__p[src_idx];

                //upper
                uint32_t dst_idx = (dst_y_upper+y)*SCREEN_RGB333_W + (dst_x_upper+x);
                unpack_rgb333_p32[dst_idx] = pixel;

                //lower
                dst_idx = (dst_y_lower+y)*SCREEN_RGB333_W + (dst_x_lower+x);
                unpack_rgb333_p32[dst_idx] = pixel;
            }
        }
        dst_x_upper += 25;
        dst_x_lower += 25;
    }

    //Left and right bound
    uint16_t dst_x_left  = 10;
    uint16_t dst_y_left  = 16;
    uint16_t dst_x_right = 480 - 27;
    uint16_t dst_y_right = 16;

    for(uint16_t z = 0; z < 9; z++){
        for(uint16_t y = 0; y < 25; y++){
            for(uint16_t x = 0; x < 16; x++){
                uint32_t src_idx = (src_y+y)*walls__w + (src_x+x);
                uint16_t pixel = walls__p[src_idx];

                //left
                uint32_t dst_idx = (dst_y_left+y)*SCREEN_RGB333_W + (dst_x_left+x);
                unpack_rgb333_p32[dst_idx] = pixel;

                //right
                dst_idx = (dst_y_right+y)*SCREEN_RGB333_W + (dst_x_right+x);
                unpack_rgb333_p32[dst_idx] = pixel;
            }
        }

        dst_y_left  += 25;
        dst_y_right += 25;
    }

    uint16_t dst_x_upper_pom = 0;
    uint16_t dst_y_upper_pom = 0;
    uint16_t dst_x_lower_pom = 480 - 12;
    uint16_t dst_y_lower_pom = 0;

    for(uint16_t y = 0; y < 257; y++) {
        for (uint16_t x = 0; x < 12; x++) {
            uint32_t dst_idx_pom = (dst_y_upper_pom + y) * SCREEN_RGB333_W + (dst_x_upper_pom + x);
            unpack_rgb333_p32[dst_idx_pom] = 910;

            dst_idx_pom = (dst_y_lower_pom + y) * SCREEN_RGB333_W + (dst_x_lower_pom + x);
            unpack_rgb333_p32[dst_idx_pom] = 910;
        }
    }
}

void draw_matrix_of_blocks(uint16_t src_x, uint16_t src_y, block_t matrix_of_blocks[Y_SIZE][X_SIZE]){
    //treba da prodjem kroz celu matricu i da crtam blokove kako dolikuje

    for(uint16_t a = 0; a < Y_SIZE; a++){
        for(uint16_t b = 0; b < X_SIZE; b++) {
            if (matrix_of_blocks[a][b].type == BLOCK_FIXED) {
                uint32_t dst_y = matrix_of_blocks[a][b].pos.y;
                uint32_t dst_x = matrix_of_blocks[a][b].pos.x;

                //Draw block
                for (uint16_t y = 0; y < 25; y++) {
                    for (uint16_t x = 0; x < 25; x++) {
                        uint32_t src_idx = (src_y + y) * walls__w + (src_x + x);
                        uint32_t dst_idx = (dst_y + y) * SCREEN_RGB333_W + (dst_x + x);
                        uint16_t pixel = walls__p[src_idx];
                        unpack_rgb333_p32[dst_idx] = pixel;
                    }
                }
            }
        }
    }
}

int check_movement(worm_t worm, char pravac, block_t matrix_of_blocks[Y_SIZE][X_SIZE]){
    uint16_t tol = 25;
    uint16_t flag = 1;

    switch(pravac){
        case 'w':
            for(uint16_t a = 0; a < Y_SIZE; a++) {
                for (uint16_t b = 0; b < X_SIZE; b++) {
                    //TODO iskuliraj sve blokove sa vecim y
                    if(matrix_of_blocks[a][b].pos.y < worm.pos.y ){
                        //TODO gledaj samo blokove koji su u x okolini
                        if( abs(matrix_of_blocks[a][b].pos.x - worm.pos.x) < tol ){
                            //TODO gledaj samo fixed blokove
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED){
                                if( (worm.pos.y - 1) - (matrix_of_blocks[a][b].pos.y + 25) < 0){
                                    flag &= 0;
                                }else{
                                    flag &= 1;
                                }
                            }
                        }
                    }
                }
            }

            return flag;
            break;
        case 'a':
            for(uint16_t a = 0; a < Y_SIZE; a++) {
                for (uint16_t b = 0; b < X_SIZE; b++) {
                    //TODO iskuliraj sve blokove sa vecim x
                    if(matrix_of_blocks[a][b].pos.x < worm.pos.x ){
                        //TODO gledaj samo blokove koji su u y okolini
                        if( abs(matrix_of_blocks[a][b].pos.y - worm.pos.y) < tol ){
                            //TODO gledaj samo fixed blokove
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED){
                                if( (worm.pos.x - 1) - (matrix_of_blocks[a][b].pos.x + 25) < 0){
                                    flag &= 0;
                                }else{
                                    flag &= 1;
                                }
                            }
                        }
                    }
                }
            }

            return flag;
            break;
        case 's':
            for(uint16_t a = 0; a < Y_SIZE; a++) {
                for (uint16_t b = 0; b < X_SIZE; b++) {
                    //TODO iskuliraj sve blokove sa manjim y
                    if(matrix_of_blocks[a][b].pos.y > worm.pos.y ){
                        //TODO gledaj samo blokove koji su u x okolini
                        if( abs(matrix_of_blocks[a][b].pos.x - worm.pos.x) < tol ){
                            //TODO gledaj samo fixed blokove
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED){
                                if( (worm.pos.y + 1 + 25) - (matrix_of_blocks[a][b].pos.y) > 0){
                                    flag &= 0;
                                }else{
                                    flag &= 1;
                                }
                            }
                        }
                    }
                }
            }

            return flag;
            break;
        case 'd':
            for(uint16_t a = 0; a < Y_SIZE; a++) {
                for (uint16_t b = 0; b < X_SIZE; b++) {
                    //TODO iskuliraj sve blokove sa manjim x
                    if(matrix_of_blocks[a][b].pos.x > worm.pos.x ){
                        //TODO gledaj samo blokove koji su u y okolini
                        if( abs(matrix_of_blocks[a][b].pos.y - worm.pos.y) < tol ){
                            //TODO gledaj samo fixed blokove
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED){
                                if( (worm.pos.x + 1 + 25) - (matrix_of_blocks[a][b].pos.x) > 0){
                                    flag &= 0;
                                }else{
                                    flag &= 1;
                                }
                            }
                        }
                    }
                }
            }

            return flag;
            break;
        default:
            break;
    }
}

//Game
int main(void) {
    //Basic settings
    srand(time(0));

    gpu_p32[0] = 3;                                                                                                     // RGB333 mode
    gpu_p32[0x800] = 0x00ff00ff;                                                                                        // Magenta for HUD

    uint16_t x_offset = 27;
    uint16_t y_offset = 16;

    // Game state.
    game_state_t gs;

    //Worm settings
    gs.worm.pos.x = x_offset;
    gs.worm.pos.y = y_offset;
    gs.worm.anim.state = WORM_IDLE;
    gs.worm.anim.delay_cnt = 0;

    gs.state = GAME_PHASE;

    //Matrix fullfilling
    for(uint16_t a = 0; a < Y_SIZE; a++){
        for(uint16_t b = 0; b < X_SIZE; b++) {
            gs.matrix_of_blocks[a][b].pos.x = b * BLOCK_SIZE + x_offset;
            gs.matrix_of_blocks[a][b].pos.y = a * BLOCK_SIZE + y_offset;

            if (a % 2 == 1 && b % 2 == 1) {
                gs.matrix_of_blocks[a][b].type = BLOCK_FIXED;
                gs.matrix_of_blocks[a][b].state = BLOCK_PRESENT;
            } else {
                gs.matrix_of_blocks[a][b].type = BLOCK_EMPTY;
                gs.matrix_of_blocks[a][b].state = BLOCK_DESTROYED;
            }
        }
    }

    char pravac = 'd';

    switch(gs.state){
        case GAME_PHASE:
            //Game loop
            while(1){
                int mov_x = 0;
                int mov_y = 0;

                //Movement in all directions
                if(joypad.up){
                    pravac = 'w';

                    if(check_movement(gs.worm, pravac, gs.matrix_of_blocks)){
                        mov_y = -1;
                    }
                }else if(joypad.left){
                    pravac = 'a';

                    if(check_movement(gs.worm, pravac, gs.matrix_of_blocks)){
                        mov_x = -1;
                    }
                }else if(joypad.down){
                    pravac = 's';

                    if(check_movement(gs.worm, pravac, gs.matrix_of_blocks)){
                        mov_y = +1;
                    }
                }else if(joypad.right){
                    pravac = 'd';

                    if(check_movement(gs.worm, pravac, gs.matrix_of_blocks)){
                        mov_x = +1;
                    }
                }

                //Out of bounds fix
                //24 - worm width, 27 frame width
                if(gs.worm.pos.x + mov_x*STEP > SCREEN_RGB333_W - 24 - 28){
                    gs.worm.pos.x = SCREEN_RGB333_W - 24 - 28;
                }else if(gs.worm.pos.x + mov_x*STEP < 28){
                    gs.worm.pos.x = 27;
                }else{
                    gs.worm.pos.x += mov_x*STEP;
                }

                //25 worm height, 16 frame height
                if(gs.worm.pos.y + mov_y*STEP > SCREEN_RGB333_H - 25 - 15){
                    gs.worm.pos.y = SCREEN_RGB333_H - 25 - 15;
                }else if(gs.worm.pos.y + mov_y*STEP < 16){
                    gs.worm.pos.y = 16;
                }else{
                    gs.worm.pos.y += mov_y*STEP;
                }

                //State machine
                switch(gs.worm.anim.state){
                    case WORM_IDLE:
                        if(mov_x != 0 || mov_y != 0){
                            gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                            gs.worm.anim.state = WORM_WALK_1;
                        }
                        break;
                    case WORM_WALK_1:
                        if(gs.worm.anim.delay_cnt != 0){
                            gs.worm.anim.delay_cnt--;
                        }else{
                            if(mov_x != 0 || mov_y != 0){
                                gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                                gs.worm.anim.state = WORM_WALK_2;
                            }else{
                                gs.worm.anim.state = WORM_IDLE;
                            }
                        }
                        break;
                    case WORM_WALK_2:
                        if(gs.worm.anim.delay_cnt != 0){
                            gs.worm.anim.delay_cnt--;
                        }else{
                            if(mov_x != 0 || mov_y != 0){
                                gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                                gs.worm.anim.state = WORM_WALK_3;
                            }else{
                                gs.worm.anim.state = WORM_WALK_1;
                            }
                        }
                        break;
                    case WORM_WALK_3:
                        if(gs.worm.anim.delay_cnt != 0){
                            gs.worm.anim.delay_cnt--;
                        }else{
                            if(mov_x != 0 || mov_y != 0){
                                gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                                gs.worm.anim.state = WORM_WALK_4;
                            }else{
                                gs.worm.anim.state = WORM_WALK_2;
                            }
                        }
                        break;
                    case WORM_WALK_4:
                        if(gs.worm.anim.delay_cnt != 0){
                            gs.worm.anim.delay_cnt--;
                        }else{
                            if(mov_x != 0 || mov_y != 0){
                                gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                                gs.worm.anim.state = WORM_WALK_5;
                            }else{
                                gs.worm.anim.state = WORM_WALK_3;
                            }
                        }
                        break;
                    case WORM_WALK_5:
                        if(gs.worm.anim.delay_cnt != 0){
                            gs.worm.anim.delay_cnt--;
                        }else{
                            if(mov_x != 0 || mov_y != 0){
                                gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                                gs.worm.anim.state = WORM_WALK_6;
                            }else{
                                gs.worm.anim.state = WORM_WALK_2;
                            }
                        }
                        break;
                    case WORM_WALK_6:
                        if(gs.worm.anim.delay_cnt != 0){
                            gs.worm.anim.delay_cnt--;
                        }else{
                            if(mov_x != 0 || mov_y != 0){
                                gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                                gs.worm.anim.state = WORM_WALK_7;
                            }else{
                                gs.worm.anim.state = WORM_WALK_1;
                            }
                        }
                        break;
                    case WORM_WALK_7:
                        if(gs.worm.anim.delay_cnt != 0){
                            gs.worm.anim.delay_cnt--;
                        }else{
                            gs.worm.anim.delay_cnt = WORM_ANIM_DELAY;
                            gs.worm.anim.state = WORM_IDLE;
                        }
                        break;
                }

                // Detecting rising edge of VSync.
                WAIT_UNITL_0(gpu_p32[2]);
                WAIT_UNITL_1(gpu_p32[2]);
                // Draw in buffer while it is in VSync.

                //Drawing
                //Draw black background
                for(uint16_t r = 0; r < SCREEN_RGB333_H; r++){
                    for(uint16_t c = 0; c < SCREEN_RGB333_W; c++){
                        unpack_rgb333_p32[r*SCREEN_RGB333_W + c] = 0000;
                    }
                }

                //Draw blocks
                draw_map(0, 0);
                draw_matrix_of_blocks(0, 0, gs.matrix_of_blocks);

                //Draw worm
                switch(gs.worm.anim.state){
                    case WORM_IDLE:
                        switch(pravac){
                            case 'w':
                                draw_sprite_from_atlas_worms(696 - 33, 8, 25, 20, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'a':
                                draw_sprite_from_atlas_worms(9, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 's':
                                draw_sprite_from_atlas_worms(696 - 33, 1920 - 28, 25, 20, gs.worm.pos.x, gs.worm.pos.y, 's');
                                break;
                            case 'd':
                                draw_sprite_from_atlas_worms(1920 - 28, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            default:
                                break;
                        }
                        break;
                    case WORM_WALK_1:
                    case WORM_WALK_7:
                        switch(pravac){
                            case 'w':
                                draw_sprite_from_atlas_worms(696 - 33, 45, 25, 20, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'a':
                                //TODO ispitaj dal' 46 ili 45
                                draw_sprite_from_atlas_worms(46, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 's':
                                draw_sprite_from_atlas_worms(696 - 33, 1920 - 65, 25, 20, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'd':
                                draw_sprite_from_atlas_worms(1920 - 65, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            default:
                                break;
                        }
                        break;
                    case WORM_WALK_2:
                    case WORM_WALK_6:
                        switch(pravac){
                            case 'w':
                                draw_sprite_from_atlas_worms(696 - 33, 85, 25, 20, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'a':
                                draw_sprite_from_atlas_worms(85, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 's':
                                draw_sprite_from_atlas_worms(696 - 33, 1920 - 101, 25, 20, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'd':
                                draw_sprite_from_atlas_worms(1920 - 101, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            default:
                                break;
                        }
                        break;
                    case WORM_WALK_3:
                    case WORM_WALK_5:
                        switch(pravac){
                            case 'w':
                                draw_sprite_from_atlas_worms(696 - 33, 120, 25, 20, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'a':
                                draw_sprite_from_atlas_worms(120, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 's':
                                draw_sprite_from_atlas_worms(696 - 33, 1920 - 140, 25, 20, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'd':
                                draw_sprite_from_atlas_worms(1920 - 139, 9, 20, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            default:
                                break;
                        }
                        break;
                    case WORM_WALK_4:
                        switch(pravac){
                            case 'w':
                                draw_sprite_from_atlas_worms(696 - 33, 155, 25, 24, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'a':
                                draw_sprite_from_atlas_worms(155, 9, 24, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 's':
                                draw_sprite_from_atlas_worms(696 - 33, 1920 - 179, 25, 24, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            case 'd':
                                draw_sprite_from_atlas_worms(1920 - 179, 9, 24, 25, gs.worm.pos.x, gs.worm.pos.y, pravac);
                                break;
                            default:
                                break;
                        }
                        break;
                }
            }
            break;
        case MENU_PHASE:
            break;
    }

    return 0;
}