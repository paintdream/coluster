if package.cpath:find(".so") and not package.cpath:find("%./lib%?%.so") then
	package.cpath = package.cpath .. ";./lib?.so"
	print(package.cpath)
end

local ngx = {}
ngx.INFO = 1
ngx.log = function (level, content)
	print(content)
end

ngx.var = {
	coluster_thread_count = 3,
	coluster_tick_interval = 5
}

ngx.timer = {
	at = function (interval, func)
		local text = io.read()
		if text == "exit" then
			return nil, "ngx simulator exited."
		else
			return func(interval)
		end
	end
}

return ngx