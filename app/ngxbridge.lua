-- must run under ngx_lua environment
if not ngx then
	ngx = require("ngxsimulator") -- just for test
end

local ngxbridge = {}
local colusterThreadCount = ngx.var.coluster_thread_count or 4 -- 4 threads by default
local colusterTickInterval = (ngx.var.coluster_tick_interval or 10) / 1000.0 -- 10 ms by default

--[[
if package.cpath:find(".so") and not package.cpath:find("%./lib%?%.so") then
	package.cpath = package.cpath .. ";./lib?.so"
	print(package.cpath)
end
]]

local coluster = assert(require("coluster").new())
if not coluster:Start(colusterThreadCount) then
	error("Coluster startup failed!")
end

--[[
local services = {}
coluster:Post(function ()
	print("Start!")
	print("Initializing coluster services ...")
	
	local function Service(name)
		local serviceModule = assert(require("service/" .. name))
		print("New service: " .. name)
		local value = serviceModule.New(coluster, services)
		if value then
			services[name] = value
			value.__name = name
			table.insert(services, value)
		else
			print("Service initialization error: " .. value)
		end
	end
	
	Service("storage")
	Service("database")
	Service("luabridge")
	Service("device")
	Service("coordinator")
	Service("example")
	
	print("Initializing coluster services complete.")
end)
]]

local function SetupTimer(func)
	if not coluster:IsWorkerTerminated() then
		local ok, err = ngx.timer.at(colusterTickInterval, func)
		if not ok then
			coluster:Stop()
			ngx.log(ngx.INFO, "Coluster instance exiting with timer message: " .. tostring(err))

			coluster:Join(function ()
				--[[
				for i = #services, 1, -1 do
					local service = services[i]
					print("Deleting service: " .. service.__name)
					service:Delete(services)
				end
				]]
			end)

			ngx.log(ngx.INFO, "Coluster instance exited.")
		end
	end
end

local function MainLoop(premature)
	coluster:Poll(false) -- do not poll async tasks
	SetupTimer(MainLoop)
end

SetupTimer(MainLoop)
ngxbridge.coluster = coluster

return setmetatable(ngxbridge, { __gc = function ()
	coluster:Terminate()
end })