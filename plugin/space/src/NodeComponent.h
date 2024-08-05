// NodeComponent.h
// PaintDream (paintdream@paintdream.com)
// 2023-12-11
//

#pragma once
#include "Space.h"
#include "glm/vec4.hpp"
#include "glm/geometric.hpp"

namespace coluster {
	using Vector4 = glm::vec4;
	struct NodeBox {
		using first_type = Vector4;
		using second_type = Vector4;
		NodeBox() noexcept {}
		NodeBox(const Vector4& from, const Vector4& to) noexcept : first(from), second(to) {}

		Vector4 first;
		Vector4 second;
		Entity entity;
	};

	using Overlap = TreeOverlap<NodeBox, typename NodeBox::first_type, uint32_t, 6>;

	class NodeComponent : public Tree<NodeBox, Overlap> {
	public:
		using Base = Tree<NodeBox, Overlap>;

		NodeComponent(Entity e) noexcept;
		~NodeComponent() noexcept;
		NodeComponent(NodeComponent&& rhs) noexcept;
		NodeComponent& operator = (NodeComponent&& rhs) noexcept;

		Entity GetEntity() const noexcept {
			return key.entity;
		}

		Vector4 Begin() const noexcept {
			return key.first;
		}

		Vector4 End() const noexcept {
			return key.second;
		}
	};

	class NodeComponentSystem : public ComponentSystem<NodeComponent> {
	public:
		NodeComponentSystem(Space& space);
	
		static void lua_registar(LuaState lua);
		bool Create(Entity entity);
		Result<void> Move(Entity entity, const std::array<float, 6>& boundingNodeBox);
		Result<std::vector<Entity>> Query(Entity entity, const std::array<float, 6>& boundingNodeBox, const std::vector<float>& convexCuller);
		Result<void> Attach(Entity parent, Entity child);
		Result<void> Detach(Entity entity);
		Result<Entity> Optimize(Entity entity);
	};
}