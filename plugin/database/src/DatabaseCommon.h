// DatabaseCommon.h
// PaintDream (paintdream@paintdream.com)
// 2023-04-02
//

#pragma once

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef DATABASE_EXPORT
	#ifdef __GNUC__
		#define DATABASE_API __attribute__ ((visibility ("default")))
	#else
		#define DATABASE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define DATABASE_API __attribute__ ((visibility ("default")))
	#else
		#define DATABASE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define DATABASE_API
#endif
