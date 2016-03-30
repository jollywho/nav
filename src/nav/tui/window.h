#ifndef FN_TUI_WINDOW_H
#define FN_TUI_WINDOW_H

#include "nav/event/event.h"
#include "nav/tui/buffer.h"

void window_init(void);
void window_cleanup(void);
void window_req_draw(void *obj, argv_callback);
void window_input(int key);

void window_add_buffer(enum move_dir);
void window_close_focus();
void window_remove_buffer();
void window_ex_cmd_end();
void window_draw_all();
Buffer* window_get_focus();
Plugin* window_get_plugin();
int window_focus_attached();
void window_shift(int lines);
void win_move(void *, Keyarg *);

char* window_cur_dir();
void window_ch_dir(char *);

void window_start_override(Plugin *);
void window_stop_override();

#endif
