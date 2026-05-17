# Grid Viewer — Lua API Reference

Grid is **20×20×20** (indices `0` to `19` on each axis).  
The constant `grid_size` (= 20) is available in all scripts.

---

## Coordinates

| Axis | Direction | Color in 3D view |
|------|-----------|-----------------|
| X    | left → right  | Red   |
| Y    | bottom → top  | Green |
| Z    | back → front  | Blue  |

Origin `(0,0,0)` is the bottom-left-back corner of the grid.

---

## On / Off

```lua
set_cube(x, y, z, bool)
```
Turn a single cube on (`true`) or off (`false`).

```lua
get_cube(x, y, z)  --> bool
```
Returns whether a cube is currently on.

```lua
clear()
```
Turn **all** cubes off.

```lua
fill()
```
Turn **all** cubes on.

```lua
set_layer("x"|"y"|"z", index, bool)
```
Turn an entire slice on or off.  
`index` is `0`–`19`.  Examples:
```lua
set_layer("y", 0, false)   -- clear the bottom layer
set_layer("x", 10, true)   -- fill the middle X slice
```

---

## Color

Colors are independent of the on/off state — you can set a color whether the cube is on or off.

```lua
set_color(x, y, z, r, g, b)
```
Set color with RGB values `0`–`255`.

```lua
set_color_hsv(x, y, z, h, s, v)
```
Set color with HSV: `h` = 0–360, `s` = 0.0–1.0, `v` = 0.0–1.0.

```lua
set_layer_color("x"|"y"|"z", index, r, g, b)
```
Set RGB color for every cube in a slice.

```lua
set_layer_color_hsv("x"|"y"|"z", index, h, s, v)
```
Set HSV color for every cube in a slice.

```lua
reset_colors()
```
Restore the default Rubik's-cube face colors:
- Top (Y=19): white
- Bottom (Y=0): yellow
- Front (Z=19): red
- Back (Z=0): orange
- Right (X=19): green
- Left (X=0): blue
- Interior: dark gray

---

## Animation

Define a global function called `update` and it will be called every frame:

```lua
function update(dt)
    -- dt = seconds since last frame (typically ~0.016 at 60 fps)
end
```

```lua
get_time()  --> float
```
Seconds elapsed since the window opened.  Use this for time-based animation.

```lua
stop()
```
Clears the `update` function so animation stops.  Equivalent to `update = nil`.

---

## Examples

### Rainbow X stripes
```lua
fill()
for x = 0, grid_size - 1 do
    set_layer_color_hsv("x", x, x / (grid_size-1) * 270, 1.0, 1.0)
end
```

### Checkerboard on the top face
```lua
clear()
local top = grid_size - 1
for x = 0, top do
    for z = 0, top do
        if (x + z) % 2 == 0 then
            set_cube(x, top, z, true)
            set_color_hsv(x, top, z, (x + z) * 10, 0.8, 1.0)
        end
    end
end
```

### Spinning wave (animation)
```lua
fill()
function update(dt)
    local t = get_time()
    for x = 0, grid_size - 1 do
        for z = 0, grid_size - 1 do
            local wave = math.sin(x * 0.7 + t * 2) * math.cos(z * 0.7 + t * 1.5)
            local y    = math.floor((wave * 0.5 + 0.5) * (grid_size - 1))
            for yy = 0, grid_size - 1 do
                set_cube(x, yy, z, yy == y)
            end
            set_color_hsv(x, y, z, (x + z) / (grid_size * 2.0) * 360, 1.0, 1.0)
        end
    end
end
```

### Hollow shell
```lua
fill()
local N = grid_size - 1
for x = 0, N do
    for y = 0, N do
        for z = 0, N do
            local shell = (x==0 or x==N or y==0 or y==N or z==0 or z==N)
            set_cube(x, y, z, shell)
        end
    end
end
```

---

## Controls

### 3D view

| Input | Action |
|-------|--------|
| Left-drag | Rotate camera |
| Scroll | Zoom in / out |
| Ctrl+W | Toggle wireframes |

### Editor

#### Navigation

| Key | Action |
|-----|--------|
| Arrow keys | Move cursor |
| Home / End | Start / end of line |
| Page Up / Page Down | Move one screenful |
| Click | Place cursor |

#### Selection

Hold **Shift** with any navigation key to extend the selection.  
The selected region is highlighted in blue.

| Key | Action |
|-----|--------|
| Shift+Arrow | Extend/shrink selection one character or line |
| Shift+Home / Shift+End | Extend to start / end of line |
| Shift+Page Up / Shift+Page Down | Extend by one screenful |
| Ctrl+A | Select all |
| Click | Clear selection and place cursor |

#### Editing

| Key | Action |
|-----|--------|
| Type | Insert character (replaces selection if active) |
| Tab | Insert 4 spaces (replaces selection if active) |
| Enter | New line (replaces selection if active) |
| Backspace | Delete character left (deletes selection if active) |
| Delete | Delete character right (deletes selection if active) |

#### Clipboard

| Key | Action |
|-----|--------|
| Ctrl+C | Copy selection; copies current line if nothing selected |
| Ctrl+X | Cut selection; cuts current line if nothing selected |
| Ctrl+V | Paste from clipboard; replaces selection if active |

#### File

| Key | Action |
|-----|--------|
| Ctrl+R | Save file to disk and reload it into the Lua interpreter |
