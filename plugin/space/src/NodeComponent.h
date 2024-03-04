// NodeComponent.h
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

	// hack for Vector3 -> Vector4 memory mapping
	struct Box {
		using first_type = Vector3;
		using second_type = Vector3;
		Box() noexcept {}
		Box(const Vector3& from, const Vector3& to) noexcept : first(from), second(to) {}

		union {
			struct {
				Vector3 first;
				Entity entity;
			};
			Vector4 data[1];
		};

		Vector3 second;
	};

	using Overlap = TreeOverlap<Box, typename Box::first_type, Vector3::length_type, 6>;

	class NodeComponent : public Tree<Box, Overlap> {
	public:
		using Base = Tree<Box, Overlap>;

		NodeComponent() noexcept;
		NodeComponent(Entity e, Ref&& r) noexcept;
		~NodeComponent() noexcept;
		NodeComponent(NodeComponent&& rhs) noexcept : Base(std::move(rhs)), ref(std::move(rhs.ref)) {}

		NodeComponent& operator = (NodeComponent&& rhs) noexcept {
			Base::operator = (std::move(rhs));
			ref = std::move(rhs.ref);

			return *this;
		}

		Entity GetEntity() const noexcept {
			return key.entity;
		}

		const Ref& GetEntityObject() const noexcept {
			return ref;
		}

		Ref& GetEntityObject() noexcept {
			return ref;
		}

		Vector4 Begin() const noexcept {
			const Vector4* data = key.data;
			return data[0];
		}

		Vector4 End() const noexcept {
			const Vector4* data = key.data;
			return data[1];
		}

	protected:
		Ref ref;
		uint32_t flags = 0;
	};

	class NodeComponentSystem : public Object {
	public:
		NodeComponentSystem(Space& space);
		~NodeComponentSystem() noexcept;
	
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		static void lua_registar(LuaState lua);

		bool Create(Entity entity, Ref&& ref);
		bool Move(Entity entity, const std::array<float, 6>& boundingBox);
		std::vector<Entity> Query(Entity entity, const std::array<float, 6>& boundingBox, const std::vector<float>& convexCuller);
		Ref GetObject(LuaState lua, Entity entity);
		void SetObject(LuaState lua, Entity entity, Ref&& ref);
		bool Attach(Entity parent, Entity child);
		bool Detach(Entity entity);
		Entity Optimize(Entity entity);

	protected:
		Space& space;
		System<Entity, NodeComponent> subSystem;
	};
}