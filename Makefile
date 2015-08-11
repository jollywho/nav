include config.mk

all: $(CORE) $(GUI)

$(CORE): $(OBJ)
	@if [ ! -d $(BINDIR) ]; then mkdir $(BINDIR); fi; $(CC) $(CCFLAGS) $(OBJ) -o $@

$(GUI): $(GOBJ)
	@if [ ! -d $(BINDIR) ]; then mkdir $(BINDIR); fi; $(CC) $(CCFLAGS) $(GOBJ) -o $@

%.o: %.c
	@if [ ! -d $(OBJECTDIR) ]; then mkdir $(OBJECTDIR); fi; $(CC) $(CCFLAGS) -c $< -o $@

.PHONY : clean
clean:
	@if [ -d $(BINDIR) ]; then rm -rfv $(BINDIR); fi; if [ -d $(OBJECTDIR) ]; then rm -rfv $(OBJECTDIR); fi;
