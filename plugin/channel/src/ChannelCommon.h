// ChannelCommon.h
// PaintDream (paintdream@paintdream.com)
// 2023-02-19
//

#include "../../../src/Coluster.h"

#if !COLUSTER_MONOLITHIC
#ifdef CHANNEL_EXPORT
	#ifdef __GNUC__
		#define CHANNEL_API __attribute__ ((visibility ("default")))
	#else
		#define CHANNEL_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define CHANNEL_API __attribute__ ((visibility ("default")))
	#else
		#define CHANNEL_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif
#else
#define CHANNEL_API
#endif