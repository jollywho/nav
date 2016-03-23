#ifndef FN_TUI_MESSAGE_H
#define FN_TUI_MESSAGE_H

extern int dialog_pending;

void dialog_input(int);
int confirm(char *, ...);

#endif
