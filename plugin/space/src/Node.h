// Node.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#include "Space.h"
#include "glm/vec3.hpp"

namespace coluster {
	using Vector = glm::vec3;
	using Box = std::pair<Vector, Vector>;
	using Overlap = TreeOverlap<Box, typename Box::first_type, 3>;

	class Node : public Object, Tree<Box, Overlap> {
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