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

    # Static versions of the non-system libs for distribution
    RAYLIB_STATIC := $(BREW)/lib/libraylib.a
    LUA_STATIC    := $(BREW)/lib/liblua.a
else
    RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
    RAYLIB_LIBS   := $(shell pkg-config --libs   raylib)
    LUA_CFLAGS    := $(shell pkg-config --cflags lua5.4)
    LUA_LIBS      := $(shell pkg-config --libs   lua5.4)
    PLATFORM_LIBS  = -lGL -ldl
endif

CFLAGS = -O2 -Wall -Wextra $(RAYLIB_CFLAGS) $(LUA_CFLAGS)
LIBS   = $(RAYLIB_LIBS) $(LUA_LIBS) $(PLATFORM_LIBS) -lpthread

# Development build (links against dylibs — fast, requires homebrew)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

# Distributable build — statically links raylib + lua, no homebrew needed on target Mac
dist: $(SRC)
ifeq ($(UNAME), Darwin)
	$(CC) $(CFLAGS) -o $(TARGET)_dist $< \
	    $(RAYLIB_STATIC) $(LUA_STATIC) \
	    $(PLATFORM_LIBS) -lpthread
	@echo "Built $(TARGET)_dist — send this file to your friend"
	@echo "They need macOS (no other dependencies)"
else
	@echo "Static dist build not configured for this platform"
endif

clean:
	rm -f $(TARGET) $(TARGET)_dist

.PHONY: clean dist
