local Device = {}
Device.__index = Device

function Device.New(coluster, services)
	local instance = {}
	setmetatable(instance, Device)
	local import = require("util/import")

	instance.object = require("device").create()
	instance.object:Initialize(services.storage.object)
	instance.types = import.FetchTypes(instance.object)

	return instance
end

function Device:Reload(coluster, services)
end

function Device:Delete(coluster, services)
end

return Device

