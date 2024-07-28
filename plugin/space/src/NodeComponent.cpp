#include "NodeComponent.h"

namespace coluster {
	NodeComponent::NodeComponent() noexcept {}
	NodeComponent::NodeComponent(Entity e) noexcept { key.entity = e; }
	NodeComponent::~NodeComponent() noexcept {}

	NodeComponentSystem::NodeComponentSystem(Space& s) : space(s) {
		space.GetSystems().attach(subSystem);
	}

	NodeComponentSystem::~NodeComponentSystem() noexcept {
		space.GetSystems().detach(subSystem);
	}

	void NodeComponentSystem::lua_initialize(LuaState lua, int index) {}
	void NodeComponentSystem::lua_finalize(LuaState lua, int index) {}
	void NodeComponentSystem::lua_registar(LuaState lua) {
		lua.set_current<&NodeComponentSystem::Create>("Create");
		lua.set_current<&NodeComponentSystem::Delete>("Delete");
		lua.set_current<&NodeComponentSystem::Clear>("Clear");
		lua.set_current<&NodeComponentSystem::Move>("Move");
		lua.set_current<&NodeComponentSystem::Query>("Query");
		lua.set_current<&NodeComponentSystem::Attach>("Attach");
		lua.set_current<&NodeComponentSystem::Detach>("Detach");
		lua.set_current<&NodeComponentSystem::Optimize>("Optimize");
	}

	bool NodeComponentSystem::Create(Entity entity) {
		return subSystem.insert(entity, NodeComponent(entity));
	}

	Result<void> NodeComponentSystem::Delete(Entity entity) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		subSystem.remove(entity);
		return {};
	}

	void NodeComponentSystem::Clear() {
		subSystem.clear();
	}

	Result<void> NodeComponentSystem::Move(Entity entity, const std::array<float, 6>& boundingNodeBox) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		bool success = false;
		subSystem.for_entity<NodeComponent>(entity, [&boundingNodeBox, &success](NodeComponent& node) noexcept {
			if (node.get_parent() == nullptr) {
				node.set_key(NodeBox(Vector4(boundingNodeBox[0], boundingNodeBox[1], boundingNodeBox[2], -1.0f), Vector4(boundingNodeBox[3], boundingNodeBox[4], boundingNodeBox[5], 1.0f)));
				success = true;
			}
		});

		if (!success) {
			return ResultError("Can only move isolated node!");
		} else {
			return {};
		}
	}

	Result<void> NodeComponentSystem::Attach(Entity parent, Entity child) {
		if (!subSystem.valid(parent)) {
			return ResultError("Invalid parent entity!");
		}

		if (!subSystem.valid(child)) {
			return ResultError("Invalid child entity!");
		}

		bool success = false;
		subSystem.for_entity<NodeComponent>(parent, [this, child, &success](NodeComponent& parentNodeComponent) noexcept {
			subSystem.for_entity<NodeComponent>(child, [&parentNodeComponent, &success](NodeComponent& subNodeComponent) noexcept {
				if (subNodeComponent.get_parent() == nullptr && subNodeComponent.get_left() != nullptr && subNodeComponent.get_right() != nullptr) {
					subNodeComponent.attach(&parentNodeComponent);
					success = true;
				}
			});
		});

		if (!success) {
			return ResultError("Child node is not an isolated node!");
		} else {
			return {};
		}
	}

	Result<void> NodeComponentSystem::Detach(Entity entity) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		subSystem.for_entity<NodeComponent>(entity, [](NodeComponent& node) noexcept {
			node.detach([](auto* left_node, auto* right_node) { return true; });
		});

		return {};
	}

	Result<std::vector<Entity>> NodeComponentSystem::Query(Entity entity, const std::array<float, 6>& boundingNodeBox, const std::vector<float>& convexCuller) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		std::vector<Entity> result;
		NodeBox box(Vector4(boundingNodeBox[0], boundingNodeBox[1], boundingNodeBox[2], -1.0f), Vector4(boundingNodeBox[3], boundingNodeBox[4], boundingNodeBox[5], 1.0f));
		subSystem.for_entity<NodeComponent>(entity, [&result, &box, &convexCuller](auto& nodeBase) {
			nodeBase.template query<true>(box, [&result](auto& subNodeComponentBase) {
				result.emplace_back(static_cast<NodeComponent&>(subNodeComponentBase).GetEntity());
				return true;
			}, [&convexCuller](auto& bound) {
				if (convexCuller.size() < 4)
					return true;

				static constexpr Vector4 half(0.5f, 0.5f, 0.5f, 0.5f);
				Vector4 begin = bound.first;
				Vector4 end = bound.second;
				Vector4 size = end - begin;
				Vector4 center = begin + size * half;

				for (size_t j = 0; j < convexCuller.size() - 3; j += 4) {
					Vector4 plane(convexCuller[j], convexCuller[j + 1], convexCuller[j + 2], convexCuller[j + 3]);
					if (glm::dot(half, glm::abs(size * plane)) + glm::dot(plane, center) < 0.0f)
						return false;
				}

				return true;
			});
		});

		return result;
	}

	Result<Entity> NodeComponentSystem::Optimize(Entity entity) {
		if (!subSystem.valid(entity)) {
			return ResultError("Invalid entity!");
		}

		Entity result = entity;
		subSystem.for_entity<NodeComponent>(entity, [&result](auto& nodeBase) {
			if (nodeBase.get_parent() == nullptr) {
				NodeComponent* node = static_cast<NodeComponent*>(nodeBase.optimize());
				result = node->GetEntity();
			}
		});

		return result;
	}
}
