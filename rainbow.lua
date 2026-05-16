-- rainbow.lua
-- Run as: ./grid_viewer rainbow.lua
-- Or paste individual blocks into the terminal REPL.

local N = grid_size - 1

-- ── 1. Axis stripes (default on load) ────────────────────────────────────
fill()
for x = 0, N do
    set_layer_color_hsv("x", x, x / N * 270, 1.0, 1.0)
end

-- ── 2. Concentric spherical shells ───────────────────────────────────────
-- fill()
-- local C = N / 2.0
-- for x = 0, N do
--     for y = 0, N do
--         for z = 0, N do
--             local dist = math.sqrt((x-C)^2 + (y-C)^2 + (z-C)^2)
--             local hue  = dist / (C * math.sqrt(3)) * 300
--             set_color_hsv(x, y, z, hue, 1.0, 1.0)
--         end
--     end
-- end

-- ── 3. Diagonal gradient ─────────────────────────────────────────────────
-- fill()
-- for x = 0, N do
--     for y = 0, N do
--         for z = 0, N do
--             local t   = (x + y + z) / (N * 3.0)
--             set_color_hsv(x, y, z, t * 300, 0.9, 1.0)
--         end
--     end
-- end
