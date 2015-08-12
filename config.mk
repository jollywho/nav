CC=cc
CCFLAGS= -lutil -lcurses -std=c99 -Wall -Werror -Wextra -pedantic -Wno-unused-but-set-parameter -Wno-unused-parameter -Wno-unused-variable
SRC=$(shell find src/core -name "*.c")
GSRC=$(shell find src/gui -name "*.c")
OBJECTDIR=obj
BINDIR=bin
OBJ=$(SRC:%.c=%.o)
GOBJ=$(GSRC:%.c=%.o)
CORE=bin/fnav
GUI=bin/gui
