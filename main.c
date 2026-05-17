#include "raylib.h"
#include "raymath.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define GRID_SIZE    20
#define CUBE_SIZE    0.88f
#define CUBE_SPACING 1.0f

static int   grid[GRID_SIZE][GRID_SIZE][GRID_SIZE];
static Color colors[GRID_SIZE][GRID_SIZE][GRID_SIZE];
static bool  show_wires = true;

// ---------------------------------------------------------------------------
// Thread-safe command queue (stdin -> main loop)
// ---------------------------------------------------------------------------
#define QUEUE_CAP 256
#define CMD_MAX   2048

static char cmd_queue[QUEUE_CAP][CMD_MAX];
static int  q_head = 0, q_tail = 0;
static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;

static void *stdin_thread(void *arg) {
    (void)arg;
    char buf[CMD_MAX];
    fprintf(stderr, "> ");
    fflush(stderr);
    while (fgets(buf, sizeof buf, stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] != '\0') {
            pthread_mutex_lock(&q_lock);
            int next = (q_tail + 1) % QUEUE_CAP;
            if (next != q_head) {
                strncpy(cmd_queue[q_tail], buf, CMD_MAX - 1);
                cmd_queue[q_tail][CMD_MAX - 1] = '\0';
                q_tail = next;
            }
            pthread_mutex_unlock(&q_lock);
        }
        fprintf(stderr, "> ");
        fflush(stderr);
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Default color per position (classic Rubik's cube face colors)
// ---------------------------------------------------------------------------
static Color default_color(int x, int y, int z) {
    if (y == GRID_SIZE - 1) return WHITE;
    if (y == 0)             return YELLOW;
    if (z == GRID_SIZE - 1) return RED;
    if (z == 0)             return ORANGE;
    if (x == GRID_SIZE - 1) return GREEN;
    if (x == 0)             return BLUE;
    return (Color){60, 60, 60, 255};
}

static void reset_all_colors(void) {
    for (int x = 0; x < GRID_SIZE; x++)
        for (int y = 0; y < GRID_SIZE; y++)
            for (int z = 0; z < GRID_SIZE; z++)
                colors[x][y][z] = default_color(x, y, z);
}

// ---------------------------------------------------------------------------
// Axis arrows (X=red, Y=green, Z=blue) — called inside BeginMode3D
// ---------------------------------------------------------------------------
static void draw_axes(void) {
    // Lua index (0,0,0) maps to this world position
    float off = (GRID_SIZE - 1) * CUBE_SPACING * 0.5f;
    Vector3 o = {-off, -off, -off};

    // Axes extend the full grid length plus a little overhang
    float grid_len = (GRID_SIZE - 1) * CUBE_SPACING;
    float cone_len = 0.9f;
    float shaft_end = grid_len + 1.2f;
    float tip_end   = shaft_end + cone_len;
    float r_shaft   = 0.07f;
    float r_cone    = 0.22f;

    Vector3 xs = {o.x + shaft_end, o.y,            o.z           };
    Vector3 xt = {o.x + tip_end,   o.y,            o.z           };
    Vector3 ys = {o.x,             o.y + shaft_end, o.z           };
    Vector3 yt = {o.x,             o.y + tip_end,   o.z           };
    Vector3 zs = {o.x,             o.y,            o.z + shaft_end};
    Vector3 zt = {o.x,             o.y,            o.z + tip_end  };

    // Origin sphere at Lua (0,0,0)
    DrawSphere(o, r_cone, WHITE);

    // Shafts
    DrawCylinderEx(o, xs, r_shaft, r_shaft, 8, RED);
    DrawCylinderEx(o, ys, r_shaft, r_shaft, 8, GREEN);
    DrawCylinderEx(o, zs, r_shaft, r_shaft, 8, BLUE);

    // Arrowhead cones
    DrawCylinderEx(xs, xt, r_cone, 0, 8, RED);
    DrawCylinderEx(ys, yt, r_cone, 0, 8, GREEN);
    DrawCylinderEx(zs, zt, r_cone, 0, 8, BLUE);
}

// ---------------------------------------------------------------------------
// Grid drawing (shared by all three camera views)
// ---------------------------------------------------------------------------
static void draw_grid(void) {
    float offset = (GRID_SIZE - 1) * CUBE_SPACING * 0.5f;
    for (int x = 0; x < GRID_SIZE; x++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int z = 0; z < GRID_SIZE; z++) {
                if (!grid[x][y][z]) continue;

                if (x > 0 && x < GRID_SIZE-1 &&
                    y > 0 && y < GRID_SIZE-1 &&
                    z > 0 && z < GRID_SIZE-1 &&
                    grid[x-1][y][z] && grid[x+1][y][z] &&
                    grid[x][y-1][z] && grid[x][y+1][z] &&
                    grid[x][y][z-1] && grid[x][y][z+1]) continue;

                Vector3 pos = {
                    x * CUBE_SPACING - offset,
                    y * CUBE_SPACING - offset,
                    z * CUBE_SPACING - offset,
                };
                DrawCube(pos, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, colors[x][y][z]);
                if (show_wires)
                    DrawCubeWires(pos, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE,
                                  (Color){0, 0, 0, 180});
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Lua API
// ---------------------------------------------------------------------------

static int l_set_cube(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int z = (int)luaL_checkinteger(L, 3);
    int s = lua_toboolean(L, 4);
    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE)
        grid[x][y][z] = s;
    return 0;
}

static int l_get_cube(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int z = (int)luaL_checkinteger(L, 3);
    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE)
        lua_pushboolean(L, grid[x][y][z]);
    else
        lua_pushboolean(L, 0);
    return 1;
}

static int l_clear(lua_State *L) {
    (void)L;
    memset(grid, 0, sizeof grid);
    return 0;
}

static int l_fill(lua_State *L) {
    (void)L;
    for (int x = 0; x < GRID_SIZE; x++)
        for (int y = 0; y < GRID_SIZE; y++)
            for (int z = 0; z < GRID_SIZE; z++)
                grid[x][y][z] = 1;
    return 0;
}

static int l_set_layer(lua_State *L) {
    const char *axis = luaL_checkstring(L, 1);
    int idx = (int)luaL_checkinteger(L, 2);
    int s   = lua_toboolean(L, 3);
    if (idx < 0 || idx >= GRID_SIZE) return 0;
    for (int a = 0; a < GRID_SIZE; a++)
        for (int b = 0; b < GRID_SIZE; b++) {
            switch (axis[0]) {
                case 'x': grid[idx][a][b] = s; break;
                case 'y': grid[a][idx][b] = s; break;
                case 'z': grid[a][b][idx] = s; break;
            }
        }
    return 0;
}

static int l_set_color(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int z = (int)luaL_checkinteger(L, 3);
    int r = (int)luaL_checknumber(L, 4);
    int g = (int)luaL_checknumber(L, 5);
    int b = (int)luaL_checknumber(L, 6);
    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE)
        colors[x][y][z] = (Color){
            (unsigned char)Clamp((float)r, 0, 255),
            (unsigned char)Clamp((float)g, 0, 255),
            (unsigned char)Clamp((float)b, 0, 255),
            255
        };
    return 0;
}

static int l_set_color_hsv(lua_State *L) {
    int   x = (int)luaL_checkinteger(L, 1);
    int   y = (int)luaL_checkinteger(L, 2);
    int   z = (int)luaL_checkinteger(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    float s = (float)luaL_checknumber(L, 5);
    float v = (float)luaL_checknumber(L, 6);
    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE && z >= 0 && z < GRID_SIZE)
        colors[x][y][z] = ColorFromHSV(h, s, v);
    return 0;
}

static int l_set_layer_color(lua_State *L) {
    const char *axis = luaL_checkstring(L, 1);
    int idx = (int)luaL_checkinteger(L, 2);
    int r   = (int)luaL_checknumber(L, 3);
    int g   = (int)luaL_checknumber(L, 4);
    int b   = (int)luaL_checknumber(L, 5);
    if (idx < 0 || idx >= GRID_SIZE) return 0;
    Color col = {
        (unsigned char)Clamp((float)r, 0, 255),
        (unsigned char)Clamp((float)g, 0, 255),
        (unsigned char)Clamp((float)b, 0, 255),
        255
    };
    for (int a = 0; a < GRID_SIZE; a++)
        for (int c = 0; c < GRID_SIZE; c++) {
            switch (axis[0]) {
                case 'x': colors[idx][a][c] = col; break;
                case 'y': colors[a][idx][c] = col; break;
                case 'z': colors[a][c][idx] = col; break;
            }
        }
    return 0;
}

static int l_set_layer_color_hsv(lua_State *L) {
    const char *axis = luaL_checkstring(L, 1);
    int   idx = (int)luaL_checkinteger(L, 2);
    float h   = (float)luaL_checknumber(L, 3);
    float s   = (float)luaL_checknumber(L, 4);
    float v   = (float)luaL_checknumber(L, 5);
    if (idx < 0 || idx >= GRID_SIZE) return 0;
    Color col = ColorFromHSV(h, s, v);
    for (int a = 0; a < GRID_SIZE; a++)
        for (int c = 0; c < GRID_SIZE; c++) {
            switch (axis[0]) {
                case 'x': colors[idx][a][c] = col; break;
                case 'y': colors[a][idx][c] = col; break;
                case 'z': colors[a][c][idx] = col; break;
            }
        }
    return 0;
}

static int l_reset_colors(lua_State *L) {
    (void)L;
    reset_all_colors();
    return 0;
}

static int l_get_time(lua_State *L) {
    lua_pushnumber(L, GetTime());
    return 1;
}

static int l_stop(lua_State *L) {
    (void)L;
    lua_pushnil(L);
    lua_setglobal(L, "update");
    return 0;
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
#define ED_MAX_LINES  1000
#define ED_MAX_LINE    512
#define ED_FONT_SIZE    18
#define ED_LINE_H       24
#define ED_GUTTER_W     52
#define ED_STATUS_H     22
#define ED_PAD           3

typedef struct {
    char   lines[ED_MAX_LINES][ED_MAX_LINE];
    int    nlines;
    int    cur_line, cur_col;
    int    scroll_y;
    bool   dirty;
    bool   has_file;
    char   filename[512];
    char   msg[128];
    double msg_time;
    // Selection: anchor is where shift-selection started; cursor is the live end
    bool   sel_active;
    int    sel_anchor_line, sel_anchor_col;
} Editor;

static Editor ed;

static void ed_clamp_col(void) {
    int len = (int)strlen(ed.lines[ed.cur_line]);
    if (ed.cur_col > len) ed.cur_col = len;
    if (ed.cur_col < 0)   ed.cur_col = 0;
}

static void ed_scroll_to_cursor(int visible_lines) {
    if (ed.cur_line < ed.scroll_y)
        ed.scroll_y = ed.cur_line;
    if (ed.cur_line >= ed.scroll_y + visible_lines)
        ed.scroll_y = ed.cur_line - visible_lines + 1;
    if (ed.scroll_y < 0) ed.scroll_y = 0;
}

// Return selection in document order (start <= end)
static void sel_range(int *sl, int *sc, int *el, int *ec) {
    int al = ed.sel_anchor_line, ac = ed.sel_anchor_col;
    int cl = ed.cur_line,        cc = ed.cur_col;
    if (al < cl || (al == cl && ac <= cc)) {
        *sl=al; *sc=ac; *el=cl; *ec=cc;
    } else {
        *sl=cl; *sc=cc; *el=al; *ec=ac;
    }
}

static void sel_start(void) {
    if (!ed.sel_active) {
        ed.sel_active      = true;
        ed.sel_anchor_line = ed.cur_line;
        ed.sel_anchor_col  = ed.cur_col;
    }
}

static void sel_clear(void) { ed.sel_active = false; }

// Delete selected text and move cursor to selection start
static void ed_delete_selection(void) {
    if (!ed.sel_active) return;
    int sl, sc, el, ec;
    sel_range(&sl, &sc, &el, &ec);

    if (sl == el) {
        char *ln  = ed.lines[sl];
        int   len = (int)strlen(ln);
        memmove(ln + sc, ln + ec, len - ec + 1);
    } else {
        char *first = ed.lines[sl];
        char *last  = ed.lines[el];
        first[sc] = '\0';
        int tail = (int)strlen(last + ec);
        if (sc + tail < ED_MAX_LINE - 1)
            memcpy(first + sc, last + ec, tail + 1);
        int after = ed.nlines - el - 1;
        if (after > 0)
            memmove(&ed.lines[sl + 1], &ed.lines[el + 1],
                    after * sizeof ed.lines[0]);
        ed.nlines -= (el - sl);
    }
    ed.cur_line   = sl;
    ed.cur_col    = sc;
    ed.sel_active = false;
    ed.dirty      = true;
}

// Copy selected region (or current line if no selection) to clipboard
static void ed_copy_selection(bool cut) {
    static char buf[ED_MAX_LINES * (ED_MAX_LINE + 1)];
    if (!ed.sel_active) {
        SetClipboardText(ed.lines[ed.cur_line]);
        if (cut) {
            if (ed.nlines > 1) {
                int after = ed.nlines - ed.cur_line - 1;
                if (after > 0)
                    memmove(&ed.lines[ed.cur_line], &ed.lines[ed.cur_line + 1],
                            after * sizeof ed.lines[0]);
                ed.nlines--;
                if (ed.cur_line >= ed.nlines) ed.cur_line = ed.nlines - 1;
            } else {
                ed.lines[0][0] = '\0';
            }
            ed.cur_col = 0;
            ed.dirty   = true;
        }
        return;
    }

    int sl, sc, el, ec;
    sel_range(&sl, &sc, &el, &ec);
    int pos = 0;

    if (sl == el) {
        int len = ec - sc;
        memcpy(buf, ed.lines[sl] + sc, len);
        buf[len] = '\0';
    } else {
        int flen = (int)strlen(ed.lines[sl]) - sc;
        if (flen > 0) { memcpy(buf + pos, ed.lines[sl] + sc, flen); pos += flen; }
        buf[pos++] = '\n';
        for (int i = sl + 1; i < el; i++) {
            int len = (int)strlen(ed.lines[i]);
            memcpy(buf + pos, ed.lines[i], len); pos += len;
            buf[pos++] = '\n';
        }
        memcpy(buf + pos, ed.lines[el], ec); pos += ec;
        buf[pos] = '\0';
    }
    SetClipboardText(buf);
    if (cut) ed_delete_selection();
}

static void ed_load(const char *path) {
    strncpy(ed.filename, path, sizeof ed.filename - 1);
    ed.nlines    = 0;
    ed.cur_line  = 0;
    ed.cur_col   = 0;
    ed.scroll_y  = 0;
    ed.dirty     = false;

    FILE *f = fopen(path, "r");
    ed.has_file = (f != NULL);
    if (!f) {
        ed.lines[0][0] = '\0';
        ed.nlines = 1;
        return;
    }
    char buf[ED_MAX_LINE];
    while (fgets(buf, sizeof buf, f) && ed.nlines < ED_MAX_LINES) {
        buf[strcspn(buf, "\r\n")] = '\0';
        strncpy(ed.lines[ed.nlines], buf, ED_MAX_LINE - 1);
        ed.nlines++;
    }
    fclose(f);
    if (ed.nlines == 0) { ed.lines[0][0] = '\0'; ed.nlines = 1; }
}

static bool ed_save(void) {
    if (!ed.has_file) return false;
    FILE *f = fopen(ed.filename, "w");
    if (!f) return false;
    for (int i = 0; i < ed.nlines; i++)
        fprintf(f, "%s\n", ed.lines[i]);
    fclose(f);
    ed.dirty = false;
    return true;
}

static void ed_insert_char(char c) {
    if (ed.sel_active) ed_delete_selection();
    char *ln  = ed.lines[ed.cur_line];
    int   len = (int)strlen(ln);
    if (len >= ED_MAX_LINE - 1) return;
    memmove(ln + ed.cur_col + 1, ln + ed.cur_col, len - ed.cur_col + 1);
    ln[ed.cur_col] = c;
    ed.cur_col++;
    ed.dirty = true;
}

static void ed_newline(void) {
    if (ed.sel_active) ed_delete_selection();
    if (ed.nlines >= ED_MAX_LINES) return;
    char *ln  = ed.lines[ed.cur_line];
    int   len = (int)strlen(ln);
    if (ed.cur_col > len) ed.cur_col = len;  // guard

    int tail_len   = len - ed.cur_col;
    int lines_after = ed.nlines - ed.cur_line - 1;
    if (lines_after > 0)
        memmove(&ed.lines[ed.cur_line + 2],
                &ed.lines[ed.cur_line + 1],
                lines_after * sizeof ed.lines[0]);

    // Zero the slot first — memmove leaves stale bytes after any prior content
    memset(ed.lines[ed.cur_line + 1], 0, ED_MAX_LINE);
    memcpy(ed.lines[ed.cur_line + 1], ln + ed.cur_col, tail_len);
    ln[ed.cur_col] = '\0';

    ed.nlines++;
    ed.cur_line++;
    ed.cur_col  = 0;
    ed.dirty    = true;
}

static void ed_backspace(void) {
    if (ed.sel_active) { ed_delete_selection(); return; }
    if (ed.cur_col > 0) {
        char *ln  = ed.lines[ed.cur_line];
        int   len = (int)strlen(ln);
        memmove(ln + ed.cur_col - 1, ln + ed.cur_col, len - ed.cur_col + 1);
        ed.cur_col--;
        ed.dirty = true;
    } else if (ed.cur_line > 0) {
        char *prev     = ed.lines[ed.cur_line - 1];
        char *curr     = ed.lines[ed.cur_line];
        int   prev_len = (int)strlen(prev);
        int   curr_len = (int)strlen(curr);
        if (prev_len + curr_len < ED_MAX_LINE - 1) {
            memcpy(prev + prev_len, curr, curr_len + 1);
            int lines_after = ed.nlines - ed.cur_line - 1;
            if (lines_after > 0)
                memmove(&ed.lines[ed.cur_line],
                        &ed.lines[ed.cur_line + 1],
                        lines_after * sizeof ed.lines[0]);
            ed.nlines--;
            ed.cur_line--;
            ed.cur_col  = prev_len;
            ed.dirty    = true;
        }
    }
}

static void ed_delete_char(void) {
    if (ed.sel_active) { ed_delete_selection(); return; }
    char *ln  = ed.lines[ed.cur_line];
    int   len = (int)strlen(ln);
    if (ed.cur_col < len) {
        memmove(ln + ed.cur_col, ln + ed.cur_col + 1, len - ed.cur_col);
        ed.dirty = true;
    } else if (ed.cur_line < ed.nlines - 1) {
        // join with next line
        char *next     = ed.lines[ed.cur_line + 1];
        int   next_len = (int)strlen(next);
        if (len + next_len < ED_MAX_LINE - 1) {
            memcpy(ln + len, next, next_len + 1);
            int lines_after = ed.nlines - ed.cur_line - 2;
            if (lines_after > 0)
                memmove(&ed.lines[ed.cur_line + 1],
                        &ed.lines[ed.cur_line + 2],
                        lines_after * sizeof ed.lines[0]);
            ed.nlines--;
            ed.dirty = true;
        }
    }
}

// Fires on initial press AND on OS key-repeat while held.
// IsKeyPressedRepeat alone only fires for the repeat events, not the first press.
#define KEY_DOWN_OR_REPEAT(k) (IsKeyPressed(k) || IsKeyPressedRepeat(k))

static void ed_handle_input(int visible_lines) {
    if (!ed.has_file) return;

    bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT)   || IsKeyDown(KEY_RIGHT_SHIFT);

    // Printable character insertion (clears selection automatically via ed_insert_char)
    if (!ctrl) {
        int ch;
        while ((ch = GetCharPressed()) > 0) {
            if (ch == '\t') { for (int i = 0; i < 4; i++) ed_insert_char(' '); }
            else if (ch >= 32 && ch < 127) ed_insert_char((char)ch);
        }
        if (KEY_DOWN_OR_REPEAT(KEY_BACKSPACE)) ed_backspace();
        if (KEY_DOWN_OR_REPEAT(KEY_DELETE))    ed_delete_char();
        if (IsKeyPressed(KEY_ENTER))           ed_newline();
    }

    // Navigation — Shift extends selection, bare arrow collapses it
#define NAV_MOVE(move_code) do { \
    if (shift) { sel_start(); move_code; } \
    else { \
        if (ed.sel_active) { \
            int sl,sc,el,ec; sel_range(&sl,&sc,&el,&ec); \
            ed.cur_line=sl; ed.cur_col=sc; sel_clear(); \
        } else { move_code; } \
    } \
} while(0)

#define NAV_MOVE_RIGHT(move_code) do { \
    if (shift) { sel_start(); move_code; } \
    else { \
        if (ed.sel_active) { \
            int sl,sc,el,ec; sel_range(&sl,&sc,&el,&ec); \
            ed.cur_line=el; ed.cur_col=ec; sel_clear(); \
        } else { move_code; } \
    } \
} while(0)

    if (KEY_DOWN_OR_REPEAT(KEY_LEFT)) {
        NAV_MOVE(
            if (ed.cur_col > 0) ed.cur_col--;
            else if (ed.cur_line > 0) { ed.cur_line--; ed.cur_col = (int)strlen(ed.lines[ed.cur_line]); }
        );
    }
    if (KEY_DOWN_OR_REPEAT(KEY_RIGHT)) {
        NAV_MOVE_RIGHT(
            int _len = (int)strlen(ed.lines[ed.cur_line]);
            if (ed.cur_col < _len) ed.cur_col++;
            else if (ed.cur_line < ed.nlines-1) { ed.cur_line++; ed.cur_col = 0; }
        );
    }
    if (KEY_DOWN_OR_REPEAT(KEY_UP)) {
        if (shift) { sel_start(); if (ed.cur_line > 0) { ed.cur_line--; ed_clamp_col(); } }
        else       { sel_clear(); if (ed.cur_line > 0) { ed.cur_line--; ed_clamp_col(); } }
    }
    if (KEY_DOWN_OR_REPEAT(KEY_DOWN)) {
        if (shift) { sel_start(); if (ed.cur_line < ed.nlines-1) { ed.cur_line++; ed_clamp_col(); } }
        else       { sel_clear(); if (ed.cur_line < ed.nlines-1) { ed.cur_line++; ed_clamp_col(); } }
    }
    if (IsKeyPressed(KEY_HOME)) {
        if (shift) { sel_start(); ed.cur_col = 0; }
        else       { sel_clear(); ed.cur_col = 0; }
    }
    if (IsKeyPressed(KEY_END)) {
        if (shift) { sel_start(); ed.cur_col = (int)strlen(ed.lines[ed.cur_line]); }
        else       { sel_clear(); ed.cur_col = (int)strlen(ed.lines[ed.cur_line]); }
    }
    if (IsKeyPressed(KEY_PAGE_UP)) {
        if (shift) sel_start(); else sel_clear();
        ed.cur_line -= visible_lines;
        if (ed.cur_line < 0) ed.cur_line = 0;
        ed_clamp_col();
    }
    if (IsKeyPressed(KEY_PAGE_DOWN)) {
        if (shift) sel_start(); else sel_clear();
        ed.cur_line += visible_lines;
        if (ed.cur_line >= ed.nlines) ed.cur_line = ed.nlines - 1;
        ed_clamp_col();
    }

#undef NAV_MOVE
#undef NAV_MOVE_RIGHT

    // Clipboard shortcuts
    if (ctrl) {
        if (IsKeyPressed(KEY_C)) ed_copy_selection(false);
        if (IsKeyPressed(KEY_X)) ed_copy_selection(true);
        if (IsKeyPressed(KEY_V)) {
            if (ed.sel_active) ed_delete_selection();
            const char *clip = GetClipboardText();
            if (clip && clip[0]) {
                for (const char *p = clip; *p; p++) {
                    if (*p == '\r') continue;
                    if (*p == '\n') ed_newline();
                    else            ed_insert_char(*p);
                }
                ed.dirty = true;
            }
        }
        if (IsKeyPressed(KEY_A)) {
            // Select all
            ed.sel_active      = true;
            ed.sel_anchor_line = 0;
            ed.sel_anchor_col  = 0;
            ed.cur_line        = ed.nlines - 1;
            ed.cur_col         = (int)strlen(ed.lines[ed.cur_line]);
        }
    }

    ed_scroll_to_cursor(visible_lines);
}

// Click in the text area to reposition the cursor
static void ed_handle_click(Vector2 panel_origin, Font font) {
    if (!ed.has_file) return;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    Vector2 mouse = GetMousePosition();
    float rel_x = mouse.x - panel_origin.x;
    float rel_y = mouse.y - panel_origin.y;

    if (rel_x < ED_GUTTER_W || rel_y >= GetScreenHeight() / 2 - ED_STATUS_H) return;

    int clicked_line = ed.scroll_y + (int)(rel_y / ED_LINE_H);
    if (clicked_line >= ed.nlines) clicked_line = ed.nlines - 1;
    ed.cur_line = clicked_line;

    // Find closest character column by measuring text widths
    float tx = rel_x - ED_GUTTER_W - ED_PAD;
    const char *ln = ed.lines[ed.cur_line];
    int len = (int)strlen(ln);
    int best_col = 0;
    float best_dist = 1e9f;
    char buf[ED_MAX_LINE];
    for (int c = 0; c <= len; c++) {
        strncpy(buf, ln, c);
        buf[c] = '\0';
        float w = MeasureTextEx(font, buf, ED_FONT_SIZE, 1).x;
        float dist = fabsf(w - tx);
        if (dist < best_dist) { best_dist = dist; best_col = c; }
    }
    ed.cur_col    = best_col;
    ed.sel_active = false;
}

static void ed_render(RenderTexture2D rt, Font font) {
    int pw = rt.texture.width;
    int ph = rt.texture.height;
    int text_area_h  = ph - ED_STATUS_H;
    int visible_lines = text_area_h / ED_LINE_H;

    BeginTextureMode(rt);
    ClearBackground((Color){18, 18, 28, 255});

    // Gutter background + separator
    DrawRectangle(0, 0, ED_GUTTER_W, text_area_h, (Color){12, 12, 20, 255});
    DrawLine(ED_GUTTER_W, 0, ED_GUTTER_W, text_area_h, (Color){40, 40, 65, 255});

    bool cursor_on = (int)(GetTime() * 2.0) % 2 == 0;

    int sel_sl=0, sel_sc=0, sel_el=0, sel_ec=0;
    if (ed.sel_active) sel_range(&sel_sl, &sel_sc, &sel_el, &sel_ec);

    for (int i = 0; i < visible_lines; i++) {
        int li = ed.scroll_y + i;
        if (li >= ed.nlines) break;

        int y = i * ED_LINE_H + ED_PAD;

        // Current line highlight
        if (li == ed.cur_line)
            DrawRectangle(ED_GUTTER_W, y - ED_PAD + 1, pw - ED_GUTTER_W, ED_LINE_H,
                          (Color){28, 30, 48, 255});

        // Selection highlight
        if (ed.sel_active && li >= sel_sl && li <= sel_el) {
            int hsc = (li == sel_sl) ? sel_sc : 0;
            int hec = (li == sel_el) ? sel_ec : (int)strlen(ed.lines[li]);
            bool extends = (li < sel_el);

            char tmp[ED_MAX_LINE];
            strncpy(tmp, ed.lines[li], hsc); tmp[hsc] = '\0';
            float x0 = MeasureTextEx(font, tmp, ED_FONT_SIZE, 1).x;

            float x1;
            if (extends) {
                x1 = (float)(pw - ED_GUTTER_W - ED_PAD);
            } else {
                strncpy(tmp, ed.lines[li], hec); tmp[hec] = '\0';
                x1 = MeasureTextEx(font, tmp, ED_FONT_SIZE, 1).x;
            }
            float w = (x1 > x0) ? (x1 - x0) : (extends ? 8.0f : 0.0f);
            if (w > 0)
                DrawRectangle((int)(ED_GUTTER_W + ED_PAD + x0), y - ED_PAD + 1,
                              (int)w, ED_LINE_H, (Color){65, 110, 210, 120});
        }

        // Line number (right-aligned in gutter)
        char num[8];
        snprintf(num, sizeof num, "%d", li + 1);
        float nw = MeasureTextEx(font, num, ED_FONT_SIZE, 1).x;
        Color num_col = (li == ed.cur_line)
            ? (Color){160, 160, 210, 255}
            : (Color){ 55,  55,  85, 255};
        DrawTextEx(font, num, (Vector2){ED_GUTTER_W - nw - 6, (float)y}, ED_FONT_SIZE, 1, num_col);

        // Line text
        DrawTextEx(font, ed.lines[li],
                   (Vector2){ED_GUTTER_W + ED_PAD, (float)y},
                   ED_FONT_SIZE, 1, (Color){200, 205, 220, 255});

        // Cursor — block on blank lines (always visible), thin bar on text lines
        if (li == ed.cur_line) {
            int   ln_len = (int)strlen(ed.lines[li]);
            bool  blank  = (ln_len == 0);
            // Don't blink on blank lines so the cursor is never invisible
            if (blank || cursor_on) {
                int bc = (ed.cur_col < ln_len) ? ed.cur_col : ln_len;
                char before[ED_MAX_LINE];
                strncpy(before, ed.lines[li], bc);
                before[bc] = '\0';
                float cx = MeasureTextEx(font, before, ED_FONT_SIZE, 1).x;
                int cw = blank ? 8 : 2;
                DrawRectangle((int)(ED_GUTTER_W + ED_PAD + cx), y, cw, ED_FONT_SIZE,
                              (Color){200, 220, 255, 220});
            }
        }
    }

    // Status bar
    DrawRectangle(0, text_area_h, pw, ED_STATUS_H, (Color){25, 25, 45, 255});
    DrawLine(0, text_area_h, pw, text_area_h, (Color){45, 45, 75, 255});

    char left[256];
    snprintf(left, sizeof left, "  %s%s   %d:%d",
             ed.has_file ? ed.filename : "(no file)",
             ed.dirty ? " *" : "",
             ed.cur_line + 1, ed.cur_col + 1);
    DrawTextEx(font, left,
               (Vector2){4, text_area_h + 5}, ED_FONT_SIZE - 1, 1,
               (Color){110, 115, 155, 255});

    // Ctrl+R hint or recent message
    const char *right_msg = (ed.msg[0] && GetTime() - ed.msg_time < 2.5)
                            ? ed.msg : "Ctrl+R: save & run";
    Color right_col = (ed.msg[0] && GetTime() - ed.msg_time < 2.5)
                      ? (Color){100, 220, 130, 255}
                      : (Color){ 55,  60,  95, 255};
    float rw = MeasureTextEx(font, right_msg, ED_FONT_SIZE - 1, 1).x;
    DrawTextEx(font, right_msg,
               (Vector2){pw - rw - 8, text_area_h + 5},
               ED_FONT_SIZE - 1, 1, right_col);

    EndTextureMode();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    for (int x = 0; x < GRID_SIZE; x++)
        for (int y = 0; y < GRID_SIZE; y++)
            for (int z = 0; z < GRID_SIZE; z++)
                grid[x][y][z] = 1;
    reset_all_colors();

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "set_cube",            l_set_cube);
    lua_register(L, "get_cube",            l_get_cube);
    lua_register(L, "clear",               l_clear);
    lua_register(L, "fill",                l_fill);
    lua_register(L, "set_layer",           l_set_layer);
    lua_register(L, "set_color",           l_set_color);
    lua_register(L, "set_color_hsv",       l_set_color_hsv);
    lua_register(L, "set_layer_color",     l_set_layer_color);
    lua_register(L, "set_layer_color_hsv", l_set_layer_color_hsv);
    lua_register(L, "reset_colors",        l_reset_colors);
    lua_register(L, "get_time",            l_get_time);
    lua_register(L, "stop",                l_stop);
    lua_pushinteger(L, GRID_SIZE);
    lua_setglobal(L, "grid_size");

    // Load editor + run startup script
    if (argc > 1) {
        ed_load(argv[1]);
        if (luaL_dofile(L, argv[1]) != LUA_OK) {
            fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    } else {
        ed.has_file = false;
        ed.nlines   = 1;
        ed.lines[0][0] = '\0';
    }

    pthread_t tid;
    pthread_create(&tid, NULL, stdin_thread, NULL);
    pthread_detach(tid);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Grid Viewer");
    SetTargetFPS(60);

    // Try to load a monospace font; fall back to default
    Font font = LoadFontEx("/System/Library/Fonts/Menlo.ttc", ED_FONT_SIZE * 2, NULL, 0);
    bool font_loaded = (font.glyphCount > 0);
    if (!font_loaded) font = GetFontDefault();
    if (font_loaded) SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    int SW = GetScreenWidth();
    int SH = GetScreenHeight();
    int PW = SW / 2;
    int PH = SH / 2;

    RenderTexture2D rt_top    = LoadRenderTexture(PW, PH);
    RenderTexture2D rt_side   = LoadRenderTexture(PW, PH);
    RenderTexture2D rt_3d     = LoadRenderTexture(PW, PH);
    RenderTexture2D rt_editor = LoadRenderTexture(PW, PH);

    float ortho_fovy = (GRID_SIZE - 1) * CUBE_SPACING * 1.25f;

    Camera3D top_cam   = {0};
    top_cam.position   = (Vector3){0, 50, 0};
    top_cam.target     = (Vector3){0,  0, 0};
    top_cam.up         = (Vector3){0,  0, -1};
    top_cam.fovy       = ortho_fovy;
    top_cam.projection = CAMERA_ORTHOGRAPHIC;

    Camera3D side_cam   = {0};
    side_cam.position   = (Vector3){50, 0, 0};
    side_cam.target     = (Vector3){ 0, 0, 0};
    side_cam.up         = (Vector3){ 0, 1, 0};
    side_cam.fovy       = ortho_fovy;
    side_cam.projection = CAMERA_ORTHOGRAPHIC;

    float    yaw    =  0.8f;
    float    pitch  =  0.4f;
    float    radius = 40.0f;
    Camera3D cam    = {0};
    cam.up          = (Vector3){0, 1, 0};
    cam.fovy        = 45.0f;
    cam.projection  = CAMERA_PERSPECTIVE;

    Rectangle panel_3d     = {0,        (float)PH, (float)PW, (float)PH};
    Rectangle panel_editor = {(float)PW, (float)PH, (float)PW, (float)PH};

    int visible_lines = (PH - ED_STATUS_H) / ED_LINE_H;

    Color bg_persp  = (Color){20, 20, 20, 255};
    Color bg_ortho  = (Color){12, 12, 20, 255};
    Color col_border = (Color){55, 55, 70, 255};
    Color col_label  = (Color){140, 140, 180, 255};

    Rectangle src = {0, 0, (float)PW, -(float)PH};  // negative H flips texture Y

    while (!WindowShouldClose()) {
        Vector2 mouse  = GetMousePosition();
        bool in_3d     = CheckCollisionPointRec(mouse, panel_3d);
        bool in_editor = CheckCollisionPointRec(mouse, panel_editor);
        bool ctrl      = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        // 3D camera controls
        if (in_3d) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 d = GetMouseDelta();
                yaw   -= d.x * 0.005f;
                pitch -= d.y * 0.005f;
                if (pitch >  1.45f) pitch =  1.45f;
                if (pitch < -1.45f) pitch = -1.45f;
            }
            radius -= GetMouseWheelMove() * 1.2f;
            if (radius <  5.0f) radius =  5.0f;
            if (radius > 70.0f) radius = 70.0f;
        }

        // Ctrl+W toggles wireframes (Ctrl avoids conflict with editor 'w')
        if (ctrl && IsKeyPressed(KEY_W)) show_wires = !show_wires;

        // Ctrl+R: save editor content and reload Lua
        if (ctrl && IsKeyPressed(KEY_R) && ed.has_file) {
            if (ed_save()) {
                lua_pushnil(L);
                lua_setglobal(L, "update");
                if (luaL_dofile(L, ed.filename) != LUA_OK) {
                    const char *err = lua_tostring(L, -1);
                    fprintf(stderr, "lua error: %s\n", err);
                    snprintf(ed.msg, sizeof ed.msg, "Error: %s", err);
                    lua_pop(L, 1);
                } else {
                    snprintf(ed.msg, sizeof ed.msg, "Reloaded!");
                }
                ed.msg_time = GetTime();
            }
        }

        // Editor mouse click (only in editor panel)
        if (in_editor)
            ed_handle_click((Vector2){(float)PW, (float)PH}, font);

        // Editor keyboard input — ctrl check is handled inside
        ed_handle_input(visible_lines);

        cam.position = (Vector3){
            radius * cosf(pitch) * sinf(yaw),
            radius * sinf(pitch),
            radius * cosf(pitch) * cosf(yaw),
        };
        cam.target = (Vector3){0};

        // Drain Lua command queue
        pthread_mutex_lock(&q_lock);
        while (q_head != q_tail) {
            char cmd[CMD_MAX];
            strncpy(cmd, cmd_queue[q_head], CMD_MAX - 1);
            cmd[CMD_MAX - 1] = '\0';
            q_head = (q_head + 1) % QUEUE_CAP;
            pthread_mutex_unlock(&q_lock);

            if (luaL_dostring(L, cmd) != LUA_OK) {
                fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
            } else if (lua_gettop(L) > 0) {
                int n = lua_gettop(L);
                for (int i = 1; i <= n; i++)
                    fprintf(stderr, "%s%s", lua_tostring(L, i), i < n ? "\t" : "\n");
                lua_settop(L, 0);
            }

            pthread_mutex_lock(&q_lock);
        }
        pthread_mutex_unlock(&q_lock);

        // Per-frame Lua update callback
        lua_getglobal(L, "update");
        if (lua_isfunction(L, -1)) {
            lua_pushnumber(L, GetFrameTime());
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                fprintf(stderr, "lua error in update: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_setglobal(L, "update");
            }
        } else {
            lua_pop(L, 1);
        }

        // ── Render each panel to its texture ────────────────────────────

        BeginTextureMode(rt_top);
            ClearBackground(bg_ortho);
            BeginMode3D(top_cam);  draw_grid();  EndMode3D();
        EndTextureMode();

        BeginTextureMode(rt_side);
            ClearBackground(bg_ortho);
            BeginMode3D(side_cam); draw_grid();  EndMode3D();
        EndTextureMode();

        BeginTextureMode(rt_3d);
            ClearBackground(bg_persp);
            BeginMode3D(cam);
                draw_grid();
                draw_axes();
            EndMode3D();
            // Axis labels — projected to render-texture space
            {
                float off  = (GRID_SIZE - 1) * CUBE_SPACING * 0.5f;
                float tip  = (GRID_SIZE - 1) * CUBE_SPACING + 1.2f + 0.9f + 0.3f;
                Vector2 lx = GetWorldToScreenEx((Vector3){-off+tip, -off,     -off    }, cam, PW, PH);
                Vector2 ly = GetWorldToScreenEx((Vector3){-off,     -off+tip, -off    }, cam, PW, PH);
                Vector2 lz = GetWorldToScreenEx((Vector3){-off,     -off,     -off+tip}, cam, PW, PH);
                DrawText("X", (int)lx.x + 4, (int)lx.y - 8, 16, RED);
                DrawText("Y", (int)ly.x + 4, (int)ly.y - 8, 16, GREEN);
                DrawText("Z", (int)lz.x + 4, (int)lz.y - 8, 16, BLUE);
            }
        EndTextureMode();

        ed_render(rt_editor, font);

        // ── Composite to screen ──────────────────────────────────────────

        BeginDrawing();
        ClearBackground(BLACK);

        DrawTextureRec(rt_top.texture,    src, (Vector2){0,        0},        WHITE);
        DrawTextureRec(rt_side.texture,   src, (Vector2){(float)PW, 0},       WHITE);
        DrawTextureRec(rt_3d.texture,     src, (Vector2){0,        (float)PH}, WHITE);
        DrawTextureRec(rt_editor.texture, src, (Vector2){(float)PW, (float)PH}, WHITE);

        // Dividers
        DrawLine(PW, 0,  PW, SH, col_border);
        DrawLine(0,  PH, SW, PH, col_border);

        // Panel labels
        DrawText("TOP",  8,       8,      13, col_label);
        DrawText("SIDE", PW + 8,  8,      13, col_label);
        DrawText("3D",   8,       PH + 8, 13, LIGHTGRAY);

        // Help in 3D panel corner
        DrawText("drag: rotate  scroll: zoom  Ctrl+W: wires",
                 8, PH + PH - 22, 12, (Color){70, 70, 95, 255});

        DrawFPS(SW - 70, 8);
        EndDrawing();
    }

    UnloadRenderTexture(rt_top);
    UnloadRenderTexture(rt_side);
    UnloadRenderTexture(rt_3d);
    UnloadRenderTexture(rt_editor);
    if (font_loaded) UnloadFont(font);
    CloseWindow();
    lua_close(L);
    return 0;
}
