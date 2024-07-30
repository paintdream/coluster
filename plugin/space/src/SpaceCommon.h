// SpaceCommon.h
// PaintDream (paintdream@paintdream.com)
// 2023-07-02
//

#pragma once

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef SPACE_EXPORT
	#ifdef __GNUC__
		#define SPACE_API __attribute__ ((visibility ("default")))
	#else
		#define SPACE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define SPACE_API __attribute__ ((visibility ("default")))
	#else
		#define SPACE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define SPACE_API
#endif
