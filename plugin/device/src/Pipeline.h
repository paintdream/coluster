// Pipeline.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-31
//

#pragma once

#include "../../../src/Coluster.h"
#include <vulkan/vulkan.h>

#ifdef DEVICE_EXPORT
	#ifdef __GNUC__
		#define DEVICE_API __attribute__ ((visibility ("default")))
	#else
		#define DEVICE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define DEVICE_API __attribute__ ((visibility ("default")))
	#else
		#define DEVICE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif

// forward declaration of VMA
typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

#define DEFINE_MAP_ENTRY(f) { #f, VK_##f }

