include config.mk
ifeq ($(ALIGN),)
	ALIGN = $(shell align/getalign)
	OBJ_EXTRA_PREREQ := align/getalign
else
	OBJ_EXTRA_PREREQ :=
endif

PROGRAM := main
SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))
PKGCONFIG_LIBS := $(LUA_LIB)
CFLAGS := -std=c99 -O2 -Wall -Wextra $(shell pkg-config --cflags $(PKGCONFIG_LIBS))
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -DPROGRAM_NAME=\"$(PROGRAM)\" -DDUMB_ALIGN=$(ALIGN)
LDLIBS := -lpthread $(shell pkg-config --libs $(PKGCONFIG_LIBS))

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LOADLIBES) $(LDLIBS) -o $@

%.o: %.c $(OBJ_EXTRA_PREREQ)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

align/getalign: align/getalign_cxx11.cpp align/getalign_gnuc.c
	$(CC) -std=c11 align/getalign_c11.c -o $@ \
		|| $(CC) align/getalign_gnuc.c -o $@ \
		|| $(CXX) -std=c++11 align/getalign_cxx11.cpp -o $@

clean:
	$(RM) $(PROGRAM) $(OBJECTS) align/getalign

.PHONY: clean
