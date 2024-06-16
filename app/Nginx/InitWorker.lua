-- To configure nginx.conf
-- Before server {} declaration:
--[[
	init_worker_by_lua_block {
		package.path = package.path .. ";/path/to/lua/?.lua"
		package.cpath = package.cpath .. ";/path/to/luabin/?.dll;/path/toluabin/?.so"
		require "InitWorker"
	}
]]

local InitWorker = {}
-- ngx.log(ngx.ERR, "Coluster Initializing ... ")
local colusterThreadCount = 4
local colusterTickInterval = 0.03
local coluster = require("coluster").new()

local function Teminate()
	coluster:Stop()
	ngx.log(ngx.ERR, "Coluster instance exiting with timer message: " .. tostring(err))

	coluster:Join(function ()
	end)

	ngx.log(ngx.ERR, "Coluster instance exited.")
end

local function SetupTimer(func)
	if not coluster:IsWorkerTerminated() then
		local ok, err = ngx.timer.at(colusterTickInterval, func)
		if not ok then
			Terminate()
		end
	end
end

local frameIndex = 0

local function MainLoop(premature)
	coluster:Poll(false) -- do not poll async tasks
	SetupTimer(MainLoop)
end

coluster:Start(colusterThreadCount)
SetupTimer(MainLoop)

function InitWorker.GetColuster()
	return coluster
end

-- ngx.log(ngx.ERR, "Coluster Initialized.")
return setmetatable(InitWorker, { __gc = function ()
	ngx.log(ngx.ERR, "Coluster instance terminated.")
		Terminate()
end })