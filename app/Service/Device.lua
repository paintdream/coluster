local Device = {}
Device.__index = Device

function Device.New(coluster, services)
	local instance = {}
	setmetatable(instance, Device)
	local import = require("Util/Import")

	instance.object = require("device").new()
	instance.object:Initialize(services.Storage.object)
	instance.types = import.FetchTypes(instance.object)

	return instance
end

function Device:Reload(coluster, services)
end

function Device:Delete(coluster, services)
end

return Device

