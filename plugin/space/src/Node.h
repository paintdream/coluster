// Node.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#pragma once
#include "Space.h"
#include "glm/vec3.hpp"

namespace coluster {
	using Vector = glm::vec3;
	using Box = std::pair<Vector, Vector>;
	using Overlap = TreeOverlap<Box, typename Box::first_type, Vector::length_type, 6>;

	class Node : public Tree<Box, Overlap> {
	public:
		using Base = Tree<Box, Overlap>;
		enum Persist {
			Persist_Script,
			Persist_Managed,
			Persist_Compressed,
			Persist_Asset,
			Persist_Remote
		};

		Node() noexcept;
		Node(Entity e, Ref&& r) noexcept;
		~Node() noexcept;
		Node(Node&& rhs) noexcept : Base(std::move(rhs)), ref(std::move(rhs.ref)) {}

		Node& operator = (Node&& rhs) noexcept {
			Base::operator = (std::move(rhs));
			ref = std::move(rhs.ref);

			return *this;
		}

		Entity GetEntity() const noexcept {
			return entity;
		}

	protected:
		Entity entity = 0;
		Ref ref;
	};

	class NodeSystem : public Object {
	public:
		NodeSystem(Space& space);
		~NodeSystem() noexcept;
	
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

		bool Create(Entity entity, Ref&& ref);
		void Move(Entity entity, const std::array<float, 6>& boundingBox);
		std::vector<Entity> Query(Entity entity, const std::array<float, 6>& boundingBox);
		bool Attach(Entity parent, Entity child);
		bool Detach(Entity entity);

	protected:
		Space& space;
		System<Entity, Node> subSystem;
	};
}