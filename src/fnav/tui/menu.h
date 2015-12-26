#ifndef FN_TUI_MENU_H
#define FN_TUI_MENU_H

//      *ex_cmd
//         |
//         v
//      *cmdline<---+
//         |        |
//         v        |
//       tokens     |
//         |        |
//         v        |
//       *menu-->---+
//
// * ex_cmd creates a raw string from input.
// some motions like kill-word on an empty line
// request menu restore the previous cmdstr, if
// one is available.
//
// * cmdline parses the raw string into tokens
// and cmdstrs.
//
// * menu creates context from the cmdstr and
// consumes the cmdline causing it to reset.
// ** this means run requests are first handled
// by the menu before cmd. request detection is
// still handled by ex_cmd along with key maps.
//
// *--------------------------------------------*
// | /fnav/src/fnav.c                           |
// |_/fnav/src/tui/event.c______________________|
// | /fnav/src/compl.c                          |
// *--------------------------------------------*
// | new | FM | {path}                          |
// *--------------------------------------------*
// |:even_                                      |
// *--------------------------------------------*
//
// *--------------------------------------------*
// | Text         | Fg=34      Bg=#AFF002       |
// | Title        | Fg=#000045                  |
// |_CursorLineNr_|_Fg=________Bg=Red___________|
// *--------------------------------------------*
// | hi | Blockquote | link | {groupname}       |
// *--------------------------------------------*
// |:_                                          |
// *--------------------------------------------*

void menu_start();
void menu_stop();

#endif
