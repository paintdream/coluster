// PyBridgeCommon.h
// PaintDream (paintdream@paintdream.com)
// 2023-05-08
//

#pragma once

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef PYBRIDGE_EXPORT
	#ifdef __GNUC__
		#define PYBRIDGE_API __attribute__ ((visibility ("default")))
	#else
		#define PYBRIDGE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define PYBRIDGE_API __attribute__ ((visibility ("default")))
	#else
		#define PYBRIDGE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define PYBRIDGE_API
#endif
