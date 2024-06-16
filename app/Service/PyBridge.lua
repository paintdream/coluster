local PyBridge = {}
PyBridge.__index = PyBridge

function PyBridge.New(coluster, services)
	local instance = {}
	setmetatable(instance, PyBridge)
	local import = require("Util/Import")

	instance.object = require("pybridge").new()
	instance.types = import.FetchTypes(instance.object)
	return instance
end

function PyBridge:Reload(coluster, services)
end

function PyBridge:Delete(coluster, services)
end

return PyBridge

