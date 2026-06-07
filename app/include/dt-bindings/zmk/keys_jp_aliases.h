/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <dt-bindings/zmk/keys.h>

/*
 * Japanese JIS keycode aliases modeled after QMK's keymap_japanese.h.
 * These aliases are layout-oriented names that map to existing ZMK keycodes.
 */

/* Unshifted aliases */
#define JP_ZKHK GRAVE
#define JP_1 N1
#define JP_2 N2
#define JP_3 N3
#define JP_4 N4
#define JP_5 N5
#define JP_6 N6
#define JP_7 N7
#define JP_8 N8
#define JP_9 N9
#define JP_0 N0
#define JP_MINS MINUS
#define JP_CIRC EQUAL
#define JP_YEN INT3
#define JP_Q Q
#define JP_W W
#define JP_E E
#define JP_R R
#define JP_T T
#define JP_Y Y
#define JP_U U
#define JP_I I
#define JP_O O
#define JP_P P
#define JP_AT LBKT
#define JP_LBRC RBKT
#define JP_EISU CAPS
#define JP_A A
#define JP_S S
#define JP_D D
#define JP_F F
#define JP_G G
#define JP_H H
#define JP_J J
#define JP_K K
#define JP_L L
#define JP_SCLN SEMI
#define JP_COLN SQT
#define JP_RBRC NUHS
#define JP_Z Z
#define JP_X X
#define JP_C C
#define JP_V V
#define JP_B B
#define JP_N N
#define JP_M M
#define JP_COMM COMMA
#define JP_DOT DOT
#define JP_SLSH FSLH
#define JP_BSLS INT1
#define JP_MHEN INT5
#define JP_HENK INT4
#define JP_KANA INT2

/* Shifted aliases */
#define JP_EXLM LS(JP_1)
#define JP_DQUO LS(JP_2)
#define JP_HASH LS(JP_3)
#define JP_DLR LS(JP_4)
#define JP_PERC LS(JP_5)
#define JP_AMPR LS(JP_6)
#define JP_QUOT LS(JP_7)
#define JP_LPRN LS(JP_8)
#define JP_RPRN LS(JP_9)
#define JP_EQL LS(JP_MINS)
#define JP_TILD LS(JP_CIRC)
#define JP_PIPE LS(JP_YEN)
#define JP_GRV LS(JP_AT)
#define JP_LCBR LS(JP_LBRC)
#define JP_CAPS LS(JP_EISU)
#define JP_PLUS LS(JP_SCLN)
#define JP_ASTR LS(JP_COLN)
#define JP_RCBR LS(JP_RBRC)
#define JP_LABK LS(JP_COMM)
#define JP_RABK LS(JP_DOT)
#define JP_QUES LS(JP_SLSH)
#define JP_UNDS LS(JP_BSLS)
