#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <cbm.h>
#include <6502.h>

#include "screen.h"
#include "charset.h"

#define USE_ASM 1

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
    CARD_BACK = 12,
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
/* The location where the card was taken from */
static uint8_t held_card_src_col;
static bool game_over = false;
/* Pointer into screen memory where card drawing is taking place (avoids parameter passing) */
uint8_t *card_draw_screenpos;
/* Same as above for color ram */
uint8_t *card_draw_colorpos;

/* Card positions */

#define NUM_STACKS      8
#define STACK_MAX_CARDS 10
static card_t stacks[NUM_STACKS][STACK_MAX_CARDS];
#define NUM_CELLS       3
static card_t freecells[NUM_CELLS];
static card_t done_stack[4];

#define CARD_WIDTH  4
#define CARD_HEIGHT 7
#define CARD_WIDTH_PX   (CARD_WIDTH * 8)
#define CARD_HEIGHT_PX  (CARD_HEIGHT * 8)

#define LOWER_STACKS_Y      (CARD_HEIGHT + 2)
#define NUM_LOWER_STACKS    8

#define STACK_MAX_ROWS  (SCREEN_HEIGHT - LOWER_STACKS_Y)

/* Fill one row of a card's color memory */
#if !USE_ASM
static void fastcall set_card_row_color(uint8_t color)
{
    memset(card_draw_colorpos, color, CARD_WIDTH);
}
#else
extern void fastcall asm_set_card_row_color(uint8_t color);
#define set_card_row_color(color) asm_set_card_row_color(color)
#endif

static void card_draw_line_advance(void)
{
    card_draw_screenpos += SCREEN_WIDTH;
    card_draw_colorpos += SCREEN_WIDTH;
}

#define card_draw_set_offset(x, y) { \
    uint16_t offset = x + y * 40; \
    card_draw_screenpos = &get_screen_mem()->mem[offset]; \
    card_draw_colorpos = &COLOR_RAM[offset]; \
}

#if !USE_ASM
static void draw_card_top(card_t card)
{
    card_draw_screenpos[0] = CARD_IDX_TOP_LEFT(card_number(card));
    card_draw_screenpos[1] = CARD_IDX(TOP);
    card_draw_screenpos[2] = CARD_IDX(TOP);
    card_draw_screenpos[3] = CARD_IDX(TOP_RIGHT);
}
#else
extern void fastcall asm_draw_card_top(card_t card);
#define draw_card_top(card) asm_draw_card_top(card)
#endif

/* Draw the left, two middle spaces, and right */
#if !USE_ASM
static void draw_card_middle(void)
{
    card_draw_screenpos[0] = CARD_IDX(LEFT);
    card_draw_screenpos[1] = ' ';
    card_draw_screenpos[2] = ' ';
    card_draw_screenpos[3] = CARD_IDX(RIGHT);
}
#else
extern void fastcall asm_draw_card_middle(void);
#define draw_card_middle() asm_draw_card_middle()
#endif

#if !USE_ASM
static void draw_card_bottom(card_t card)
{
    card_draw_screenpos[0] = CARD_IDX(BOTTOM_LEFT);
    card_draw_screenpos[1] = CARD_IDX(BOTTOM);
    card_draw_screenpos[2] = CARD_IDX(BOTTOM);
    card_draw_screenpos[3] = CARD_IDX_BOTTOM_RIGHT(card_number(card));
}
#else
extern void fastcall asm_draw_card_bottom(card_t card);
#define draw_card_bottom(card) asm_draw_card_bottom(card)
#endif

static void draw_bg()
{
    card_draw_screenpos[0] = BG_CHAR;
    card_draw_screenpos[1] = BG_CHAR;
    card_draw_screenpos[2] = BG_CHAR;
    card_draw_screenpos[3] = BG_CHAR;
}

static void draw_card(uint8_t x, uint8_t y, card_t card)
{
    char *char_addr = &get_screen_mem()->mem[0];
    int i;

    card_draw_set_offset(x, y);

    if (card == 0) {
        for (i = 0; i < CARD_HEIGHT; i++) {
            set_card_row_color(COLOR_GREEN);
            draw_bg();
            card_draw_line_advance();
        }
        return;
    }

    draw_card_top(card);
    set_card_row_color(card_color(card));
    card_draw_line_advance();


    for (i = 0; i < CARD_HEIGHT - 2; i++) {
        draw_card_middle();
        set_card_row_color(card_color(card));
        card_draw_line_advance();
    }

    draw_card_bottom(card);
    set_card_row_color(card_color(card));
}

static void move_done_stack(uint8_t done, uint8_t card)
{
    uint8_t x = (done+4) * (CARD_WIDTH+1);
    done_stack[done] = card;
    draw_card(x, 1, card);
}

static void draw_cell(uint8_t cell)
{
    uint8_t x = cell * (CARD_WIDTH+1);
    draw_card(x, 1, freecells[cell]);
}

static void draw_stack(uint8_t stack)
{
    register uint8_t row;
    register card_t *stack_cards;
    register card_t card;
    register uint8_t color;
    register uint8_t body = 0;
    static uint8_t body_count; // Not reentrant

    if (stack > NUM_STACKS) {
        draw_cell(stack - NUM_STACKS);
        return;
    }

    stack_cards = stacks[stack];
    card_draw_set_offset(stack * (CARD_WIDTH + 1), LOWER_STACKS_Y);

    for (row = 0; row < STACK_MAX_ROWS; row++) {
        card = row < STACK_MAX_CARDS ? stack_cards[row] : 0;

        if (card) {
            draw_card_top(card);
            color = card_color(card);
            set_card_row_color(color);
            body = card;
            body_count = 0;
        } else if (body) {
            set_card_row_color(color);
            if (body_count < CARD_HEIGHT - 2) {
                draw_card_middle();
                body_count++;
            } else {
                draw_card_bottom(body);
                body = 0;
            }
        } else {
            // Background
            set_card_row_color(COLOR_GREEN);
            draw_bg();
        }

        card_draw_line_advance();
    }
}

static void check_moves(void);

#define DECK_SIZE 38

#define rand() (SID.noise)

static void cards(void)
{
    int i;
    int j = 0;
    static uint8_t deck[DECK_SIZE];
    uint8_t rnd1, rnd2, tmp;

    for (i=0; i<11; i++) {
        deck[j++] = make_card(i+1, RED);
    }
    for (i=0; i<11; i++) {
        deck[j++] = make_card(i+1, GREEN);
    }
    /* No black flower */
    for (i=0; i<10; i++) {
        deck[j++] = make_card(i+1, BLACK);
    }

    /* There are actually 3 each of the dragons, so throw in the extras */
    deck[j++] = make_card(CARD_DRAGON, RED);
    deck[j++] = make_card(CARD_DRAGON, RED);
    deck[j++] = make_card(CARD_DRAGON, GREEN);
    deck[j++] = make_card(CARD_DRAGON, GREEN);
    deck[j++] = make_card(CARD_DRAGON, BLACK);
    deck[j++] = make_card(CARD_DRAGON, BLACK);
    
    /* Setup RNG */
    SID.v3.freq = 0xffff;
    /* Noise waveform, output disabled */
    SID.v3.ctrl = 0x80;

    /* Perform 100 swaps */
    for (i=0; i<100; i++) {
        rnd1 = rand() % DECK_SIZE;
        rnd2 = rand() % DECK_SIZE;

        tmp = deck[rnd1];
        deck[rnd1] = deck[rnd2];
        deck[rnd2] = tmp;
    }

    /* Deal them out */
    j=0;
    for(i = 0; i<NUM_STACKS; i++) {
        stacks[i][0] = deck[j++];
        draw_stack(i);
    }
    for(i = 0; i<NUM_STACKS; i++) {
        stacks[i][1] = deck[j++];
        draw_stack(i);
    }
    for(i = 0; i<NUM_STACKS; i++) {
        stacks[i][2] = deck[j++];
        draw_stack(i);
    }
    for(i = 0; i<NUM_STACKS; i++) {
        stacks[i][3] = deck[j++];
        draw_stack(i);
    }
    for(i = 0; i<6; i++) {
        stacks[i][4] = deck[j++];
        draw_stack(i);
    }

    check_moves();
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

static void drop_card_internal(uint8_t stack, uint8_t held_card);

static void drop_card_cell(uint8_t stack, uint8_t held_card)
{
    uint8_t cell = stack - NUM_STACKS;

    if (cell > NUM_CELLS-1)
        cell = NUM_CELLS-1;

    if (freecells[cell]) {
        /* Stack already occupied */
        drop_card_internal(held_card_src_col, held_card);
    } else {
        freecells[cell] = held_card;
        draw_cell(cell);
    }

    held_card = 0;
}

static void drop_card_internal(uint8_t stack, uint8_t held_card)
{
    int i;

    if (stack >= NUM_STACKS) {
        drop_card_cell(stack, held_card);
        return;
    }

    for (i=0; i<STACK_MAX_CARDS; i++) {
        if (stacks[stack][i] == 0) {
            stacks[stack][i] = held_card;
            draw_stack(stack);
            break;
        }
    }

    if (i==STACK_MAX_CARDS) {
        drop_card_internal(held_card_src_col, held_card);
    }

    held_card = 0;
}

static void drop_card(uint8_t stack, uint8_t held_card)
{
    int j;
    card_t top_card;

    if (stack < NUM_STACKS) {
        /* Can't move a dragon from stack to stack */
        if (card_number(held_card) == CARD_DRAGON) {
            drop_card_internal(held_card_src_col, held_card);
            return;
        }

        for (j=STACK_MAX_CARDS-1; j>=0; j--) {
            if (stacks[stack][j])
                break;
        }

        /* Check if stack is empty */
        if (j >= 0) {
            top_card = stacks[stack][j];

            /* Can only stack cards by descending number */
            if (card_number(held_card) != card_number(top_card)-1) {
                drop_card_internal(held_card_src_col, held_card);
                return;
            }

            /* Can't stack cards of the same color */
            if (card_color(held_card) == card_color(top_card)-1) {
                drop_card_internal(held_card_src_col, held_card);
                return;
            }
        }
    }

    drop_card_internal(stack, held_card);
}

#define x_to_stack(x) (((x)/8/(CARD_WIDTH+1)) > NUM_STACKS-1 ? NUM_STACKS-1 : ((x)/8/(CARD_WIDTH+1)))

static uint8_t pos_to_stack()
{
    uint8_t stack = x_to_stack(posx - SPRITE_XOFFSET);
    if (posy - SPRITE_YOFFSET < LOWER_STACKS_Y*8) {
        /* Cursor is in the free cell area */
        stack += NUM_STACKS;
    }
    return stack;
}

static card_t take_card_cell(uint8_t stack)
{
    uint8_t cell = stack - NUM_STACKS;
    uint8_t card;

    if (cell > NUM_CELLS-1)
        cell = NUM_CELLS-1;

    card = freecells[cell];
    freecells[cell] = 0;
    draw_cell(cell);
    return card;
}

static card_t take_card()
{
    uint8_t stack = pos_to_stack();
    card_t card = 0;
    int i;

    if (stack >= NUM_STACKS) {
        return take_card_cell(stack);
    }

    for (i=0; i<STACK_MAX_CARDS; i++) {
        if (stacks[stack][i] == 0) {
            break;
        }
        card = stacks[stack][i];
    }

    if (card) {
        stacks[stack][i-1] = 0;
        held_card_src_col = stack;
        draw_stack(stack);
    }

    return card;
}

static int color_to_stack(uint8_t card) {
    switch (card_color(card)) {
        case COLOR_RED:
            return 0;
        case COLOR_GREEN:
            return 1;
        case COLOR_BLACK:
            return 2;
    }
}

static void remove_free_cards(card_t card)
{
    int i, j;

    for (i=0; i<NUM_STACKS; i++) {
        for (j=STACK_MAX_CARDS-1; j>=0; j--) {
            if (stacks[i][j])
                break;
        }

        /* Check if stack is empty */
        if (j<0)
            continue;

        if (stacks[i][j] == card) {
            stacks[i][j] = 0;
            draw_stack(i);
        }
    }

    for (i=0; i<NUM_CELLS; i++) {
        if (freecells[i] == card) {
            freecells[i] = 0;
            draw_cell(i);
        }
    }
}

/* Look for any flowers or cards that can move to done */
static void check_moves(void)
{
    int i, j;
    card_t card;
    bool redraw;
    bool rerun;
    int stack;
    bool found_card;
    uint8_t free_dragons[3];

again:
    free_dragons[0] = 0;
    free_dragons[1] = 0;
    free_dragons[2] = 0;
    rerun = false;
    found_card = false;
    for (i=0; i<NUM_STACKS; i++) {
        redraw = false;

        for (j=STACK_MAX_CARDS-1; j>=0; j--) {
            if (stacks[i][j])
                break;
        }

        /* Check if stack is empty */
        if (j<0)
            continue;

        found_card = true;
        card = stacks[i][j];
        if (card_number(card) == CARD_FLOWER) {
            stacks[i][j] = 0;
            redraw = true;
        } else {
            stack = color_to_stack(card);
            if (card_number(card) == CARD_DRAGON)
                free_dragons[stack]++;

            if (card_number(card) == card_number(done_stack[stack])+1) {
                move_done_stack(stack, card);
                stacks[i][j] = 0;
                redraw = true;
            }
        }

        if (redraw) {
            draw_stack(i);
            rerun = true;
        }
    }

    for (i=0; i<NUM_CELLS; i++) {
        if (freecells[i] == 0)
            continue;

        found_card = true;
        card = freecells[i];
        stack = color_to_stack(card);
        if (card_number(card) == CARD_DRAGON)
            free_dragons[stack]++;
        if (card_number(card) == card_number(done_stack[stack])+1) {
            move_done_stack(stack, card);
            freecells[i] = 0;
            draw_cell(i);
            rerun = true;
        }
    }

    for (i=0; i<3; i++) {
        card = done_stack[i];
        stack = color_to_stack(card);
        if (card_number(card) == CARD_DRAGON)
            free_dragons[stack]++;
    }

    if (free_dragons[0] == 3) {
        remove_free_cards(make_card(CARD_DRAGON, RED));
        rerun = true;
    }
    if (free_dragons[1] == 3) {
        remove_free_cards(make_card(CARD_DRAGON, GREEN));
        rerun = true;
    }
    if (free_dragons[2] == 3) {
        remove_free_cards(make_card(CARD_DRAGON, BLACK));
        rerun = true;
    }

    if (!found_card) {
        game_over = true;
        return;
    }

    if (rerun)
        goto again;
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

static void sprite_card_personify(card_t card)
{
    copy_char_to_sprite(CARD_TOP_LEFT + (card_number(card)-1) * 8, SPRITE_CARD_TOP);
    /* Bottom right is row 26, col 2 */
    copy_char_to_sprite(CARD_BOTTOM_RIGHT + (card_number(card)-1) * 8, SPRITE_CARD_BOTTOM + (26 - 21) * 3 + 2);
    VIC.spr_color[SPRITE_ID_CARD_TOP] = card_color(card);
    VIC.spr_color[SPRITE_ID_CARD_BOTTOM] = card_color(card);
}

static void move_card_sprite(uint16_t x, uint8_t y)
{
    VIC.spr_pos[SPRITE_ID_CARD_BG].x = (uint8_t)x;
    VIC.spr_pos[SPRITE_ID_CARD_TOP].x = (uint8_t)x;
    VIC.spr_pos[SPRITE_ID_CARD_BOTTOM].x = (uint8_t)x;

    if (x >> 8) {
        VIC.spr_hi_x |= SPRITE_CARD_MASK;
    } else {
        VIC.spr_hi_x &= ~SPRITE_CARD_MASK;
    }

    VIC.spr_pos[SPRITE_ID_CARD_BG].y = (uint8_t)y;
    VIC.spr_pos[SPRITE_ID_CARD_TOP].y = (uint8_t)y;
    VIC.spr_pos[SPRITE_ID_CARD_BOTTOM].y = (uint8_t)(y + 21);
}

static void joy2_process(void)
{
    uint16_t card_posx;
    uint8_t card_posy;
    uint8_t joyval = ~CIA1.pra;
    uint8_t stack;
    bool cur_button_state;

    VIC.bordercolor = COLOR_BLUE;

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
            held_card = take_card();
            if (held_card) {
                sprite_card_personify(held_card);
                show_card_sprites();
            }
        } else {
            hide_card_sprites();
            if (held_card) {
                stack = pos_to_stack();
                drop_card(stack, held_card);
                check_moves();
            }
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
    card_posy = posy - (SPRITE_CARD_HEIGHT_PX / 2);
    move_card_sprite(card_posx, card_posy);

    if (posx >> 8) {
        VIC.spr_hi_x |= SPRITE_MOUSE_MASK;
    } else {
        VIC.spr_hi_x &= ~SPRITE_MOUSE_MASK;
    }

    VIC.spr_pos[SPRITE_ID_MOUSE].x = (uint8_t)posx;
    VIC.spr_pos[SPRITE_ID_MOUSE].y = (uint8_t)posy;
    VIC.bordercolor = COLOR_RED;
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
    while (cbm_k_getin() != 'q' && !game_over) {
        while (VIC.rasterline < RASTER_MAX);

        VIC.bordercolor = COLOR_RED;
        joy2_process();
        VIC.bordercolor = COLOR_BLACK;

        while (VIC.rasterline >= RASTER_MAX);
    }

    restore_screen_addr();

    return 0;
}
