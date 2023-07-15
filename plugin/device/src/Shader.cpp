#include "Shader.h"
#include "Device.h"
#include "../ref/vulkansdk/Public/ShaderLang.h"
#include "../ref/vulkansdk/SPIRV/GlslangToSpv.h"

namespace coluster {
	Shader::Shader(Device& dev) noexcept : DeviceObject(dev) {}
	Shader::~Shader() noexcept {
		Uninitialize();
	}

	class CustomIncluder : public glslang::TShader::Includer {
		void releaseInclude(IncludeResult*) override {
			// delete this;
		}
	};

	static uint32_t GetBasicTypeSize(glslang::TBasicType type) {
		using namespace glslang;
		switch (type) {
			case EbtVoid:
				return 0;
			case EbtFloat:
				return 4;
			case EbtDouble:
				return 8;
			case EbtFloat16:
				return 2;
			case EbtInt8:
				return 1;
			case EbtUint8:
				return 1;
			case EbtInt16:
				return 2;
			case EbtUint16:
				return 2;
			case EbtInt:
				return 4;
			case EbtUint:
				return 4;
			case EbtInt64:
				return 8;
			case EbtUint64:
				return 8;
			case EbtBool:
				return 1;
			case EbtAtomicUint:
				return 4;
			default:
				return 0;
		}
	}

	static const TBuiltInResource DefaultTBuiltInResource = {
		/* .MaxLights = */ 32,
		/* .MaxClipPlanes = */ 6,
		/* .MaxTextureUnits = */ 32,
		/* .MaxTextureCoords = */ 32,
		/* .MaxVertexAttribs = */ 64,
		/* .MaxVertexUniformComponents = */ 4096,
		/* .MaxVaryingFloats = */ 64,
		/* .MaxVertexTextureImageUnits = */ 32,
		/* .MaxCombinedTextureImageUnits = */ 80,
		/* .MaxTextureImageUnits = */ 32,
		/* .MaxFragmentUniformComponents = */ 4096,
		/* .MaxDrawBuffers = */ 32,
		/* .MaxVertexUniformVectors = */ 128,
		/* .MaxVaryingVectors = */ 8,
		/* .MaxFragmentUniformVectors = */ 16,
		/* .MaxVertexOutputVectors = */ 16,
		/* .MaxFragmentInputVectors = */ 15,
		/* .MinProgramTexelOffset = */ -8,
		/* .MaxProgramTexelOffset = */ 7,
		/* .MaxClipDistances = */ 8,
		/* .MaxComputeWorkGroupCountX = */ 65535,
		/* .MaxComputeWorkGroupCountY = */ 65535,
		/* .MaxComputeWorkGroupCountZ = */ 65535,
		/* .MaxComputeWorkGroupSizeX = */ 1024,
		/* .MaxComputeWorkGroupSizeY = */ 1024,
		/* .MaxComputeWorkGroupSizeZ = */ 64,
		/* .MaxComputeUniformComponents = */ 1024,
		/* .MaxComputeTextureImageUnits = */ 16,
		/* .MaxComputeImageUniforms = */ 8,
		/* .MaxComputeAtomicCounters = */ 8,
		/* .MaxComputeAtomicCounterBuffers = */ 1,
		/* .MaxVaryingComponents = */ 60,
		/* .MaxVertexOutputComponents = */ 64,
		/* .MaxGeometryInputComponents = */ 64,
		/* .MaxGeometryOutputComponents = */ 128,
		/* .MaxFragmentInputComponents = */ 128,
		/* .MaxImageUnits = */ 8,
		/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
		/* .MaxCombinedShaderOutputResources = */ 8,
		/* .MaxImageSamples = */ 0,
		/* .MaxVertexImageUniforms = */ 0,
		/* .MaxTessControlImageUniforms = */ 0,
		/* .MaxTessEvaluationImageUniforms = */ 0,
		/* .MaxGeometryImageUniforms = */ 0,
		/* .MaxFragmentImageUniforms = */ 8,
		/* .MaxCombinedImageUniforms = */ 8,
		/* .MaxGeometryTextureImageUnits = */ 16,
		/* .MaxGeometryOutputVertices = */ 256,
		/* .MaxGeometryTotalOutputComponents = */ 1024,
		/* .MaxGeometryUniformComponents = */ 1024,
		/* .MaxGeometryVaryingComponents = */ 64,
		/* .MaxTessControlInputComponents = */ 128,
		/* .MaxTessControlOutputComponents = */ 128,
		/* .MaxTessControlTextureImageUnits = */ 16,
		/* .MaxTessControlUniformComponents = */ 1024,
		/* .MaxTessControlTotalOutputComponents = */ 4096,
		/* .MaxTessEvaluationInputComponents = */ 128,
		/* .MaxTessEvaluationOutputComponents = */ 128,
		/* .MaxTessEvaluationTextureImageUnits = */ 16,
		/* .MaxTessEvaluationUniformComponents = */ 1024,
		/* .MaxTessPatchComponents = */ 120,
		/* .MaxPatchVertices = */ 32,
		/* .MaxTessGenLevel = */ 64,
		/* .MaxViewports = */ 16,
		/* .MaxVertexAtomicCounters = */ 0,
		/* .MaxTessControlAtomicCounters = */ 0,
		/* .MaxTessEvaluationAtomicCounters = */ 0,
		/* .MaxGeometryAtomicCounters = */ 0,
		/* .MaxFragmentAtomicCounters = */ 8,
		/* .MaxCombinedAtomicCounters = */ 8,
		/* .MaxAtomicCounterBindings = */ 1,
		/* .MaxVertexAtomicCounterBuffers = */ 0,
		/* .MaxTessControlAtomicCounterBuffers = */ 0,
		/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
		/* .MaxGeometryAtomicCounterBuffers = */ 0,
		/* .MaxFragmentAtomicCounterBuffers = */ 1,
		/* .MaxCombinedAtomicCounterBuffers = */ 1,
		/* .MaxAtomicCounterBufferSize = */ 16384,
		/* .MaxTransformFeedbackBuffers = */ 4,
		/* .MaxTransformFeedbackInterleavedComponents = */ 64,
		/* .MaxCullDistances = */ 8,
		/* .MaxCombinedClipAndCullDistances = */ 8,
		/* .MaxSamples = */ 4,
		/* .maxMeshOutputVerticesNV = */ 256,
		/* .maxMeshOutputPrimitivesNV = */ 512,
		/* .maxMeshWorkGroupSizeX_NV = */ 32,
		/* .maxMeshWorkGroupSizeY_NV = */ 1,
		/* .maxMeshWorkGroupSizeZ_NV = */ 1,
		/* .maxTaskWorkGroupSizeX_NV = */ 32,
		/* .maxTaskWorkGroupSizeY_NV = */ 1,
		/* .maxTaskWorkGroupSizeZ_NV = */ 1,
		/* .maxMeshViewCountNV = */ 4,
		/* .maxDualSourceDrawBuffersEXT = */ 1,

		/* .limits = */ {
			/* .nonInductiveForLoops = */ 1,
			/* .whileLoops = */ 1,
			/* .doWhileLoops = */ 1,
			/* .generalUniformIndexing = */ 1,
			/* .generalAttributeMatrixVectorIndexing = */ 1,
			/* .generalVaryingIndexing = */ 1,
			/* .generalSamplerIndexing = */ 1,
			/* .generalVariableIndexing = */ 1,
			/* .generalConstantMatrixVectorIndexing = */ 1,
		}
	};

	std::string Shader::Initialize(std::string_view source, std::string_view entry) {
		if (pipeline != VK_NULL_HANDLE || pipelineLayout != VK_NULL_HANDLE)
			return "";

		const int version = 450;
		EShLanguage glslStage = EShLangCompute;
		glslang::TShader shader(glslStage);
		const char* versionDef = "#version 450\n";

		const char* s[] = { versionDef, source.data() };
		shader.setEntryPoint(entry.empty() ? "main" : entry.data());
		shader.setStrings(s, sizeof(s) / sizeof(s[0]));
		shader.setEnvInput(glslang::EShSourceGlsl, glslStage, glslang::EShClientVulkan, version);
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
		shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

		std::string str;
		CustomIncluder includer;
		if (shader.preprocess(&DefaultTBuiltInResource, version, ECoreProfile, false, true, EShMessages::EShMsgDefault, &str, includer)) {
			if (shader.parse(&DefaultTBuiltInResource, version, true, EShMessages::EShMsgDefault)) {
				glslang::TProgram program;
				program.addShader(&shader);
				if (program.link(EShMessages::EShMsgDefault) && program.buildReflection(EShReflectionSeparateBuffers)) {
					// program.dumpReflection();
					glslang::TIntermediate* intermediate = program.getIntermediate(glslStage);
					std::vector<unsigned int> spirv;
					spv::SpvBuildLogger logger;
					glslang::SpvOptions options;
					options.validate = true;
					glslang::GlslangToSpv(*intermediate, spirv, &logger, &options);

					if (!spirv.empty()) {
						VkShaderModuleCreateInfo createInfo = {};
						createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
						createInfo.codeSize = spirv.size() * sizeof(spirv[0]);
						createInfo.pCode = spirv.data();

						device.Verify("create shader module", vkCreateShaderModule(device.GetDevice(), &createInfo, device.GetAllocator(), &shaderModule));

						localSize = { program.getLocalSize(0), program.getLocalSize(1), program.getLocalSize(2) };
						int uniformVariablesCount = program.getNumUniformVariables();
						int uniformBlocksCount = program.getNumUniformBlocks();
						int bufferVariablesCount = program.getNumBufferVariables();
						int bufferBlocksCount = program.getNumBufferBlocks();

						// create descriptor set
						std::vector<VkDescriptorSetLayoutBinding> bindings;
						bindings.reserve(uniformBlocksCount + bufferBlocksCount);

						size_t allNameLength = 0;
						for (int k = 0; k < uniformBlocksCount; k++) {
							allNameLength += program.getUniformBlock(k).name.size();
						}
						
						for (int k = 0; k < bufferBlocksCount; k++) {
							allNameLength += program.getBufferBlock(k).name.size();
						}

						for (int k = 0; k < uniformVariablesCount; k++) {
							allNameLength += program.getUniform(k).name.size();
						}
						
						for (int k = 0; k < bufferVariablesCount; k++) {
							allNameLength += program.getBufferVariable(k).name.size();
						}

						allNameStrings.resize(allNameLength + 1);
						size_t allNameIndex = 0;
						auto allocateName = [this, &allNameIndex](const std::string& input) {
							auto* data = allNameStrings.data() + allNameIndex;
							allNameIndex += input.size();
							assert(allNameIndex < allNameStrings.size());
							memcpy(data, input.data(), input.size());
							return std::string_view(data, input.size());
						};

						std::map<std::string, std::pair<std::string_view, uint32_t>> mapFieldNameToBlockName;
						for (int k = 0; k < uniformBlocksCount; k++) {
							auto& uniform = program.getUniformBlock(k);
							VkDescriptorSetLayoutBinding binding;
							binding.descriptorCount = 1;
							binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
							binding.binding = uniform.getBinding();
							if (uniform.getType()->isStruct()) {
								binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
								bindings.emplace_back(std::move(binding));

								BufferLayout bufferLayout;
								bufferLayout.size = iris::iris_verify_cast<uint32_t>(uniform.size);
								bufferLayout.isUniformBuffer = true;
								bufferLayout.bindingPoint = binding.binding;
								std::string_view blockName = allocateName(uniform.name);
								iris::iris_binary_insert(bufferLayoutMaps, iris::iris_make_key_value(blockName, std::move(bufferLayout)));

								for (auto&& entry : *uniform.getType()->getStruct()) {
									mapFieldNameToBlockName[std::string(blockName) + "." + entry.type->getFieldName().c_str()] = std::make_pair(blockName, entry.type->getVectorSize());
								}
							} else {
								// do not support other types
							}
						}
						
						for (int k = 0; k < bufferBlocksCount; k++) {
							auto& buffer = program.getBufferBlock(k);
							VkDescriptorSetLayoutBinding binding;
							binding.descriptorCount = 1;
							binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
							binding.binding = buffer.getBinding();

							if (buffer.getType()->isStruct()) {
								binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
								bindings.emplace_back(std::move(binding));

								BufferLayout bufferLayout;
								bufferLayout.size = iris::iris_verify_cast<uint32_t>(buffer.size);
								bufferLayout.isUniformBuffer = false;
								bufferLayout.bindingPoint = binding.binding;

								std::string_view blockName = allocateName(buffer.name);
								iris::iris_binary_insert(bufferLayoutMaps, iris::iris_make_key_value(blockName, std::move(bufferLayout)));

								for (auto&& entry : *buffer.getType()->getStruct()) {
									mapFieldNameToBlockName[std::string(blockName) + "." + entry.type->getFieldName().c_str()] = std::make_pair(blockName, entry.type->getVectorSize());
								}
							} else {
								// do not support other types
							}
						}

						for (int k = 0; k < uniformVariablesCount; k++) {
							auto& variable = program.getUniform(k);

							VkDescriptorSetLayoutBinding binding;
							binding.descriptorCount = 1;
							binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
							binding.binding = variable.getBinding();

							if (variable.getType()->isImage()) {
								binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // binding storage image
								bindings.emplace_back(std::move(binding));
								iris::iris_binary_insert(imageMaps, iris::iris_make_key_value(allocateName(variable.name), binding.binding));
							} else {
								auto p = mapFieldNameToBlockName.find(variable.name);
								if (p != mapFieldNameToBlockName.end()) {
									auto iterator = iris::iris_binary_find(bufferLayoutMaps.begin(), bufferLayoutMaps.end(), p->second.first);
									if (iterator != bufferLayoutMaps.end()) {
										Variable var;
										var.offset = variable.offset;
										var.size = GetBasicTypeSize(variable.getType()->getBasicType()) * p->second.second * variable.size;

										iris::iris_binary_insert(iterator->second.variableMap, iris::iris_make_key_value(allocateName(variable.name.substr(p->second.first.size() + 1)), std::move(var)));
									}
								}
							}
						}

						for (int k = 0; k < bufferVariablesCount; k++) {
							auto& variable = program.getBufferVariable(k);
							VkDescriptorSetLayoutBinding binding;
							binding.descriptorCount = 1;
							binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
							binding.binding = variable.getBinding();

							if (!variable.getType()->isImage()) {
								auto p = mapFieldNameToBlockName.find(variable.name);
								if (p != mapFieldNameToBlockName.end()) {
									auto iterator = iris::iris_binary_find(bufferLayoutMaps.begin(), bufferLayoutMaps.end(), p->second.first);
									if (iterator != bufferLayoutMaps.end()) {
										Variable var;
										var.offset = variable.offset;
										var.size = GetBasicTypeSize(variable.getType()->getBasicType()) * p->second.second * variable.size;

										iris::iris_binary_insert(iterator->second.variableMap, iris::iris_make_key_value(allocateName(variable.name.substr(p->second.first.size() + 1)), std::move(var)));
									}
								}
							}
						}

						VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
						descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						descriptorSetLayoutInfo.bindingCount = (uint32_t)bindings.size();
						descriptorSetLayoutInfo.pBindings = &bindings[0];

						device.Verify("create descriptor set layout", vkCreateDescriptorSetLayout(device.GetDevice(), &descriptorSetLayoutInfo, device.GetAllocator(), &descriptorSetLayout));

						VkPipelineLayoutCreateInfo layoutInfo = {};
						layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
						layoutInfo.setLayoutCount = 1;
						layoutInfo.pSetLayouts = &descriptorSetLayout;
						layoutInfo.pushConstantRangeCount = 0;
						layoutInfo.pPushConstantRanges = nullptr;

						device.Verify("create pipeline layout", vkCreatePipelineLayout(device.GetDevice(), &layoutInfo, device.GetAllocator(), &pipelineLayout));

						VkPipelineShaderStageCreateInfo shaderStageInfo = {};
						shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
						shaderStageInfo.module = shaderModule;
						shaderStageInfo.pName = "main";
						shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;

						VkComputePipelineCreateInfo pipelineCreateInfo = {};
						pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
						pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
						pipelineCreateInfo.basePipelineIndex = 0;
						pipelineCreateInfo.flags = 0;
						pipelineCreateInfo.stage = shaderStageInfo;
						pipelineCreateInfo.layout = pipelineLayout;

						device.Verify("create compute pipeline", vkCreateComputePipelines(device.GetDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, device.GetAllocator(), &pipeline));
						return "";
					}
				}
			}
		}

		return shader.getInfoLog();
	}

	uint32_t Shader::GetImageBindingPoint(std::string_view imageName) const noexcept {
		auto iterator = iris::iris_binary_find(imageMaps.begin(), imageMaps.end(), imageName);
		if (iterator != imageMaps.end()) {
			return iterator->second;
		} else {
			return ~(uint32_t)0;
		}
	}

	uint32_t Shader::GetBufferBindingPoint(std::string_view bufferName) const noexcept {
		auto iterator = iris::iris_binary_find(bufferLayoutMaps.begin(), bufferLayoutMaps.end(), bufferName);
		if (iterator != bufferLayoutMaps.end()) {
			return iterator->second.bindingPoint;
		} else {
			return ~(uint32_t)0;
		}
	}

	uint32_t Shader::GetBufferSize(std::string_view bufferName) const noexcept {
		auto iterator = iris::iris_binary_find(bufferLayoutMaps.begin(), bufferLayoutMaps.end(), bufferName);
		if (iterator != bufferLayoutMaps.end()) {
			return iterator->second.size;
		} else {
			return 0;
		}
	}

	bool Shader::IsUniformBuffer(std::string_view bufferName) const noexcept {
		auto iterator = iris::iris_binary_find(bufferLayoutMaps.begin(), bufferLayoutMaps.end(), bufferName);
		if (iterator != bufferLayoutMaps.end()) {
			return iterator->second.isUniformBuffer;
		} else {
			return false;
		}
	}

	template <typename type_t>
	static std::string Format(std::vector<type_t>&& buffer) {
		std::string result;
		if (!buffer.empty()) {
			result.resize(buffer.size() * sizeof(buffer[0]));
			memcpy(result.data(), buffer.data(), buffer.size() * sizeof(buffer[0]));
		}

		return result;
	}

	std::string Shader::FormatIntegers(std::vector<int32_t>&& buffer) {
		return Format(std::move(buffer));
	}

	std::string Shader::FormatFloats(std::vector<float>&& buffer) {
		return Format(std::move(buffer));
	}

	std::string Shader::FormatBuffer(std::string_view bufferName, std::vector<std::pair<std::string_view, std::string_view>>&& variables) const noexcept {
		auto iterator = iris::iris_binary_find(bufferLayoutMaps.begin(), bufferLayoutMaps.end(), bufferName);
		if (iterator != bufferLayoutMaps.end()) {
			auto& bufferLayout = iterator->second;
			auto& variableMap = bufferLayout.variableMap;
			std::string result;
			result.resize(bufferLayout.size);

			for (auto&& variable : variables) {
				auto target = iris::iris_binary_find(variableMap.begin(), variableMap.end(), variable.first);
				if (target != variableMap.end()) {
					assert(target->second.offset + target->second.size <= result.size());
					memcpy(result.data() + target->second.offset, variable.second.data(), std::min(variable.second.size(), iris::iris_verify_cast<size_t>(target->second.size)));
				}
			}

			return result;
		} else {
			return "";
		}
	}

	bool Shader::IsBufferLayoutCompatible(std::string_view bufferName, Required<Shader*> rhsShader, std::string_view rhsBufferName) const noexcept {
		auto lhs = iris::iris_binary_find(bufferLayoutMaps.begin(), bufferLayoutMaps.end(), bufferName);
		if (lhs != bufferLayoutMaps.end()) {
			auto rhs = iris::iris_binary_find(rhsShader.get()->bufferLayoutMaps.begin(), rhsShader.get()->bufferLayoutMaps.end(), rhsBufferName);
			if (rhs != rhsShader.get()->bufferLayoutMaps.end()) {
				return lhs->second.variableMap == rhs->second.variableMap && lhs->second.isUniformBuffer == rhs->second.isUniformBuffer;
			}
		}

		return false;
	}

	void Shader::Uninitialize() {
		if (pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device.GetDevice(), pipeline, device.GetAllocator());
			pipeline = VK_NULL_HANDLE;
		}

		if (pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(device.GetDevice(), pipelineLayout, device.GetAllocator());
			pipelineLayout = VK_NULL_HANDLE;
		}

		if (descriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(device.GetDevice(), descriptorSetLayout, device.GetAllocator());
			descriptorSetLayout = VK_NULL_HANDLE;
		}

		if (shaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device.GetDevice(), shaderModule, device.GetAllocator());
			shaderModule = VK_NULL_HANDLE;
		}
	}

	void Shader::lua_registar(LuaState lua) {
		lua.define<&Shader::Initialize>("Initialize");
		lua.define<&Shader::Uninitialize>("Uninitialize");
		lua.define<&Shader::GetBufferSize>("GetBufferSize");
		lua.define<&Shader::FormatIntegers>("FormatIntegers");
		lua.define<&Shader::FormatFloats>("FormatFloats");
		lua.define<&Shader::FormatBuffer>("FormatBuffer");
		lua.define<&Shader::IsBufferLayoutCompatible>("IsBufferLayoutCompatible");
		lua.define<&Shader::GetLocalSize>("GetLocalSize");
	}
}
