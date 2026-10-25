-- input: stock LIPC; output: desktop readiness for powerd; pos: replacement desktop lifecycle.
-- powerd rejects the physical key if either frameworkStarted or blanket.load is missing.
local lipc = require('liblipclua')
local desktop = assert(lipc.init('com.lab126.kaf'))
local ready = desktop:register_int_property('frameworkStarted', 'r')
ready.value = 1
local blanket = assert(lipc.init('com.lab126.blanket'))
local screen = blanket:register_string_property('load', 'r')
screen.value = ''
local power = 'com.lab126.powerd'
local brightness, pausedGame
local function gameProcess()
    local f = io.open('/tmp/game.pid'); if not f then return nil end
    local pid = tonumber(f:read('*a')); f:close()
    if not pid or pid < 2 then return nil end
    f = io.open('/proc/' .. pid .. '/cmdline'); if not f then return nil end
    local cmd = f:read('*a'); f:close()
    if cmd:find('/doom%-voyage') or cmd:find('/arcade%-voyage') then return pid end
end
desktop:subscribe(power, 'goingToScreenSaver', function()
    if brightness == nil then brightness = desktop:get_int_property(power, 'flIntensity') end
    desktop:set_int_property(power, 'flIntensity', 0)
    pausedGame = gameProcess()
    if pausedGame then
        os.execute('kill -USR1 ' .. pausedGame)
    end
end)
desktop:subscribe(power, 'outOfScreenSaver', function()
    if brightness ~= nil then desktop:set_int_property(power, 'flIntensity', brightness); brightness = nil end
    if pausedGame and gameProcess() == pausedGame then os.execute('kill -CONT ' .. pausedGame) end
    pausedGame = nil
end)
-- Native liblipclua timeout is in seconds. Blocking dispatch has no idle polling.
while true do lipc.run_event_loop(86400) end
