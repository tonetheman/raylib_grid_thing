-- target1.lua
-- Goal: draw a flat 10x10 square on the ground layer (y=0)
-- Hint: use two nested for loops with set_cube() and set_color_hsv()

clear()
for x = 5, 14 do
    for z = 5, 14 do
        set_cube(x, 0, z, true)
        set_color_hsv(x, 0, z, 210, 0.6, 0.9)
    end
end
