// Node.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#include "Space.h"

namespace coluster {
	class Node : public Object {
	public:
		enum Persist {
			Persist_Script,
			Persist_Managed,
			Persist_Compressed,
			Persist_Asset,
			Persist_Remote
		};

		Node();
		~Node() noexcept override;
	};
}