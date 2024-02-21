// Node.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#pragma once
#include "Space.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/geometric.hpp"

namespace coluster {
	using Vector3 = glm::vec3;
	using Vector4 = glm::vec4;
	using Box = std::pair<Vector3, Vector3>;
	using Overlap = TreeOverlap<Box, typename Box::first_type, Vector3::length_type, 6>;

	class Node : public Tree<Box, Overlap> {
	public:
		using Base = Tree<Box, Overlap>;

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

		const Ref& GetEntityObject() const noexcept {
			return ref;
		}

		Ref& GetEntityObject() noexcept {
			return ref;
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
		std::vector<Entity> Query(Entity entity, const std::array<float, 6>& boundingBox, const std::vector<float>& convexCuller);
		Ref GetObject(LuaState lua, Entity entity);
		void SetObject(LuaState lua, Entity entity, Ref&& ref);
		bool Attach(Entity parent, Entity child);
		bool Detach(Entity entity);
		Entity Optimize(Entity entity);

	protected:
		Space& space;
		System<Entity, Node> subSystem;
	};
}