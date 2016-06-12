#ifndef NV_TUI_OVERLAY_H
#define NV_TUI_OVERLAY_H

#include <ncurses.h>
#include "nav/plugins/plugin.h"
#include "nav/option.h"

Overlay* overlay_new();
void overlay_delete(Overlay *ov);
void overlay_set(Overlay *ov, pos_T size, pos_T ofs, int sep);

void overlay_bufno(Overlay *ov, int id);
void overlay_lnum(Overlay *ov, int lnum, int max);
void overlay_filter(Overlay *ov, int max, bool enable);
void overlay_edit(Overlay *ov, char *, char *, char *);
void overlay_progress(Overlay *ov, long);
void overlay_draw(void **argv);
void overlay_clear(Overlay *ov);
void overlay_erase(Overlay *ov);
void overlay_focus(Overlay *ov);
void overlay_unfocus(Overlay *ov);

#endif
