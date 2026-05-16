-- demo.lua: example shapes you can paste into the terminal REPL

-- Hollow shell (only outer layer visible)
-- clear()
-- for x = 0, 8 do
--   for y = 0, 8 do
--     for z = 0, 8 do
--       local shell = (x==0 or x==8 or y==0 or y==8 or z==0 or z==8)
--       set_cube(x, y, z, shell)
--     end
--   end
-- end

-- Diagonal stripe across the Y axis
clear()
for i = 0, 8 do
    set_cube(i, i, i, true)
    set_cube(8-i, i, i, true)
    set_cube(i, i, 8-i, true)
    set_cube(8-i, i, 8-i, true)
end

-- Ring around the middle Y slice
for x = 0, 8 do
    set_cube(x, 4, 0, true)
    set_cube(x, 4, 8, true)
end
for z = 1, 7 do
    set_cube(0, 4, z, true)
    set_cube(8, 4, z, true)
end
