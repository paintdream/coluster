local SandBox = {}
SandBox.__index = SandBox

function SandBox.New(coluster, services)
	local instance = {}
	setmetatable(instance, SandBox)

	return instance
end

function SandBox:Reload(coluster, services)
end

function SandBox:Delete(coluster, services)
end

function SandBox:Compile(code)
	local env = {
		-- modules
		coroutine = coroutine,
		math = math,
		string = string,
		utf8 = utf8,
		table = table,

		-- functions
		assert = assert,
		error = error,
		ipairs = ipairs,
		next = next,
		pairs = pairs,
		pcall = pcall,
		print = print,
		require = require,
		setmetatable = setmetatable,
		tonumber = tonumber,
		tostring = tostring,
		type = type,
		xpcall = xpcall
	}

	env._G = env
	instance.code = load(code, "sandbox", "t", env)
end

function SandBox:Run(...)
	local code = self.code
	if code then
		local ret, message = pcall(code, ...)
		if not ret then
			print("SandBox running error: " .. message)
		else
			return ret
		end
	end
end

return SandBox
