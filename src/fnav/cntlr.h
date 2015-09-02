#ifndef FN_CORE_CNTLR_H
#define FN_CORE_CNTLR_H

#include "fnav.h"

typedef void (*fn)(char **args);

typedef struct {
  String name;
  int required_parameters;
  fn exec;
} Cmd;

// TOOD:  context for cntlr cmds.
//        consider cmds for:
//  "dd"  -> del line
//  "diw" -> del inner word
// a context is held for the cntlr
// internally. a focus change/<esc>
// is sent to the cntlr to clear
// context or do whatever. running
// a command consumes context, too.
//  "d"  ->  del()  updates cmd_lst.
//  "i"  ->  _del() updates again.
//  "w"  ->  _del() runs as 'diw'.
//
//  "d"  -> del()   updates cmd_lst.
//  "D"  -> Del2()  ignores previous
//                  line and runs as
//                  'D'.
//
// cntlr starts with default cmds.
// regex match as keys received.
// cmd is run when unambiguous.
// context is reset if no match.
//
// [N]operator [obj] [N]motion
//
// default context -> operators + motions
// operator -> motion context for that op
// obj      -> prefix motion. *do later*
// motion   -> run

typedef struct {
  Cmd *cmd_lst; //should be hash
  void (*_cancel);
  int  (*_input)(String key);
} Cntlr;

#endif
