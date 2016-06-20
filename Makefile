CC ?= gcc
AR ?= gcc

BIN = lib/libbs.a
MODULES = rbtree util event timer queue work net

OBJ = $(MODULES:%=build/bs_gcc/%.o)
LINKOBJ = $(OBJ) $(RES)

INCS = -I include
LIBS = -lm -lpthread -Llib -lbs
CFLAGS += -D_GNU_SOURCE

.PHONY: all all-before all-after install clean clean-custom

all: all-before $(BIN) all-after

all-before:
	mkdir -p build/bs_gcc
	mkdir -p lib

clean: clean-custom
	rm -rf build lib

remake: clean all

$(BIN): $(LINKOBJ)
	$(AR) rsc $@ $^
	
build/bs_gcc/%.o: src/%.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@
