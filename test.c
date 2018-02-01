#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <cbm.h>
#include <6502.h>

#include "screen.h"
#include "charset.h"

enum card_id {
    CARD0 = 0,
    CARD1 = 1,
    CARD2 = 2,
    CARD3 = 3,
    CARD4 = 4,
    CARD5 = 5,
    CARD6 = 6,
    CARD7 = 7,
    CARD8 = 8,
    CARD9 = 9,
    CARD_DRAGON = 10,
    CARD_FLOWER = 11,
};

enum suit {
    RED = COLOR_RED,
    GREEN = COLOR_GREEN,
    BLACK = COLOR_BLACK,
};

typedef uint8_t card_t;

#define card_number(card)       (card & 0xf)
#define card_color(card)         (card >> 4)
#define make_card(number, color) ((color << 4) | number)

/* Cursor position */
static uint16_t posx;
static uint8_t posy;
/* ID of card held by cursor. 0 if none */
static uint8_t held_card;
/* Pointer into screen memory where card drawing is taking place (avoids parameter passing) */
static uint8_t *card_draw_screenpos;
/* Same as above for color ram */
static uint8_t *card_draw_colorpos;

/* Card positions */

#define CARD_WIDTH  4
#define CARD_HEIGHT 7
#define CARD_WIDTH_PX   (CARD_WIDTH * 8)
#define CARD_HEIGHT_PX  (CARD_HEIGHT * 8)

#define LOWER_STACKS_Y      (CARD_HEIGHT + 2)
#define NUM_LOWER_STACKS    8
#define UPPER_STACKS_Y      1

/* Fill one row of a card's color memory */
static void set_card_row_color(uint8_t color)
{
    memset(card_draw_colorpos, color, CARD_WIDTH);
}

static void draw_card_top(card_t card)
{
    card_draw_screenpos[0] = CARD_IDX_TOP_LEFT(card_number(card));
    card_draw_screenpos[1] = CARD_IDX(TOP);
    card_draw_screenpos[2] = CARD_IDX(TOP);
    card_draw_screenpos[3] = CARD_IDX(TOP_RIGHT);
}

static void draw_card_middle(void)
{
    card_draw_screenpos[0] = CARD_IDX(LEFT);
    card_draw_screenpos[1] = ' ';
    card_draw_screenpos[2] = ' ';
    card_draw_screenpos[3] = CARD_IDX(RIGHT);
}

static void draw_card_bottom(card_t card)
{
    card_draw_screenpos[0] = CARD_IDX(BOTTOM_LEFT);
    card_draw_screenpos[1] = CARD_IDX(BOTTOM);
    card_draw_screenpos[2] = CARD_IDX(BOTTOM);
    card_draw_screenpos[3] = CARD_IDX_BOTTOM_RIGHT(card_number(card));
}

#define screenpos_oob() (card_draw_screenpos >= &get_screen_mem()->mem[0] + (SCREENMEM_SIZE - 3))
static void draw_card(uint8_t x, uint8_t y, card_t card)
{
    char *char_addr = &get_screen_mem()->mem[0];
    register uint16_t offset = x + y * 40;
    int i;

    card_draw_screenpos = &get_screen_mem()->mem[offset];
    card_draw_colorpos = &COLOR_RAM[offset];

    if (screenpos_oob())
        return;

    draw_card_top(card);
    set_card_row_color(card_color(card));
    card_draw_screenpos += SCREEN_WIDTH;
    card_draw_colorpos += SCREEN_WIDTH;


    for (i = 0; i < CARD_HEIGHT - 2; i++) {
        if (screenpos_oob())
            return;
        draw_card_middle();
        set_card_row_color(card_color(card));
        card_draw_screenpos += SCREEN_WIDTH;
        card_draw_colorpos += SCREEN_WIDTH;
    }

    if (screenpos_oob())
        return;

    draw_card_bottom(card);
    set_card_row_color(card_color(card));
}

static void cards(void)
{
    int i;

#define STEP (CARD_WIDTH + 1)
    draw_card(STEP * 5, UPPER_STACKS_Y, make_card(7, COLOR_RED));
    draw_card(STEP * 6, UPPER_STACKS_Y, make_card(7, COLOR_GREEN));
    draw_card(STEP * 7, UPPER_STACKS_Y, make_card(7, COLOR_BLACK));

    for (i = 0; i < (NUM_LOWER_STACKS * STEP); i += STEP) {
        draw_card(i, LOWER_STACKS_Y, make_card(7, COLOR_RED));
    }
    for (i = 0; i < (NUM_LOWER_STACKS * STEP / 2); i += STEP) {
        draw_card(i, LOWER_STACKS_Y + 1, make_card(6, COLOR_GREEN));
    }
    for (i = 0; i < (NUM_LOWER_STACKS * STEP / 4); i += STEP) {
        draw_card(i, LOWER_STACKS_Y + 2, make_card(6, COLOR_BLACK));
    }
#undef STEP
}

#define RASTER_MIN      51
#define RASTER_MAX      (RASTER_MIN + SCREEN_HEIGHT * 8)

#define SPRITE_XOFFSET  24
#define SPRITE_YOFFSET  (29 + 21)

#define SPRITE_XMIN (SPRITE_XOFFSET)
#define SPRITE_XMAX (SPRITE_XOFFSET + (SCREEN_WIDTH * 8) - 2) /* Leave a couple extra pixels so it's still visible */
#define SPRITE_YMIN (SPRITE_YOFFSET)
#define SPRITE_YMAX (SPRITE_YOFFSET + (SCREEN_HEIGHT * 8) - 2) /* Leave a couple extra pixels so it's still visible */

#define SPRITE_ID_CARD_BG       7
#define SPRITE_ID_CARD_TOP      6
#define SPRITE_ID_CARD_BOTTOM   5
#define SPRITE_ID_MOUSE         0

#define SPRITE_CARD_MASK    ((1 << SPRITE_ID_CARD_BG) | \
                             (1 << SPRITE_ID_CARD_TOP) | \
                             (1 << SPRITE_ID_CARD_BOTTOM))
#define SPRITE_MOUSE_MASK   (1 << SPRITE_ID_MOUSE)

#define show_card_sprites() { VIC.spr_ena |= SPRITE_CARD_MASK; }
#define hide_card_sprites() { VIC.spr_ena &= ~SPRITE_CARD_MASK; }

/*
 * 3 sprites to draw the card.
 * 24x34 pixels
 * top: 24x21
 * bot: 24x13
 * bg:  24x17 (y doubled) 24x34
 */
#define SPRITE_CARD_WIDTH_PX    (24)
#define SPRITE_CARD_HEIGHT_PX   (34)

#define JOY_UP      (1 << 0)
#define JOY_DOWN    (1 << 1)
#define JOY_LEFT    (1 << 2)
#define JOY_RIGHT   (1 << 3)
#define JOY_BTN     (1 << 4)

#define JOY_SPEED 4

/* Debounced button state. */
static bool button_state;
static uint8_t button_state_frames = 255;
#define button_changed()    (button_state_frames == 2) /* 2 frame debounce interval */

static void joy2_process(void)
{
    uint16_t card_posx;
    uint8_t card_posy;
    uint8_t joyval = ~CIA1.pra;
    bool cur_button_state;

    /* Handle button debounce */
    cur_button_state = !!(joyval & JOY_BTN);
    if (cur_button_state == button_state) {
        if (button_state_frames < 255) {
            button_state_frames++;
        }
    } else {
        button_state_frames = 1;
    }
    button_state = cur_button_state;

    if (button_changed()) {
        if (button_state) {
            held_card = make_card(7, RED);
            show_card_sprites();
        } else {
            hide_card_sprites();
            draw_card(posx / 8 - (SPRITE_XOFFSET / 8), posy / 8 - (SPRITE_YOFFSET / 8), held_card);
            held_card = 0;
        }
    }

    if (joyval & JOY_UP) {
        posy -= JOY_SPEED;
    }
    if (joyval & JOY_DOWN) {
        posy += JOY_SPEED;
    }
    if (posy > SPRITE_YMAX) {
        posy = SPRITE_YMAX;
    }
    if (posy < SPRITE_YMIN) {
        posy = SPRITE_YMIN;
    }
    if (joyval & JOY_LEFT) {
        posx -= JOY_SPEED;
    }
    if (joyval & JOY_RIGHT) {
        posx += JOY_SPEED;
    }
    if (posx > SPRITE_XMAX) {
        posx = SPRITE_XMAX;
    }
    if (posx < SPRITE_XMIN) {
        posx = SPRITE_XMIN;
    }
    card_posx = posx - (SPRITE_CARD_WIDTH_PX / 2);
    VIC.spr_pos[SPRITE_ID_CARD_BG].x = (uint8_t)card_posx;
    VIC.spr_pos[SPRITE_ID_CARD_TOP].x = (uint8_t)card_posx;
    VIC.spr_pos[SPRITE_ID_CARD_BOTTOM].x = (uint8_t)card_posx;
    VIC.spr_pos[SPRITE_ID_MOUSE].x = (uint8_t)posx;

    VIC.spr_hi_x &= ~(SPRITE_CARD_MASK | SPRITE_MOUSE_MASK);
    if (card_posx >> 8) {
        VIC.spr_hi_x |= SPRITE_CARD_MASK;
    }
    if (posx >> 8) {
        VIC.spr_hi_x |= SPRITE_MOUSE_MASK;
    }

    card_posy = posy - (SPRITE_CARD_HEIGHT_PX / 2);
    VIC.spr_pos[SPRITE_ID_CARD_BG].y = (uint8_t)card_posy;
    VIC.spr_pos[SPRITE_ID_CARD_TOP].y = (uint8_t)card_posy;
    VIC.spr_pos[SPRITE_ID_CARD_BOTTOM].y = (uint8_t)(card_posy + 21);
    VIC.spr_pos[SPRITE_ID_MOUSE].y = (uint8_t)posy;
}

/*
 * Copy a character (8x8) bitmap to an 8x8 location in a sprite of width 24.
 * Must be byte aligned.
 */
static void copy_char_to_sprite(uint8_t *c, uint8_t *sprite)
{
    int i;

    for (i = 0; i < 8; i++) {
        *sprite = c[i];
        sprite += 3;
    }
}

static void sprite_card_personify(uint8_t number)
{

    copy_char_to_sprite(CARD_TOP_LEFT + number * 8, SPRITE_CARD_TOP);
    /* Bottom right is row 26, col 2 */
    copy_char_to_sprite(CARD_BOTTOM_RIGHT + number * 8, SPRITE_CARD_BOTTOM + (26 - 21) * 3 + 2);
}

static void sprite_setup(void)
{
    get_screen_mem()->sprite_ptr[SPRITE_ID_CARD_BG] = (uint8_t)&SPRITE_PTR_CARD_BG;
    get_screen_mem()->sprite_ptr[SPRITE_ID_CARD_TOP] = (uint8_t)&SPRITE_PTR_CARD_TOP;
    get_screen_mem()->sprite_ptr[SPRITE_ID_CARD_BOTTOM] = (uint8_t)&SPRITE_PTR_CARD_BOTTOM;
    get_screen_mem()->sprite_ptr[SPRITE_ID_MOUSE] = (uint8_t)&SPRITE_PTR_MOUSE;
    VIC.spr_exp_y = (1 << SPRITE_ID_CARD_BG);
    VIC.spr_color[SPRITE_ID_CARD_BG] = COLOR_WHITE;
    VIC.spr_color[SPRITE_ID_CARD_TOP] = COLOR_BLACK;
    VIC.spr_color[SPRITE_ID_CARD_BOTTOM] = COLOR_BLACK;
    VIC.spr_color[SPRITE_ID_MOUSE] = COLOR_BLACK;
    VIC.spr_ena = SPRITE_MOUSE_MASK; // Enable mouse
    joy2_process(); // Set up initial position

    sprite_card_personify(7);
}


int main(void)
{
    printf("Hello world port: 0x%x\n", *(unsigned char *)(0x01));
    printf("Screen at 0x%x\n", (uint16_t)get_screen_mem());
#if 1
    sprite_setup();
    copy_character_rom();
    set_screen_addr();
    init_screen();
    cards();
#endif
    //printf("Screenreg 0x %x\n", (char)&SCREENREG);
    //printf("Press return to exit");
    while (1) {
        while (VIC.rasterline < RASTER_MAX);

        VIC.bordercolor = COLOR_RED;
        joy2_process();
        VIC.bordercolor = COLOR_BLACK;

        while (VIC.rasterline >= RASTER_MAX);
    }
    while (cbm_k_getin() != '1');

    restore_screen_addr();

    return 0;
}
