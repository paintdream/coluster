local Database = {}
Database.__index = Database

function Database.New(coluster, services)
	local instance = {}
	setmetatable(instance, Database)
	local import = require("Util/Import")

	instance.object = require("database").new()
	instance.types = import.FetchTypes(instance.object)
	return instance
end

function Database:Reload(coluster, services)
end

function Database:Delete(coluster, services)
end

return Database

