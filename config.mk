CC=cc
CCFLAGS= -lutil -lcurses -std=c99 -Wall -Werror -Wextra -pedantic
SRC=$(shell find src/core -name "*.c")
GSRC=$(shell find src/gui -name "*.c")
OBJECTDIR=obj
BINDIR=bin
OBJ=$(SRC:%.c=%.o)
GOBJ=$(GSRC:%.c=%.o)
CORE=bin/clam
GUI=bin/gui
