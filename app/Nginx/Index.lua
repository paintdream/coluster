-- To configure nginx.conf
-- In server {} declaration:
--[[
	location ~ /path/to/service {
		default_type text/html;
	    content_by_lua_file /path/to/Index.lua;
	}
]]

local status, message = pcall(function ()
	local initWorker = require("InitWorker")
	ngx.say('hello, lua nginx!' .. tostring(initWorker.GetColuster()))
end)

if not status then
	ngx.say(message)
end
