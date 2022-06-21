//Includes
#include <stdint.h>
#include "system.h"
#include <stdio.h>
#include "worms_rgb333.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>

//Global variables
int end_flag = 0;

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
#define BOMB_ANIM_DELAY 5
#define X_SIZE 15
#define Y_SIZE 7
#define BLOCK_SIZE 30
#define BOUND_BLOCK_SIZE 15
#define PROBABILITY 40
#define BOMB_TIME 90
#define NUMBER_OF_PLAYERS 2
#define START_DELAY 120

//Structures
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

//Game phases
typedef enum {
    START_PHASE,
    GAME_PHASE,
    END_PHASE,
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

typedef enum {
    WORM_PRESENT,
    WORM_NOT_PRESENT,
} worm_present_t;

typedef struct {
    point_t pos;
    worm_anim_t anim;
    worm_present_t presence;
} worm_t;

//Structs for blocks
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

//Structs for bomb
typedef enum {
    BOMB_PRESENT,
    BOMB_NOT_PRESENT
} bomb_state_t;

typedef struct {
    point_t pos;
    bomb_state_t state;
    int bomb_counter;
} bomb_t;

//Structs for explosion
typedef enum {
    EXPLOSION_IDLE,
    EXPLOSION_1,
    EXPLOSION_2,
    EXPLOSION_3,
    EXPLOSION_4,
    EXPLOSION_5,
    EXPLOSION_6,
    EXPLOSION_7
} explosion_state_t;

typedef enum {
    EXPLOSION_PRESENT,
    EXPLOSION_NOT_PRESENT,
} explosion_present_t;

typedef struct {
    point_t pos;
    explosion_state_t state;
    explosion_present_t presence;
    uint8_t delay_cnt;
} explosion_t;

typedef enum {
    BULLET_PRESENT,
    BULLET_NOT_PRESENT,
} bullet_present_t;

typedef struct {
    point_t pos;
    bullet_present_t presence;
} bullet_t;

//Struct for all participants
typedef struct {
    worm_t worm[NUMBER_OF_PLAYERS];
    bomb_t bomb[NUMBER_OF_PLAYERS];
    explosion_t explosion[NUMBER_OF_PLAYERS];
    bullet_t bullet[NUMBER_OF_PLAYERS];
    game_states_t state;
    block_t matrix_of_blocks[Y_SIZE][X_SIZE];
    uint16_t start_delay;
} game_state_t;


//Nintendo controller
int set_interface_attribs (int fd, int speed, int parity)
{
    struct termios tty;
    if (tcgetattr (fd, &tty) != 0)
    {
        //error_message ("error %d from tcgetattr", errno);
        printf("Prvi je problem\n");
        return -1;
    }

    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    //tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        //error_message ("error %d from tcsetattr", errno);
        printf("Drugi je problem\n");
        return -1;
    }
    return 0;
}

void set_blocking (int fd, int should_block)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        //error_message ("error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN]  = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    if (tcsetattr (fd, TCSANOW, &tty) != 0);
    //error_message ("error %d setting term attributes", errno);
}

char movement;
char pom_movement;

void* readDesc(void* par)
{
    char* port = "/dev/ttyUSB0";
    int desc = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (desc < 0)
    {
        //error_message ("error %d opening %s: %s", errno, port, strerror (errno));
        printf("Nije kreiran deskriptor za serijski port\n");
        return 0;
    }

    set_interface_attribs (desc, B9600, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    set_blocking (desc, 0);
    char b[1];
    while(1)
    {
        int p = read(desc, b, sizeof b);
        //printf("%c", b[0]);
        movement = b[0];
        //printf("%c", movement);
        pom_movement = movement;

        memset(&b, 0, sizeof b);
        //printf("%c", movement);
        usleep(10);

    }

}


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
                case 'b':
                    src_idx = (src_y+y)*worms_1_left__w + (src_x+x);
                    pixel = worms_1_left__p[src_idx];
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

void draw_map(uint16_t src_x, uint16_t src_y){
    //Upper and lower bound
    uint16_t dst_x_upper = 0;
    uint16_t dst_y_upper = 8;
    uint16_t dst_x_lower = 0;
    uint16_t dst_y_lower = 256 - BOUND_BLOCK_SIZE - 8;

    for(uint16_t z = 0; z < 32; z++){
        for(uint16_t y = 0; y < BOUND_BLOCK_SIZE; y++){
            for(uint16_t x = 0; x < BOUND_BLOCK_SIZE; x++){
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
        dst_x_upper += BOUND_BLOCK_SIZE;
        dst_x_lower += BOUND_BLOCK_SIZE;
    }

    //Left and right bound
    uint16_t dst_x_left  = 0;
    uint16_t dst_y_left  = 8;
    uint16_t dst_x_right = 480 - BOUND_BLOCK_SIZE;
    uint16_t dst_y_right = 8;

    //TODO jedan pixel dole je ostao nedirnut, ako bude potrebno, regulisi ovo
    for(uint16_t z = 0; z < 17; z++){
        for(uint16_t y = 0; y < BOUND_BLOCK_SIZE; y++){
            for(uint16_t x = 0; x < BOUND_BLOCK_SIZE; x++){
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

        dst_y_left  += BOUND_BLOCK_SIZE;
        dst_y_right += BOUND_BLOCK_SIZE;
    }

    uint16_t dst_x_upper_pom = 0;
    uint16_t dst_y_upper_pom = 0;
    uint16_t dst_x_lower_pom = 0;
    uint16_t dst_y_lower_pom = 256 - 8;

    for(uint16_t y = 0; y < 8; y++) {
        for (uint16_t x = 0; x < 480; x++) {
            uint32_t dst_idx_pom = (dst_y_upper_pom + y) * SCREEN_RGB333_W + (dst_x_upper_pom + x);
            unpack_rgb333_p32[dst_idx_pom] = 5;

            dst_idx_pom = (dst_y_lower_pom + y) * SCREEN_RGB333_W + (dst_x_lower_pom + x);
            unpack_rgb333_p32[dst_idx_pom] = 5;
        }
    }
}

void draw_matrix_of_blocks(uint16_t src_x, uint16_t src_y, uint16_t src_xR, uint16_t src_yR, block_t matrix_of_blocks[Y_SIZE][X_SIZE]){

    //draw_sprite_from_atlas_walls(3280, 172, 30, 30, 400, 160);

    for(uint16_t a = 0; a < Y_SIZE; a++){
        for(uint16_t b = 0; b < X_SIZE; b++) {
            if (matrix_of_blocks[a][b].type == BLOCK_FIXED) {
                uint32_t dst_y = matrix_of_blocks[a][b].pos.y;
                uint32_t dst_x = matrix_of_blocks[a][b].pos.x;

                //Draw block
                for (uint16_t y = 0; y < BLOCK_SIZE; y++) {
                    for (uint16_t x = 0; x < BLOCK_SIZE; x++) {
                        uint32_t src_idx = (src_y + y) * walls__w + (src_x + x);
                        uint32_t dst_idx = (dst_y + y) * SCREEN_RGB333_W + (dst_x + x);
                        uint16_t pixel = walls__p[src_idx];
                        unpack_rgb333_p32[dst_idx] = pixel;
                    }
                }
            }else if (matrix_of_blocks[a][b].type == BLOCK_EMPTY && matrix_of_blocks[a][b].state == BLOCK_PRESENT) {

                uint32_t dst_y = matrix_of_blocks[a][b].pos.y;
                uint32_t dst_x = matrix_of_blocks[a][b].pos.x;

                //Draw random block
                for (uint16_t y = 0; y < BLOCK_SIZE; y++) {
                    for (uint16_t x = 0; x < BLOCK_SIZE; x++) {
                        uint32_t src_idx = (src_yR + y) * walls__w + (src_xR + x);
                        uint32_t dst_idx = (dst_y + y) * SCREEN_RGB333_W + (dst_x + x);
                        uint16_t pixel = walls__p[src_idx];
                        unpack_rgb333_p32[dst_idx] = pixel;
                    }
                }
            }
        }
    }
}

void draw_sprite_from_atlas_explosion(uint16_t src_x, uint16_t src_y, uint16_t w, uint16_t h, uint16_t dst_x, uint16_t dst_y, uint16_t x_offset, uint16_t y_offset) {
    int nesto_x = dst_x - x_offset;
    int nesto_y = dst_y - y_offset;

    //TODO smanji sirinu i visinu iscrtavanja

    if(nesto_x < 15){
        dst_x = 15;
        src_x += abs(15 - nesto_x);
        w -= abs(15 - nesto_x);
    }else{
        dst_x -= x_offset;
    }

    if(nesto_y < 23){
        dst_y = 23;
        src_y += abs(23 - nesto_y);
        h -= abs(23 - nesto_y);
    }else{
        dst_y -= y_offset;
    }

    for(uint16_t y = 0; y < h; y++){
        for(uint16_t x = 0; x < w; x++) {
            uint32_t src_idx = (src_y + y) * explosion__w + (src_x + x);
            uint32_t dst_idx = (dst_y + y) * SCREEN_RGB333_W + (dst_x + x);

            uint16_t pixel = explosion__p[src_idx];
            unpack_rgb333_p32[dst_idx] = pixel;
        }
    }
}

void draw_notifications(uint16_t src_x, uint16_t src_y, uint16_t w, uint16_t h, uint16_t dst_x, uint16_t dst_y, int x){
    switch(x){
        case 0:
            for(uint16_t y = 0; y < h; y++){
                for(uint16_t x = 0; x < w; x++){
                    uint32_t src_idx = (src_y+y)*start_page__w + (src_x+x);
                    uint32_t dst_idx = (dst_y+y)*SCREEN_RGB333_W + (dst_x+x);
                    uint16_t pixel = start_page__p[src_idx];
                    unpack_rgb333_p32[dst_idx] = pixel;
                }
            }
            break;
        case 1:
            for(uint16_t y = 0; y < h; y++){
                for(uint16_t x = 0; x < w; x++){
                    uint32_t src_idx = (src_y+y)*one_won__w + (src_x+x);
                    uint32_t dst_idx = (dst_y+y)*SCREEN_RGB333_W + (dst_x+x);
                    uint16_t pixel = one_won__p[src_idx];
                    unpack_rgb333_p32[dst_idx] = pixel;
                }
            }
            break;
        case 2:
            for(uint16_t y = 0; y < h; y++){
                for(uint16_t x = 0; x < w; x++){
                    uint32_t src_idx = (src_y+y)*two_won__w + (src_x+x);
                    uint32_t dst_idx = (dst_y+y)*SCREEN_RGB333_W + (dst_x+x);
                    uint16_t pixel = two_won__p[src_idx];
                    unpack_rgb333_p32[dst_idx] = pixel;
                }
            }
            break;
    }
}

void draw_blocks(uint16_t src_x, uint16_t src_y, uint16_t w, uint16_t h, uint16_t dst_x, uint16_t dst_y) {
    for(uint16_t y = 0; y < h; y++){
        for(uint16_t x = 0; x < w; x++){
            uint32_t src_idx = (src_y+y)*blocks__w + (src_x+x);
            uint32_t dst_idx = (dst_y+y)*SCREEN_RGB333_W + (dst_x+x);
            uint16_t pixel = blocks__p[src_idx];
            unpack_rgb333_p32[dst_idx] = pixel;
        }
    }
}

int check_movement(worm_t worm, char direction, block_t matrix_of_blocks[Y_SIZE][X_SIZE]){
    uint16_t tol = BLOCK_SIZE;
    uint16_t flag = 1;

    switch(direction){
        case 'w':
            for(uint16_t a = 0; a < Y_SIZE; a++) {
                for (uint16_t b = 0; b < X_SIZE; b++) {
                    //TODO iskuliraj sve blokove sa manjim y
                    if(matrix_of_blocks[a][b].pos.y < worm.pos.y ){
                        //TODO gledaj samo blokove koji su u x okolini
                        if( abs(matrix_of_blocks[a][b].pos.x - worm.pos.x) < 30){
                            //TODO gledaj samo fixed blokove
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED || (matrix_of_blocks[a][b].type == BLOCK_EMPTY && matrix_of_blocks[a][b].state == BLOCK_PRESENT)){
                                if( ((worm.pos.y - 1) - (matrix_of_blocks[a][b].pos.y + BLOCK_SIZE) < 0) && (( (worm.pos.x + 25) - matrix_of_blocks[a][b].pos.x) > 0 ) && (( matrix_of_blocks[a][b].pos.x + BLOCK_SIZE - (worm.pos.x)) > 0 )){
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
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED || (matrix_of_blocks[a][b].type == BLOCK_EMPTY && matrix_of_blocks[a][b].state == BLOCK_PRESENT)){
                                if( ((worm.pos.x - 1) - (matrix_of_blocks[a][b].pos.x + BLOCK_SIZE) < 0) && (( (worm.pos.y + 25) - matrix_of_blocks[a][b].pos.y) > 0 ) && (( matrix_of_blocks[a][b].pos.y + BLOCK_SIZE - (worm.pos.y)) > 0 )){
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
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED || (matrix_of_blocks[a][b].type == BLOCK_EMPTY && matrix_of_blocks[a][b].state == BLOCK_PRESENT)){
                                if( ((worm.pos.y + 1 + 20) - (matrix_of_blocks[a][b].pos.y) > 0) && (( (worm.pos.x + 25) - matrix_of_blocks[a][b].pos.x) > 0 ) && (( matrix_of_blocks[a][b].pos.x + BLOCK_SIZE - (worm.pos.x)) > 0 ) ){
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
                            if(matrix_of_blocks[a][b].type == BLOCK_FIXED || (matrix_of_blocks[a][b].type == BLOCK_EMPTY && matrix_of_blocks[a][b].state == BLOCK_PRESENT)){
                                if( ((worm.pos.x + 1 + 20) - (matrix_of_blocks[a][b].pos.x) > 0) && (( (worm.pos.y + 25) - matrix_of_blocks[a][b].pos.y) > 0 ) && (( matrix_of_blocks[a][b].pos.y + BLOCK_SIZE - (worm.pos.y)) > 0 )){
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

    int p = 0;
    pthread_t thread1;
    pthread_create(&thread1, NULL, readDesc, (void*)&p);

    int toggle_pre[NUMBER_OF_PLAYERS] = {'x', 'x'};
    int toggle_sad[NUMBER_OF_PLAYERS] = {'x', 'x'};

    gpu_p32[0] = 3;                                                                                                     // RGB333 mode
    gpu_p32[0x800] = 0x00ff00ff;                                                                                        // Magenta for HUD

    uint16_t x_offset = BOUND_BLOCK_SIZE;
    uint16_t y_offset = BOUND_BLOCK_SIZE + 8;

    //TODO sumnjivo
    uint16_t x_offset_array[] = {BOUND_BLOCK_SIZE, SCREEN_RGB333_W - 30};
    uint16_t y_offset_array[] = {BOUND_BLOCK_SIZE + 8, SCREEN_RGB333_H - BOUND_BLOCK_SIZE - 32};

    //Game state settings
    game_state_t gs;
    gs.state = START_PHASE;
    gs.start_delay = START_DELAY;

    int i;
    for(i = 0; i < NUMBER_OF_PLAYERS; i++){
        //Worm settings
        gs.worm[i].presence = WORM_PRESENT;
        gs.worm[i].pos.x = x_offset_array[i];
        gs.worm[i].pos.y = y_offset_array[i];
        gs.worm[i].anim.state = WORM_IDLE;
        gs.worm[i].anim.delay_cnt = 0;

        //Bullet settings
        gs.bullet[i].presence = BULLET_NOT_PRESENT;

        //Bomb settings
        gs.bomb[i].state = BOMB_NOT_PRESENT;

        //Explosion settings
        gs.explosion[i].presence = EXPLOSION_NOT_PRESENT;
    }

    //Matrix fullfilling
    for(uint16_t a = 0; a < Y_SIZE; a++){
        for(uint16_t b = 0; b < X_SIZE; b++) {
            gs.matrix_of_blocks[a][b].pos.x = b * BLOCK_SIZE + x_offset;
            gs.matrix_of_blocks[a][b].pos.y = a * BLOCK_SIZE + y_offset;

            if (a % 2 == 1 && b % 2 == 1) {
                gs.matrix_of_blocks[a][b].type  = BLOCK_FIXED;
                gs.matrix_of_blocks[a][b].state = BLOCK_PRESENT;
            } else {
                gs.matrix_of_blocks[a][b].type  = BLOCK_EMPTY;
                gs.matrix_of_blocks[a][b].state = BLOCK_DESTROYED;
                uint16_t rnd = rand();
                if(rnd % 100 > PROBABILITY){
                    gs.matrix_of_blocks[a][b].state = BLOCK_PRESENT;
                }
            }
        }
    }
    //Empty blocks for worm start position
    gs.matrix_of_blocks[0][0].state = BLOCK_DESTROYED;
    gs.matrix_of_blocks[0][1].state = BLOCK_DESTROYED;
    gs.matrix_of_blocks[1][0].state = BLOCK_DESTROYED;

    gs.matrix_of_blocks[6][14].state = BLOCK_DESTROYED;
    gs.matrix_of_blocks[5][14].state = BLOCK_DESTROYED;
    gs.matrix_of_blocks[6][13].state = BLOCK_DESTROYED;

    char direction[NUMBER_OF_PLAYERS] = {'d', 'a'};

    int speed = 0;

    while(1) {

        //Game states
        switch (gs.state) {
            case GAME_PHASE:
                //Game loop
                while (1) {

                    if (speed == 22) {
                        movement = " ";
                        speed = 0;
                    }
                    speed++;

                    //Movement in all directions
                    int mov_x[NUMBER_OF_PLAYERS] = {0, 0};
                    int mov_y[NUMBER_OF_PLAYERS] = {0, 0};

                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        if (i == 0) {
                            if (joypad.up) {
                                direction[i] = 'w';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_y[i] = -1;
                                }
                            } else if (joypad.left) {
                                direction[i] = 'a';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_x[i] = -1;
                                }
                            } else if (joypad.down) {
                                direction[i] = 's';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_y[i] = +1;
                                }
                            } else if (joypad.right) {
                                direction[i] = 'd';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_x[i] = +1;
                                }
                            }
                        } else {
                            if (movement == 'U') {
                                direction[i] = 'w';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_y[i] = -1;
                                }
                            } else if (movement == 'L') {
                                direction[i] = 'a';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_x[i] = -1;
                                }
                            } else if (movement == 'D') {
                                direction[i] = 's';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_y[i] = +1;
                                }
                            } else if (movement == 'R') {
                                direction[i] = 'd';

                                if (check_movement(gs.worm[i], direction[i], gs.matrix_of_blocks)) {
                                    mov_x[i] = +1;
                                }
                            }
                        }
                    }


                    //Toggle fix when worm change direction
                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        toggle_sad[i] = direction[i];

                        if (toggle_pre[i] == 's' && toggle_sad[i] == 'a' ||
                            toggle_pre[i] == 's' && toggle_sad[i] == 'd') {
                            gs.worm[i].pos.y -= 5;
                        } else if (toggle_pre[i] == 'd' && toggle_sad[i] == 'w' ||
                                   toggle_pre[i] == 'd' && toggle_sad[i] == 's') {
                            gs.worm[i].pos.x -= 5;
                        }

                        toggle_pre[i] = toggle_sad[i];
                    }

                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        //Out of bounds fix
                        if (gs.worm[i].pos.x + mov_x[i] * STEP > SCREEN_RGB333_W - 20 - BOUND_BLOCK_SIZE) {
                            gs.worm[i].pos.x = SCREEN_RGB333_W - 20 - BOUND_BLOCK_SIZE;
                        } else if (gs.worm[i].pos.x + mov_x[i] * STEP < BOUND_BLOCK_SIZE) {
                            gs.worm[i].pos.x = BOUND_BLOCK_SIZE;
                        } else {
                            gs.worm[i].pos.x += mov_x[i] * STEP;
                        }

                        if (gs.worm[i].pos.y + mov_y[i] * STEP > SCREEN_RGB333_H - 20 - BOUND_BLOCK_SIZE - 8) {
                            gs.worm[i].pos.y = SCREEN_RGB333_H - 20 - BOUND_BLOCK_SIZE - 8;
                        } else if (gs.worm[i].pos.y + mov_y[i] * STEP < BOUND_BLOCK_SIZE + 8) {
                            gs.worm[i].pos.y = BOUND_BLOCK_SIZE + 8;
                        } else {
                            gs.worm[i].pos.y += mov_y[i] * STEP;
                        }
                    }


                    //Bomb functionality
                    uint16_t explosion_x[NUMBER_OF_PLAYERS];
                    uint16_t explosion_y[NUMBER_OF_PLAYERS];

                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        if (i == 0) {
                            if (gs.bomb[i].state == BOMB_NOT_PRESENT && joypad.b == 1) {
                                gs.bomb[i].state = BOMB_PRESENT;
                                gs.bomb[i].bomb_counter = BOMB_TIME;
                                for (uint16_t a = 0; a < Y_SIZE; a++) {
                                    for (uint16_t b = 0; b < X_SIZE; b++) {
                                        if (gs.worm[i].pos.x >= gs.matrix_of_blocks[a][b].pos.x) {
                                            if (gs.worm[i].pos.x < gs.matrix_of_blocks[a][b].pos.x + 30) {
                                                if (gs.worm[i].pos.y >= gs.matrix_of_blocks[a][b].pos.y) {
                                                    if (gs.worm[i].pos.y < gs.matrix_of_blocks[a][b].pos.y + 30) {
                                                        explosion_x[i] = gs.matrix_of_blocks[a][b].pos.x + 15;
                                                        explosion_y[i] = gs.matrix_of_blocks[a][b].pos.y + 15;
                                                        gs.bomb[i].pos.x = gs.matrix_of_blocks[a][b].pos.x + 9;
                                                        gs.bomb[i].pos.y = gs.matrix_of_blocks[a][b].pos.y + 4;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else if (gs.bomb[i].state == BOMB_PRESENT) {
                                gs.bomb[i].bomb_counter--;

                                if (gs.bomb[i].bomb_counter < 0) {
                                    gs.bomb[i].bomb_counter = 0;
                                    gs.bomb[i].state = BOMB_NOT_PRESENT;
                                    gs.explosion[i].presence = EXPLOSION_PRESENT;
                                    gs.explosion[i].pos.x = explosion_x[i];
                                    gs.explosion[i].pos.y = explosion_y[i];
                                }
                            }
                        } else {
                            if (gs.bomb[i].state == BOMB_NOT_PRESENT && movement == 'B') {
                                gs.bomb[i].state = BOMB_PRESENT;
                                gs.bomb[i].bomb_counter = BOMB_TIME;
                                for (uint16_t a = 0; a < Y_SIZE; a++) {
                                    for (uint16_t b = 0; b < X_SIZE; b++) {
                                        if (gs.worm[i].pos.x >= gs.matrix_of_blocks[a][b].pos.x) {
                                            if (gs.worm[i].pos.x < gs.matrix_of_blocks[a][b].pos.x + 30) {
                                                if (gs.worm[i].pos.y >= gs.matrix_of_blocks[a][b].pos.y) {
                                                    if (gs.worm[i].pos.y < gs.matrix_of_blocks[a][b].pos.y + 30) {
                                                        explosion_x[i] = gs.matrix_of_blocks[a][b].pos.x + 15;
                                                        explosion_y[i] = gs.matrix_of_blocks[a][b].pos.y + 15;
                                                        gs.bomb[i].pos.x = gs.matrix_of_blocks[a][b].pos.x + 9;
                                                        gs.bomb[i].pos.y = gs.matrix_of_blocks[a][b].pos.y + 4;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else if (gs.bomb[i].state == BOMB_PRESENT) {
                                gs.bomb[i].bomb_counter--;

                                if (gs.bomb[i].bomb_counter < 0) {
                                    gs.bomb[i].bomb_counter = 0;
                                    gs.bomb[i].state = BOMB_NOT_PRESENT;
                                    gs.explosion[i].presence = EXPLOSION_PRESENT;
                                    gs.explosion[i].pos.x = explosion_x[i];
                                    gs.explosion[i].pos.y = explosion_y[i];
                                }
                            }
                        }
                    }


                    //Update worm state
                    int worm_height;
                    int worm_width;

                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        switch (direction[i]) {
                            case 'w':
                            case 's':
                                worm_height = 20;
                                worm_width = 25;
                                break;

                            case 'a':
                            case 'd':
                                worm_height = 25;
                                worm_width = 20;
                                break;
                        }
                    }

                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        //Worm and bomb functionality
                        if (gs.worm[i].pos.x > gs.explosion[i].pos.x - (worm_width + 8) &&
                            gs.worm[i].pos.x < gs.explosion[i].pos.x + 8 &&
                            gs.worm[i].pos.y > gs.explosion[i].pos.y - (40 + worm_height) &&
                            gs.worm[i].pos.y < gs.explosion[i].pos.y + 40 &&
                            gs.explosion[i].presence == EXPLOSION_PRESENT) {
                            gs.worm[i].presence = WORM_NOT_PRESENT;
                        } else if (gs.worm[i].pos.x > gs.explosion[i].pos.x - (40 + worm_width) &&
                                   gs.worm[i].pos.x < gs.explosion[i].pos.x + 40 &&
                                   gs.worm[i].pos.y > gs.explosion[i].pos.y - (worm_height + 8) &&
                                   gs.worm[i].pos.y < gs.explosion[i].pos.y + 8 &&
                                   gs.explosion[i].presence == EXPLOSION_PRESENT) {
                            gs.worm[i].presence = WORM_NOT_PRESENT;
                        }

                        //Blocks and bomb functionality
                        for (uint16_t a = 0; a < Y_SIZE; a++) {
                            for (uint16_t b = 0; b < X_SIZE; b++) {
                                if (gs.explosion[i].pos.x - 40 < gs.matrix_of_blocks[a][b].pos.x + 30 &&
                                    gs.explosion[i].pos.x + 40 > gs.matrix_of_blocks[a][b].pos.x
                                    && gs.explosion[i].pos.y - 8 < gs.matrix_of_blocks[a][b].pos.y + 30 &&
                                    gs.explosion[i].pos.y + 8 > gs.matrix_of_blocks[a][b].pos.y
                                    && gs.explosion[i].presence == EXPLOSION_PRESENT)
                                    gs.matrix_of_blocks[a][b].state = BLOCK_DESTROYED;
                                if (gs.explosion[i].pos.y - 40 < gs.matrix_of_blocks[a][b].pos.y + 30 &&
                                    gs.explosion[i].pos.y + 40 > gs.matrix_of_blocks[a][b].pos.y
                                    && gs.explosion[i].pos.x - 8 < gs.matrix_of_blocks[a][b].pos.x + 30 &&
                                    gs.explosion[i].pos.x + 8 > gs.matrix_of_blocks[a][b].pos.x
                                    && gs.explosion[i].presence == EXPLOSION_PRESENT)
                                    gs.matrix_of_blocks[a][b].state = BLOCK_DESTROYED;
                            }
                        }
                    }


                    //State machine

                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        //State machine for worm
                        switch (gs.worm[i].anim.state) {
                            case WORM_IDLE:
                                if (mov_x[i] != 0 || mov_y[i] != 0) {
                                    gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                    gs.worm[i].anim.state = WORM_WALK_1;
                                }
                                break;
                            case WORM_WALK_1:
                                if (gs.worm[i].anim.delay_cnt != 0) {
                                    gs.worm[i].anim.delay_cnt--;
                                } else {
                                    if (mov_x[i] != 0 || mov_y[i] != 0) {
                                        gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                        gs.worm[i].anim.state = WORM_WALK_2;
                                    } else {
                                        gs.worm[i].anim.state = WORM_IDLE;
                                    }
                                }
                                break;
                            case WORM_WALK_2:
                                if (gs.worm[i].anim.delay_cnt != 0) {
                                    gs.worm[i].anim.delay_cnt--;
                                } else {
                                    if (mov_x[i] != 0 || mov_y[i] != 0) {
                                        gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                        gs.worm[i].anim.state = WORM_WALK_3;
                                    } else {
                                        gs.worm[i].anim.state = WORM_WALK_1;
                                    }
                                }
                                break;
                            case WORM_WALK_3:
                                if (gs.worm[i].anim.delay_cnt != 0) {
                                    gs.worm[i].anim.delay_cnt--;
                                } else {
                                    if (mov_x[i] != 0 || mov_y[i] != 0) {
                                        gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                        gs.worm[i].anim.state = WORM_WALK_4;
                                    } else {
                                        gs.worm[i].anim.state = WORM_WALK_2;
                                    }
                                }
                                break;
                            case WORM_WALK_4:
                                if (gs.worm[i].anim.delay_cnt != 0) {
                                    gs.worm[i].anim.delay_cnt--;
                                } else {
                                    if (mov_x[i] != 0 || mov_y[i] != 0) {
                                        gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                        gs.worm[i].anim.state = WORM_WALK_5;
                                    } else {
                                        gs.worm[i].anim.state = WORM_WALK_3;
                                    }
                                }
                                break;
                            case WORM_WALK_5:
                                if (gs.worm[i].anim.delay_cnt != 0) {
                                    gs.worm[i].anim.delay_cnt--;
                                } else {
                                    if (mov_x[i] != 0 || mov_y[i] != 0) {
                                        gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                        gs.worm[i].anim.state = WORM_WALK_6;
                                    } else {
                                        gs.worm[i].anim.state = WORM_WALK_2;
                                    }
                                }
                                break;
                            case WORM_WALK_6:
                                if (gs.worm[i].anim.delay_cnt != 0) {
                                    gs.worm[i].anim.delay_cnt--;
                                } else {
                                    if (mov_x[i] != 0 || mov_y[i] != 0) {
                                        gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                        gs.worm[i].anim.state = WORM_WALK_7;
                                    } else {
                                        gs.worm[i].anim.state = WORM_WALK_1;
                                    }
                                }
                                break;
                            case WORM_WALK_7:
                                if (gs.worm[i].anim.delay_cnt != 0) {
                                    gs.worm[i].anim.delay_cnt--;
                                } else {
                                    gs.worm[i].anim.delay_cnt = WORM_ANIM_DELAY;
                                    gs.worm[i].anim.state = WORM_IDLE;
                                }
                                break;
                        }
                    }

                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        //State machine for explosion
                        if (gs.explosion[i].presence == EXPLOSION_PRESENT) {
                            switch (gs.explosion[i].state) {
                                case EXPLOSION_IDLE:
                                    gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                    gs.explosion[i].state = EXPLOSION_1;
                                    break;
                                case EXPLOSION_1:
                                    if (gs.explosion[i].delay_cnt != 0) {
                                        gs.explosion[i].delay_cnt--;
                                    } else {
                                        gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                        gs.explosion[i].state = EXPLOSION_2;
                                    }
                                    break;
                                case EXPLOSION_2:
                                    if (gs.explosion[i].delay_cnt != 0) {
                                        gs.explosion[i].delay_cnt--;
                                    } else {
                                        gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                        gs.explosion[i].state = EXPLOSION_3;
                                    }
                                    break;
                                case EXPLOSION_3:
                                    if (gs.explosion[i].delay_cnt != 0) {
                                        gs.explosion[i].delay_cnt--;
                                    } else {
                                        gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                        gs.explosion[i].state = EXPLOSION_4;
                                    }
                                    break;
                                case EXPLOSION_4:
                                    if (gs.explosion[i].delay_cnt != 0) {
                                        gs.explosion[i].delay_cnt--;
                                    } else {
                                        gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                        gs.explosion[i].state = EXPLOSION_5;
                                    }
                                    break;
                                case EXPLOSION_5:
                                    if (gs.explosion[i].delay_cnt != 0) {
                                        gs.explosion[i].delay_cnt--;
                                    } else {
                                        gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                        gs.explosion[i].state = EXPLOSION_6;
                                    }
                                    break;
                                case EXPLOSION_6:
                                    if (gs.explosion[i].delay_cnt != 0) {
                                        gs.explosion[i].delay_cnt--;
                                    } else {
                                        gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                        gs.explosion[i].state = EXPLOSION_7;
                                    }
                                    break;
                                case EXPLOSION_7:
                                    if (gs.explosion[i].delay_cnt != 0) {
                                        gs.explosion[i].delay_cnt--;
                                    } else {
                                        gs.explosion[i].delay_cnt = BOMB_ANIM_DELAY;
                                        gs.explosion[i].state = EXPLOSION_IDLE;
                                        gs.explosion[i].presence = EXPLOSION_NOT_PRESENT;

                                        for(i = 0; i < NUMBER_OF_PLAYERS; i++){
                                            if(gs.worm[i].presence == WORM_NOT_PRESENT){
                                                if(i == 1){
                                                    end_flag = 2;
                                                }else{
                                                    end_flag = 1;
                                                }

                                            }
                                        }
                                    }
                                    break;
                            }
                        } else {
                            gs.explosion[i].state = EXPLOSION_IDLE;
                        }
                    }


                    // Detecting rising edge of VSync
                    // Draw in buffer while it is in VSync
                    WAIT_UNITL_0(gpu_p32[2]);
                    WAIT_UNITL_1(gpu_p32[2]);


                    //Drawing
                    //Draw black background
                    for (uint16_t r = 0; r < SCREEN_RGB333_H; r++) {
                        for (uint16_t c = 0; c < SCREEN_RGB333_W; c++) {
                            unpack_rgb333_p32[r * SCREEN_RGB333_W + c] = 0000;
                        }
                    }

                    //Draw bomb
                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        if (gs.bomb[i].state == BOMB_PRESENT) {
                            draw_sprite_from_atlas_worms(13, 382, 11, 21, gs.bomb[i].pos.x, gs.bomb[i].pos.y, 'b');
                        }
                    }

                    //Draw worm
                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        if (gs.worm[i].presence == WORM_PRESENT) {
                            switch (gs.worm[i].anim.state) {
                                case WORM_IDLE:
                                    switch (direction[i]) {
                                        case 'w':
                                            draw_sprite_from_atlas_worms(696 - 33, 8, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'a':
                                            draw_sprite_from_atlas_worms(9, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 's':
                                            draw_sprite_from_atlas_worms(696 - 33, 1920 - 28, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'd':
                                            draw_sprite_from_atlas_worms(1920 - 28, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                case WORM_WALK_1:
                                case WORM_WALK_7:
                                    switch (direction[i]) {
                                        case 'w':
                                            draw_sprite_from_atlas_worms(696 - 33, 45, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'a':
                                            //TODO ispitaj dal' 46 ili 45
                                            draw_sprite_from_atlas_worms(46, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 's':
                                            draw_sprite_from_atlas_worms(696 - 33, 1920 - 65, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'd':
                                            draw_sprite_from_atlas_worms(1920 - 65, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                case WORM_WALK_2:
                                case WORM_WALK_6:
                                    switch (direction[i]) {
                                        case 'w':
                                            draw_sprite_from_atlas_worms(696 - 33, 85, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'a':
                                            draw_sprite_from_atlas_worms(85, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 's':
                                            draw_sprite_from_atlas_worms(696 - 33, 1920 - 101, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'd':
                                            draw_sprite_from_atlas_worms(1920 - 101, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                case WORM_WALK_3:
                                case WORM_WALK_5:
                                    switch (direction[i]) {
                                        case 'w':
                                            draw_sprite_from_atlas_worms(696 - 33, 120, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'a':
                                            draw_sprite_from_atlas_worms(120, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 's':
                                            draw_sprite_from_atlas_worms(696 - 33, 1920 - 140, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'd':
                                            draw_sprite_from_atlas_worms(1920 - 139, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                case WORM_WALK_4:
                                    switch (direction[i]) {
                                        case 'w':
                                            draw_sprite_from_atlas_worms(696 - 33, 155, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'a':
                                            draw_sprite_from_atlas_worms(155, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 's':
                                            draw_sprite_from_atlas_worms(696 - 33, 1920 - 175, 25, 20, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        case 'd':
                                            draw_sprite_from_atlas_worms(1920 - 175, 9, 20, 25, gs.worm[i].pos.x,
                                                                         gs.worm[i].pos.y, direction[i]);
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                            }
                        } else if (gs.worm[i].presence == WORM_NOT_PRESENT) {
                            //TODO dodaj da crv crkava
                            break;
                        }
                    }

                    //Draw explosion
                    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                        if (gs.explosion[i].presence == EXPLOSION_PRESENT) {
                            switch (gs.explosion[i].state) {
                                case EXPLOSION_IDLE:
                                    break;
                                case EXPLOSION_1:
                                case EXPLOSION_7:
                                    draw_sprite_from_atlas_explosion(41, 448, 72, 72, gs.explosion[i].pos.x,
                                                                     gs.explosion[i].pos.y, 35, 35);
                                    break;
                                case EXPLOSION_2:
                                case EXPLOSION_6:
                                    draw_sprite_from_atlas_explosion(130, 447, 74, 74, gs.explosion[i].pos.x,
                                                                     gs.explosion[i].pos.y, 36, 36);
                                    break;
                                case EXPLOSION_3:
                                case EXPLOSION_5:
                                    draw_sprite_from_atlas_explosion(34, 556, 79, 80, gs.explosion[i].pos.x,
                                                                     gs.explosion[i].pos.y, 38, 39);
                                    break;
                                case EXPLOSION_4:
                                    draw_sprite_from_atlas_explosion(133, 557, 80, 80, gs.explosion[i].pos.x,
                                                                     gs.explosion[i].pos.y, 39, 39);
                                    break;
                            }
                        }
                    }

                    //Draw blocks
                    draw_map(3168, 192);
                    draw_matrix_of_blocks(3280, 172, 3280, 209, gs.matrix_of_blocks);

                    if(end_flag != 0){
                        gs.state = END_PHASE;
                        break;
                    }

                }
                break;
            case START_PHASE:
                draw_notifications(0, 0, 480, 256, 0, 0, 0);

                sleep(2);

                gs.state = GAME_PHASE;

                break;
            case END_PHASE:
                if(end_flag == 1){
                    draw_notifications(0, 0, 480, 256, 0, 0, 2);
                }else if(end_flag == 2){
                    draw_notifications(0, 0, 480, 256, 0, 0, 1);
                }

                sleep(5);

                break;
        }
    }

    return 0;
}