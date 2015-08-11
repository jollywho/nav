include config.mk

all: $(EXECUTABLE) run

%.o: $(SOURCEDIR)/%.c
	@if [ ! -d $(OBJECTDIR) ]; then mkdir $(OBJECTDIR); fi; $(CC) $(CCFLAGS) -c $< -o $(OBJECTDIR)/$@

$(EXECUTABLE): $(OBJECTS)
	@if [ ! -d $(BINDIR) ]; then mkdir $(BINDIR); fi; $(CC) $(CCFLAGS) $(addprefix $(OBJECTDIR)/,$(OBJECTS)) -o $@

.PHONY : clean
clean:
	@if [ -d $(BINDIR) ]; then rm -rfv $(BINDIR); fi; if [ -d $(OBJECTDIR) ]; then rm -rfv $(OBJECTDIR); fi;

.PHONY : run
run: $(EXECUTABLE)
	@$(EXECUTABLE) ncmpcpp
