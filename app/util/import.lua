local Import = {}
Import.__index = {}

function Import.FetchTypes(instance)
	assert(instance)
	assert(getmetatable(instance))

	local types = {}

	for name, value in pairs(getmetatable(instance)) do
		if name:sub(0, 4) == "Type" then
			types[name:sub(5)] = value(instance).create
		end
	end

	return types
end

return Import