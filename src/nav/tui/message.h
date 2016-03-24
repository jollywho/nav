#ifndef FN_TUI_MESSAGE_H
#define FN_TUI_MESSAGE_H

extern int dialog_pending;
extern int message_pending;

void dialog_input(int);
int confirm(char *, ...);
void nv_err(char *fmt, ...);
void nv_clr_msg();

#endif
