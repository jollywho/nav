# project name (generate executable with this name)
TARGET   = fnav

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

SOURCES  := $(wildcard $(SRCDIR)/core/*.c)
INCLUDES := $(wildcard $(SRCDIR)/core/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm       = rm -f

$(BINDIR)/$(TARGET): $(OBJECTS)
	@$(LINKER) $@ $(LFLAGS) $(OBJECTS)
	@echo "Linking complete!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

.PHONEY: clean
clean:
	@$(rm) $(OBJECTS)
	@echo "Cleanup complete!"

.PHONEY: remove
remove: clean
	@$(rm) $(BINDIR)/$(TARGET)
	@echo "Executable removed!"
