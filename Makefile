# project name (generate executable with this name)
CORE     = fnavd
GUI      = fnav

CC       = gcc
# compiling flags here
CFLAGS= -std=c99 -Wall -I. -Werror -Wextra -pedantic -Wno-unused-but-set-parameter -Wno-unused-parameter -Wno-unused-variable

LINKER   = gcc -o
# linking flags here
LFLAGS   = -lutil -lcurses

# change these to set the proper directories where each files shoould be
SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

SOURCES1  := $(wildcard $(SRCDIR)/core/*.c)
INCLUDES1 := $(wildcard $(SRCDIR)/core/*.h)
OBJECTS1  := $(SOURCES1:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

SOURCES2  := $(wildcard $(SRCDIR)/gui/*.c)
INCLUDES2 := $(wildcard $(SRCDIR)/gui/*.h)
OBJECTS2  := $(SOURCES2:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm       = rm -f

all: bin/fnavd bin/fnav

$(BINDIR)/$(CORE): $(OBJECTS1)
	@$(LINKER) $@ $(LFLAGS) $(OBJECTS1)
	@echo "Linking complete!"

$(BINDIR)/$(GUI): $(OBJECTS2)
	@$(LINKER) $@ $(LFLAGS) $(OBJECTS2)
	@echo "Linking complete!"

$(OBJECTS1): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

$(OBJECTS2): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

.PHONEY: clean
clean:
	@$(rm) $(OBJECTS1) $(OBJECTS2)
	@echo "Cleanup complete!"

.PHONEY: remove
remove: clean
	@$(rm) $(BINDIR)/$(CORE)
	@echo "Executable removed!"
