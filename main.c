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

// set_layer("x"|"y"|"z", index, bool)
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

// set_color(x, y, z, r, g, b)  -- r/g/b: 0-255
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

// set_color_hsv(x, y, z, h, s, v)  -- h: 0-360, s/v: 0.0-1.0
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

// set_layer_color(axis, index, r, g, b)
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

// set_layer_color_hsv(axis, index, h, s, v)
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

// reset_colors() -- restore default Rubik's cube face colors
static int l_reset_colors(lua_State *L) {
    (void)L;
    reset_all_colors();
    return 0;
}

// get_time() -- seconds since window opened
static int l_get_time(lua_State *L) {
    lua_pushnumber(L, GetTime());
    return 1;
}

// stop() -- clear the update function
static int l_stop(lua_State *L) {
    (void)L;
    lua_pushnil(L);
    lua_setglobal(L, "update");
    return 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    // Start with a full solid cube, default colors
    for (int x = 0; x < GRID_SIZE; x++)
        for (int y = 0; y < GRID_SIZE; y++)
            for (int z = 0; z < GRID_SIZE; z++)
                grid[x][y][z] = 1;
    reset_all_colors();

    // Init Lua
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "set_cube",           l_set_cube);
    lua_register(L, "get_cube",           l_get_cube);
    lua_register(L, "clear",              l_clear);
    lua_register(L, "fill",               l_fill);
    lua_register(L, "set_layer",          l_set_layer);
    lua_register(L, "set_color",          l_set_color);
    lua_register(L, "set_color_hsv",      l_set_color_hsv);
    lua_register(L, "set_layer_color",    l_set_layer_color);
    lua_register(L, "set_layer_color_hsv",l_set_layer_color_hsv);
    lua_register(L, "reset_colors",       l_reset_colors);
    lua_register(L, "get_time",           l_get_time);
    lua_register(L, "stop",               l_stop);

    lua_pushinteger(L, GRID_SIZE);
    lua_setglobal(L, "grid_size");

    // Run optional startup script
    if (argc > 1) {
        if (luaL_dofile(L, argv[1]) != LUA_OK) {
            fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    // Start stdin reader thread
    pthread_t tid;
    pthread_create(&tid, NULL, stdin_thread, NULL);
    pthread_detach(tid);

    // Raylib window
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "9x9x9 Grid Viewer");
    SetTargetFPS(60);

    // Orbital camera state
    float yaw       =  0.8f;
    float pitch     =  0.4f;
    float radius    = 40.0f;
    bool  show_wires = true;

    Camera3D cam  = {0};
    cam.up         = (Vector3){0.0f, 1.0f, 0.0f};
    cam.fovy       = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        // Rotate on left-drag
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            yaw   -= d.x * 0.005f;
            pitch -= d.y * 0.005f;
            if (pitch >  1.45f) pitch =  1.45f;
            if (pitch < -1.45f) pitch = -1.45f;
        }

        // Scroll to zoom
        radius -= GetMouseWheelMove() * 1.2f;
        if (radius <  5.0f) radius =  5.0f;
        if (radius > 70.0f) radius = 70.0f;

        if (IsKeyPressed(KEY_W)) show_wires = !show_wires;

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

        // Call update(dt) each frame if the global is a function
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

        // Draw
        BeginDrawing();
        ClearBackground((Color){20, 20, 20, 255});

        BeginMode3D(cam);

        float offset = (GRID_SIZE - 1) * CUBE_SPACING * 0.5f;
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                for (int z = 0; z < GRID_SIZE; z++) {
                    if (!grid[x][y][z]) continue;

                    // skip interior cubes — all 6 neighbours are on, so never visible
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

        EndMode3D();

        DrawFPS(10, 10);
        DrawText("Left-drag: rotate   Scroll: zoom   W: toggle wires", 10, 40, 16, LIGHTGRAY);
        DrawText("Colors: set_color_hsv(x,y,z,h,s,v)  set_layer_color_hsv(axis,i,h,s,v)  reset_colors()",
                 10, 62, 13, (Color){130, 130, 130, 255});
        DrawText("Animate: define update(dt) in Lua  |  get_time()  |  stop()",
                 10, 80, 13, (Color){130, 130, 130, 255});

        EndDrawing();
    }

    CloseWindow();
    lua_close(L);
    return 0;
}
