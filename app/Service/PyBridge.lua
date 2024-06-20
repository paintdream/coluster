local PyBridge = {}
PyBridge.__index = PyBridge

function PyBridge.New(coluster, services)
	local instance = {}
	setmetatable(instance, PyBridge)
	local import = require("Util/Import")

	local status, message = pcall(function ()
		require("pybridge")
		instance.object = mod.new()
		instance.types = import.FetchTypes(instance.object)
	end)

	if not status then
		print(message)
	end

	return instance
end

function PyBridge:Reload(coluster, services)
end

function PyBridge:Delete(coluster, services)
end

return PyBridge

