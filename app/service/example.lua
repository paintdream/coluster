local Example = {}
Example.__index = Example

local function Main(coluster, services)
	local concurrent = require("util/concurrent")

	local device = services.device
	local storage = services.storage
	local luabridge = services.luabridge

	local remotePrint = luabridge.object:Get("print")
	luabridge.object:Call(remotePrint, "hello luabridge! ", 716)
	local remoteAdd = luabridge.object:Load("local a, b = ...\nreturn a + b, a - b")
	local resultAdd, resultSub = luabridge.object:Call(remoteAdd, 1, 2)
	print("Remote Add/Sub: " .. tostring(resultAdd) .. " | " .. tostring(resultSub))

	-- declare type creators
	local CmdBuffer = device.types.CmdBuffer
	local Buffer = device.types.Buffer
	local Shader = device.types.Shader
	local Pass = device.types.Pass
	local Image = device.types.Image
	local Texture = device.types.Texture
	local File = storage.types.File

	--[[
	for _, d in ipairs(storage.object:ListDirectory(".")) do 
		print("item " .. d)
	end
	]]

	local database = services.database.object
	if database:Initialize(":memory:", true) then
		print("Write data to db ...")
		database:Execute([[
			create table `example` (intValue INTEGER, floatValue FLOAT, textValue TEXT);
		]])
		database:Execute([[
			insert into `example` values (?, ?, ?);
		]], {
			{ 1, 1.414, "sqrt2" },
			{ 2, 1.732, "sqrt3" }
		})

		local result = database:Execute([[
			select * from `example`;
		]])

		print("Select any:")
		for _, v in ipairs(result) do
			print("result: " .. v[2])			
		end

		print("Select sqrt3:")
		local resultWhere = database:Execute([[
			select * from `example` where `textValue` = 'sqrt3';
		]])

		for _, v in ipairs(resultWhere) do
			print("result: " .. v[2])
		end

		print("Select both:")
		local resultWhereBatch = database:Execute([[
			select * from `example` where `textValue` = ?;
		]], {{ "sqrt2" }, { "sqrt3" }}, true)

		for _, v in ipairs(resultWhereBatch) do
			print("result: " .. v[2])
		end
	end

	-- compute gaussian weights
	local sigma = 16.0
	local weights = {}
	local length = 9
	for i = 1, length do
		weights[i] = math.exp(-0.5 * ((i - 1) / sigma) ^ 2)
	end

	local cmdBuffer = CmdBuffer()
	local shaderFile = File()
	if shaderFile:Open("shader/gaussian.glsl") then
		local size = shaderFile:GetSize()
		local content = shaderFile:Read(0, size)
		shaderFile:Close()

		-- print(content)
		local shaderGaussianHorizontal = Shader()
		local compileResultHorinzontal = shaderGaussianHorizontal:Initialize("#define GAUSSIAN_VERTICAL 0\n" .. content, "main")
		if #compileResultHorinzontal ~= 0 then
			print("Compiling error: \n" .. compileResultHorinzontal)
			return
		end

		local shaderGaussianVertical = Shader()
		local compileResultVertical = shaderGaussianVertical:Initialize("#define GAUSSIAN_VERTICAL 1\n" .. content, "main")
		if #compileResultVertical ~= 0 then
			print("Compiling error: \n" .. compileResultVertical)
			return
		end

		local localSize = shaderGaussianHorizontal:GetLocalSize()
		assert(shaderGaussianHorizontal:IsBufferLayoutCompatible("theParameters", shaderGaussianVertical, "theParameters"))

		local inputImageFile = File()
		local inputImagePath = "asset/test.webp"
		if inputImageFile:Open(inputImagePath) then
			local imageTexture = Texture()
			if imageTexture:Load(inputImageFile) then
				print("Loading webp: " .. inputImagePath)

				local resolution = imageTexture:GetResolution()

				-- uniform buffer, cpu visible
				local formatFloat = "f"
				local bufferContent = shaderGaussianHorizontal:FormatBuffer("theParameters", {
					{ "radius", shaderGaussianHorizontal:FormatIntegers({ length }) },
					{ "weights", shaderGaussianHorizontal:FormatFloats(weights) }
				})
		
				local theParameters = Buffer()
				theParameters:Initialize(#bufferContent, true, true)
				local theImage = Image()

				-- resource preparation

				concurrent.Folk({
					function () theParameters:Upload(cmdBuffer, 0, bufferContent) end,
					function () imageTexture:Upload(cmdBuffer, theImage) end
				})

				-- dispatch to compute device
				local dispatchCount = { (math.modf((resolution[1] + localSize[1] - 1) / localSize[1])), (math.modf((resolution[2] + localSize[2] - 1) / localSize[2])), 1 }
				local passGaussianHorizontal = Pass()
				passGaussianHorizontal:Initialize(shaderGaussianHorizontal)
				passGaussianHorizontal:BindBuffer("theParameters", theParameters, 0)
				passGaussianHorizontal:BindImage("theImage", theImage)
				passGaussianHorizontal:Dispatch(cmdBuffer, dispatchCount)

				local passGaussianVertical = Pass()
				passGaussianVertical:Initialize(shaderGaussianVertical)
				passGaussianVertical:BindBuffer("theParameters", theParameters, 0)
				passGaussianVertical:BindImage("theImage", theImage)
				passGaussianVertical:Dispatch(cmdBuffer, dispatchCount)
				cmdBuffer:Submit()

				-- fetch the result
				imageTexture:Download(cmdBuffer, theImage)

				-- write to output
				local outputImageFile = File()
				local outputImagePath = "asset/test_out.webp"
				if outputImageFile:Open(outputImagePath, true) then
					print("Saving webp: " .. outputImagePath)
					imageTexture:Save(outputImageFile)
					outputImageFile:Close()
				end

				inputImageFile:Close()
			end
		end
	end
end

function Example.New(coluster, services)
	local instance = {}
	setmetatable(instance, Example)

	coroutine.wrap(Main)(coluster, services)
	return instance
end

function Example:Reload(coluster, services)
end

function Example:Delete(coluster, services)
end

return Example

