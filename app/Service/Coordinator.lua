local Coordinator = {}
Coordinator.__index = Coordinator

function Coordinator.New(coluster, services)
	return Coordinator -- singleton, return ourself
end

function Coordinator:Reload(coluster, services)
end

function Coordinator:Delete(coluster, services)
end

return Coordinator
