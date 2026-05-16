CC     = cc
TARGET = grid_viewer
SRC    = main.c

# Detect OS
UNAME := $(shell uname)

# Try pkg-config for raylib and lua; fall back to homebrew on macOS
ifeq ($(UNAME), Darwin)
    BREW := $(shell brew --prefix 2>/dev/null || echo /usr/local)

    RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null || echo -I$(BREW)/include)
    RAYLIB_LIBS   := $(shell pkg-config --libs   raylib 2>/dev/null || echo -L$(BREW)/lib -lraylib)

    LUA_CFLAGS := $(shell pkg-config --cflags lua5.4 lua-5.4 lua 2>/dev/null | head -1 || echo -I$(BREW)/include/lua)
    LUA_LIBS   := $(shell pkg-config --libs   lua5.4 lua-5.4 lua 2>/dev/null | head -1 || echo -L$(BREW)/lib -llua)

    PLATFORM_LIBS = -framework CoreVideo -framework IOKit -framework Cocoa \
                    -framework GLUT -framework OpenGL
else
    RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
    RAYLIB_LIBS   := $(shell pkg-config --libs   raylib)
    LUA_CFLAGS    := $(shell pkg-config --cflags lua5.4)
    LUA_LIBS      := $(shell pkg-config --libs   lua5.4)
    PLATFORM_LIBS  = -lGL -ldl
endif

CFLAGS = -O2 -Wall -Wextra $(RAYLIB_CFLAGS) $(LUA_CFLAGS)
LIBS   = $(RAYLIB_LIBS) $(LUA_LIBS) $(PLATFORM_LIBS) -lpthread

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean
