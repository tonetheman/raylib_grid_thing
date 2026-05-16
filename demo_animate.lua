-- demo_animate.lua
-- Four animation modes. Switch between them from the REPL by calling:
--   wave()   helix()   rain()   pulse()   stop()

local N = grid_size - 1          -- max index (19 for 20x20x20)
local C = (grid_size - 1) / 2.0  -- center (9.5)

local function clamp(v, lo, hi)
    return math.max(lo, math.min(hi, v))
end

local function iclamp(v)
    return math.max(0, math.min(N, math.floor(v)))
end

local function set_update(fn)
    clear()
    update = fn
end

-- ── 1. wave ───────────────────────────────────────────────────────────────
-- A rippling sine-wave surface with rainbow color.

function wave()
    set_update(function(dt)
        local t = get_time()
        local scale = 6.0 / N   -- keep wave frequency consistent across grid sizes
        for x = 0, N do
            for z = 0, N do
                local h = math.sin(x * scale + t * 2.5) * math.cos(z * scale + t * 1.8)
                local y = iclamp((h * 0.5 + 0.5) * N + 0.5)
                for yy = 0, N do
                    set_cube(x, yy, z, yy == y)
                end
                local hue = ((x + z) / (N * 2.0) * 360 + t * 40) % 360
                set_color_hsv(x, y, z, hue, 1.0, 1.0)
            end
        end
    end)
end

-- ── 2. helix ──────────────────────────────────────────────────────────────
-- Two interleaved helixes rotating around the Y axis.

function helix()
    local r = C * 0.75  -- helix radius
    set_update(function(dt)
        local t = get_time()
        clear()
        for yi = 0, N do
            local angle = yi / N * math.pi * 4 + t * 2
            for strand = 0, 1 do
                local a   = angle + strand * math.pi
                local x   = iclamp(math.cos(a) * r + C)
                local z   = iclamp(math.sin(a) * r + C)
                local hue = (yi / N * 300 + strand * 180 + t * 60) % 360
                set_cube(x, yi, z, true)
                set_color_hsv(x, yi, z, hue, 1.0, 1.0)
            end
        end
    end)
end

-- ── 3. rain ───────────────────────────────────────────────────────────────
-- Colored droplets fall and stack at the bottom.

function rain()
    local drops = {}
    local floor_h = {}
    for x = 0, N do
        floor_h[x] = {}
        for z = 0, N do floor_h[x][z] = -1 end
    end

    local timer   = 0
    local speed   = grid_size * 0.8  -- fall speed scales with grid height

    set_update(function(dt)
        timer = timer + dt
        clear()

        -- spawn a new drop every ~60ms
        if timer > 0.06 then
            timer = 0
            local x = math.random(0, N)
            local z = math.random(0, N)
            local key = x * grid_size + z
            if not drops[key] then
                drops[key] = { x=x, z=z, y=N + 0.0, hue=math.random(0, 360) }
            end
        end

        -- move drops
        for key, d in pairs(drops) do
            local bottom = floor_h[d.x][d.z] + 1
            d.y = d.y - speed * dt
            if d.y <= bottom then
                floor_h[d.x][d.z] = bottom
                drops[key] = nil
            else
                local yi = iclamp(d.y)
                set_cube(d.x, yi, d.z, true)
                set_color_hsv(d.x, yi, d.z, d.hue, 1.0, 1.0)
            end
        end

        -- draw stacked cubes
        for x = 0, N do
            for z = 0, N do
                for y = 0, floor_h[x][z] do
                    set_cube(x, y, z, true)
                    local hue = ((x + z + y) / (N * 3.0) * 360) % 360
                    set_color_hsv(x, y, z, hue, 0.8, 0.9)
                end
            end
        end
    end)
end

-- ── 4. pulse ──────────────────────────────────────────────────────────────
-- Concentric spherical shells expanding outward from centre.

function pulse()
    local max_r = C * math.sqrt(3)
    set_update(function(dt)
        local t = get_time()
        local r = (t * 4.0) % max_r
        for x = 0, N do
            for y = 0, N do
                for z = 0, N do
                    local dist = math.sqrt((x-C)^2 + (y-C)^2 + (z-C)^2)
                    local on   = math.abs(dist - r) < 1.2
                    set_cube(x, y, z, on)
                    if on then
                        local hue = (dist / max_r * 300 + t * 60) % 360
                        set_color_hsv(x, y, z, hue, 1.0, 1.0)
                    end
                end
            end
        end
    end)
end

-- ── start with wave by default ────────────────────────────────────────────
wave()
