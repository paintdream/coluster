local Storage = {}
Storage.__index = Storage

function Storage.New(coluster, services)
	local instance = {}
	setmetatable(instance, Storage)
	local import = require("util/import")

	instance.object = require("storage").new()
	instance.types = import.FetchTypes(instance.object)
	return instance
end

function Storage:Reload(coluster, services)
end

function Storage:Delete(coluster, services)
end

return Storage

