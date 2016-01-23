#ifndef FN_ASCII_H
#define FN_ASCII_H

#include <stdbool.h>

#define CharOrd(x)      ((x) < 'a' ? (x) - 'A' : (x) - 'a')
#define CharOrdLow(x)   ((x) - 'a')
#define CharOrdUp(x)    ((x) - 'A')
#define ROT13(c, a)     (((((c) - (a)) + 13) % 26) + (a))

#define NUL             '\000'
#define BELL            '\007'
#define BS              '\010'
#define TAB             '\011'
#define NL              '\012'
#define NL_STR          (char_u *)"\012"
#define FF              '\014'
#define CAR             '\015'  /* CR is used by Mac OS X */
#define ESC             '\033'
#define ESC_STR         (char_u *)"\033"
#define ESC_STR_nc      "\033"
#define DEL             0x7f
#define DEL_STR         (char_u *)"\177"
#define CSI             0x9b    /* Control Sequence Introducer */
#define CSI_STR         "\233"
#define DCS             0x90    /* Device Control String */
#define STERM           0x9c    /* String Terminator */

#define POUND           0xA3

#define Ctrl_chr(x)     (TOUPPER_ASC(x) ^ 0x40) /* '?' -> DEL, '@' -> ^@, etc. */
#define Meta(x)         ((x) | 0x80)

#define CTRL_F_STR      "\006"
#define CTRL_H_STR      "\010"
#define CTRL_V_STR      "\026"

#define Ctrl_AT         0   /* @ */
#define Ctrl_A          1
#define Ctrl_B          2
#define Ctrl_C          3
#define Ctrl_D          4
#define Ctrl_E          5
#define Ctrl_F          6
#define Ctrl_G          7
#define Ctrl_H          8
#define Ctrl_I          9
#define Ctrl_J          10
#define Ctrl_K          11
#define Ctrl_L          12
#define Ctrl_M          13
#define Ctrl_N          14
#define Ctrl_O          15
#define Ctrl_P          16
#define Ctrl_Q          17
#define Ctrl_R          18
#define Ctrl_S          19
#define Ctrl_T          20
#define Ctrl_U          21
#define Ctrl_V          22
#define Ctrl_W          23
#define Ctrl_X          24
#define Ctrl_Y          25
#define Ctrl_Z          26
/* CTRL- [ Left Square Bracket == ESC*/
#define Ctrl_BSL        28  /* \ BackSLash */
#define Ctrl_RSB        29  /* ] Right Square Bracket */
#define Ctrl_HAT        30  /* ^ */
#define Ctrl__          31


#define FORWARD  1
#define BACKWARD (-1)

#define NCH    0x01       /* need second char */
#define NCH_S  (0x02|NCH) /* NCH is special   */
#define NCH_A  (0x04|NCH) /* NCH is any char  */

/* Bits for modifier mask */
/* 0x01 cannot be used, because the modifier must be 0x02 or higher */
#define MOD_MASK_SHIFT      0x02
#define MOD_MASK_CTRL       0x04
#define MOD_MASK_ALT        0x08        /* aka META */
#define MOD_MASK_META       0x10        /* META when it's different from ALT */

#define KEY2TERMCAP0(x)         ((-(x)) & 0xff)
#define KEY2TERMCAP1(x)         (((unsigned)(-(x)) >> 8) & 0xff)
#define TERMCAP2KEY(a, b)       (-((a) + ((int)(b) << 8)))
#define IS_SPECIAL(c)           ((c) < 0)

#define KS_EXTRA                253
#define KS_ZERO                 255
#define KE_FILLER               ('X')
#define K_S_TAB         TERMCAP2KEY('k', 'B')
#define K_ZERO          TERMCAP2KEY(KS_ZERO, KE_FILLER)
#define HC_S_TAB            ("S-Tab")

# define ASCII_ISLOWER(c) ((unsigned)(c) >= 'a' && (unsigned)(c) <= 'z')
# define ASCII_ISUPPER(c) ((unsigned)(c) >= 'A' && (unsigned)(c) <= 'Z')
# define ASCII_ISALPHA(c) (ASCII_ISUPPER(c) || ASCII_ISLOWER(c))
# define ASCII_ISALNUM(c) (ASCII_ISALPHA(c) || ascii_isdigit(c))

/// Check whether character is a decimal digit.
///
/// Library isdigit() function is officially locale-dependent and, for
/// example, returns true for superscript 1 (¹) in locales where encoding
/// contains it in lower 8 bits. Also avoids crashes in case c is below
/// 0 or above 255: library functions are officially defined as accepting
/// only EOF and unsigned char values (otherwise it is undefined behaviour)
/// what may be used for some optimizations (e.g. simple `return
/// isdigit_table[c];`).
static inline bool ascii_isdigit(int c)
{
  return c >= '0' && c <= '9';
}

#endif
