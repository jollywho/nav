#ifndef FN_TUI_OVERLAY_H
#define FN_TUI_OVERLAY_H

#include <ncurses.h>
#include "fnav/plugins/plugin.h"
#include "fnav/option.h"

Overlay* overlay_new();
void overlay_delete(Overlay *ov);
void overlay_set(Overlay *ov, pos_T size, pos_T ofs, int sep);

void overlay_bufno(Overlay *ov, int id);
void overlay_edit(Overlay *ov, String name, String usr, String in, String out);
void overlay_draw(void **argv);
void overlay_clear(Overlay *ov);
void overlay_focus(Overlay *ov);
void overlay_unfocus(Overlay *ov);

#endif
