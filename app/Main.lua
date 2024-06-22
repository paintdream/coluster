if package.cpath:find(".so") and not package.cpath:find("%./lib%?%.so") then
	package.cpath = package.cpath .. ";./lib?.so"
end

-- for debugging from Visual Studio
local platform = os.getenv('OS')
if platform and platform:find("^Windows") then
	package.cpath = package.cpath .. ";../build32/Debug/?.dll"
end

warn = function (message)
	print("[Warning] " .. message)
end

local coluster = assert(require("coluster").new())
local services = {}
coluster:Start(4)
coluster:Post(function ()
	print("Start!")
	print("Initializing coluster services ...")
	
	local function Service(name)
		local serviceModule = assert(require("Service/" .. name))
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
	
	Service("Storage")
	Service("Database")
	Service("LuaBridge")
	Service("PyBridge")
	Service("Device")
	Service("Coordinator")
	Service("Example")
	
	print("Initializing coluster services complete.")
end)

for i = 1, 8 do
	coluster:Poll()
	for co, info in pairs(coluster:GetProfile().trace) do
		print("------------------")
		print("[C] " .. info)
		print("[Lua] " .. debug.traceback(co))
	end
	print("------------------")
end

coluster:Join(function ()
	for i = #services, 1, -1 do
		local service = services[i]
		print("Deleting service: " .. service.__name)
		service:Delete(services)
	end
end, true)
