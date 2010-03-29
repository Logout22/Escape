ROOT = ../../..
BUILDL = $(BUILD)/user/d/$(NAME)
BIN = $(BUILD)/user_$(NAME).bin
LIB = $(ROOT)/lib
LIBC = $(ROOT)/libc
LIBD = $(ROOT)/libd
LDCONF=$(LIBD)/ld.conf
SUBDIRS = . $(filter-out Makefile $(wildcard *.*),$(wildcard *))
BUILDDIRS = $(addprefix $(BUILDL)/,$(SUBDIRS))
DEPS = $(shell find $(BUILDDIRS) -mindepth 0 -maxdepth 1 -name "*.d")

DC = $(BUILD)/dmd
DFLAGS = $(DDEFFLAGS) -I$(LIBD)/druntime/import -I$(LIBD)/phobos
LINKER = ld
LFLAGS = -T$(LDCONF) --build-id=none $(ADDFLAGS)

# sources
DSRC = $(shell find $(SUBDIRS) -mindepth 0 -maxdepth 1 -name "*.d")

# objects
LIBDA = $(BUILD)/libd.a
START = $(BUILD)/libd_startup.o
DOBJ = $(patsubst %.d,$(BUILDL)/%.o,$(DSRC))

.PHONY: all clean

all:	$(BIN)

$(BIN):	$(BUILDDIRS) $(LDCONF) $(DOBJ) $(START) $(LIBDA) $(ADDLIBS)
		@echo "	" LINKING $(BIN)
		@$(LINKER) $(LFLAGS) -o $(BIN) $(START) $(DOBJ) $(LIBDA) $(ADDLIBS);
		@echo "	" COPYING ON DISK
		$(ROOT)/tools/disk.sh copy $(BIN) /bin/$(NAME)

$(BUILDDIRS):
		@for i in $(BUILDDIRS); do \
			if [ ! -d $$i ]; then mkdir -p $$i; fi \
		done;

$(BUILDL)/%.o:		%.d
		@echo "	" DC $<
		@$(DC) $(DFLAGS) -of$@ -c $<

-include $(DEPS)

clean:
		rm -f $(BIN) $(DOBJ) $(DEPS)