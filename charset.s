    .segment "CHARMEM"
    .export _CHARMEM
    .align 1024
_CHARMEM:
    .res    33*8 ; 33 chars from the ROM table will be copied here. 33rd is space (blank) character
    ; Numbers 1-10
    .export _CARD_IDX_TOP_LEFT = (_CARD_TOP_LEFT - _CHARMEM) / 8
    .export _CARD_TOP_LEFT
_CARD_TOP_LEFT:
    .incbin "images/1.bitmap"
    .incbin "images/2.bitmap"
    .incbin "images/3.bitmap"
    .incbin "images/4.bitmap"
    .incbin "images/5.bitmap"
    .incbin "images/6.bitmap"
    .incbin "images/7.bitmap"
    .incbin "images/8.bitmap"
    .incbin "images/9.bitmap" ; Really this is the same a 6 reversed
    .export _CARD_IDX_BOTTOM_RIGHT = (_CARD_BOTTOM_RIGHT - _CHARMEM) / 8
    .export _CARD_BOTTOM_RIGHT
_CARD_BOTTOM_RIGHT:
    .incbin "images/1.reversed.bitmap"
    .incbin "images/2.reversed.bitmap"
    .incbin "images/3.reversed.bitmap"
    .incbin "images/4.reversed.bitmap"
    .incbin "images/5.reversed.bitmap"
    .incbin "images/6.reversed.bitmap"
    .incbin "images/7.reversed.bitmap"
    .incbin "images/8.reversed.bitmap"
    .incbin "images/9.reversed.bitmap"
    .export _CARD_IDX_TOP = (CARD_TOP - _CHARMEM) / 8
CARD_TOP:
    .byte   $ff, $00, $00, $00, $00, $00, $00, $00
    .export _CARD_IDX_RIGHT = (CARD_RIGHT - _CHARMEM) / 8
CARD_RIGHT:
    .byte   $01, $01, $01, $01, $01, $01, $01, $01
    .export _CARD_IDX_BOTTOM = (CARD_BOTTOM - _CHARMEM) / 8
CARD_BOTTOM:
    .byte   $00, $00, $00, $00, $00, $00, $00, $ff
    .export _CARD_IDX_BOTTOM_LEFT = (CARD_BOTTOM_LEFT - _CHARMEM) / 8
CARD_BOTTOM_LEFT:
    .byte   $80, $80, $80, $80, $80, $80, $80, $7f
    .export _CARD_IDX_TOP_RIGHT = (CARD_TOP_RIGHT - _CHARMEM) / 8
CARD_TOP_RIGHT:
    .byte   $fe, $01, $01, $01, $01, $01, $01, $01
    .export _CARD_IDX_LEFT = (CARD_LEFT - _CHARMEM) / 8
CARD_LEFT:
    .byte   $80, $80, $80, $80, $80, $80, $80, $80
    ; solid
    .byte   $ff, $ff, $ff, $ff, $ff, $ff, $ff, $ff
    .align  256 * 8

    .export _SCREENREG = (_SCREENMEM >> (2 + 4)) | (_CHARMEM >> 10) ; Use our char mem
    ;.export _SCREENREG = ($400 >> (2 + 4)) | (_CHARMEM >> 10) ; Use our char mem with stock screen ram position
    ;.export _SCREENREG = (_SCREENMEM >> (2 + 4)) | ($1000 >> 10) ; Use the ROM
    ;.export _SCREENREG = _CHARMEM >> 10 ; Use our char mem with stock screen ram position
    .export _SCREENMEM
_SCREENMEM:
    .res    1024
    .align  64
    .export _SPRITE_PTR_CARD_TOP = _SPRITE_CARD_TOP / 64
    .export _SPRITE_CARD_TOP
_SPRITE_CARD_TOP:
    .incbin "images/cardsprite_top.bitmap"
    .align  64

    .export _SPRITE_PTR_CARD_BOTTOM = _SPRITE_CARD_BOTTOM / 64
    .export _SPRITE_CARD_BOTTOM
_SPRITE_CARD_BOTTOM:
    .incbin "images/cardsprite_bottom.bitmap"
    .align  64

.export _SPRITE_PTR_CARD_BG = SPRITE_CARD_BG / 64
SPRITE_CARD_BG:
    .incbin "images/cardsprite_bg.bitmap"
    .align  64

.export _SPRITE_PTR_MOUSE = SPRITE_MOUSE / 64
SPRITE_MOUSE:
    .incbin "images/mouse_sprite.bitmap"
    .align  64
