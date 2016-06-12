#ifndef NV_TUI_WINDOW_H
#define NV_TUI_WINDOW_H

#include "nav/event/event.h"
#include "nav/tui/buffer.h"

void window_init(void);
void window_cleanup(void);
void window_req_draw(void *obj, argv_callback);
void window_refresh();
void window_input(Keyarg *ca);

void window_add_buffer(enum move_dir);
void window_close_focus();
void window_remove_buffer(Buffer *);
void window_ex_cmd_end();
void window_draw_all();
void window_update();
Buffer* window_get_focus();
Plugin* window_get_plugin();
int window_focus_attached();
void win_move(void *, Keyarg *);

char* window_cur_dir();
void window_ch_dir(char *);

void window_start_override(Plugin *);
void window_stop_override();

#endif
