// DeviceCommon.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-28
//

#pragma once

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
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
#else
#define DEVICE_API
#endif

