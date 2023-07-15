local Concurrent = {}
Concurrent.__index = {}

function Concurrent.Folk(routines)
	local host = coroutine.running()
	local count = #routines
	for i = 1, #routines do
		local routine = routines[i]
		coroutine.wrap(function ()
			routine()
			
			count = count - 1
			if count == 0 then
				coroutine.resume(host)
			end
		end)()
	end

	if count ~= 0 then
		coroutine.yield()
	end
end

return Concurrent