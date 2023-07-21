if package.cpath:find(".so") and not package.cpath:find("%./lib%?%.so") then
	package.cpath = package.cpath .. ";./lib?.so"
	print(package.cpath)
end

local coluster = assert(require("coluster").create())
local services = {}
coluster:Start(4)
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

coluster:Join(function ()
	for i = #services, 1, -1 do
		local service = services[i]
		print("Deleting service: " .. service.__name)
		service:Delete(services)
	end
end, true)
