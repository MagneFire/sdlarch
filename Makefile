target   := sdlarch
sources  := sdlarch.c glad.c gles.c
CFLAGS   := -Wall -g
LFLAGS   := -static-libgcc
LIBS     := 
packages := sdl2

# do not edit from here onwards
objects := $(addprefix build/,$(sources:.c=.o))
#ifneq ($(packages),)
LIBS    += $(shell pkg-config --libs-only-l $(packages)) -lGLESv2 -lEGL
LFLAGS  += $(shell pkg-config --libs-only-L --libs-only-other $(packages))
CFLAGS  += $(shell pkg-config --cflags $(packages))
#endif

.PHONY: all clean

all: $(target)
clean:
	-rm -rf build
	-rm -f $(target)

$(target): Makefile $(objects)
	$(CC) $(LFLAGS) -o $@ $(objects) $(LIBS)

build/%.o: %.c Makefile
	-@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -MMD -o $@ $<

-include $(addprefix build/,$(sources:.c=.d))

