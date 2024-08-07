#include "NodeComponent.h"

namespace coluster {
	NodeComponent::NodeComponent(Entity e) noexcept { key.entity = e; }
	NodeComponent::~NodeComponent() noexcept {}

	NodeComponent::NodeComponent(NodeComponent&& rhs) noexcept {
		*this = std::move(rhs);
	}

	NodeComponent& NodeComponent::operator = (NodeComponent&& rhs) noexcept {
		Base::operator = (std::move(rhs));

		NodeComponent* parent = static_cast<NodeComponent*>(parent_node);
		if (parent != nullptr) {
			if (parent->left_node == &rhs) {
				parent->left_node = this;
			} else {
				assert(parent->right_node == &rhs);
				parent->right_node = this;
			}
		}

		NodeComponent* left = static_cast<NodeComponent*>(left_node);
		if (left != nullptr) {
			left->parent_node = this;
		}

		NodeComponent* right = static_cast<NodeComponent*>(right_node);
		if (right != nullptr) {
			right->parent_node = this;
		}

		return *this;
	}

	NodeComponentSystem::NodeComponentSystem(Space& s) : BaseClass(s) {}

	void NodeComponentSystem::lua_registar(LuaState lua) {
		BaseClass::lua_registar(lua);

		lua.set_current<&NodeComponentSystem::Create>("Create");
		lua.set_current<&NodeComponentSystem::Move>("Move");
		lua.set_current<&NodeComponentSystem::Query>("Query");
		lua.set_current<&NodeComponentSystem::Attach>("Attach");
		lua.set_current<&NodeComponentSystem::Detach>("Detach");
		lua.set_current<&NodeComponentSystem::Optimize>("Optimize");
	}

	bool NodeComponentSystem::Create(Entity entity) {
		return subSystem.insert(entity, NodeComponent(entity));
	}

	Result<void> NodeComponentSystem::Move(Entity entity, const std::array<float, 6>& boundingNodeBox) {
		bool removed = false;
		if (subSystem.filter<NodeComponent>(entity, [&boundingNodeBox, &removed](NodeComponent& node) noexcept {
			if (node.get_parent() == nullptr) {
				node.set_key(NodeBox(Vector4(boundingNodeBox[0], boundingNodeBox[1], boundingNodeBox[2], -1.0f), Vector4(boundingNodeBox[3], boundingNodeBox[4], boundingNodeBox[5], 1.0f)));
				removed = true;
			}
		})) {
			if (removed) {
				return {};
			} else {
				return ResultError("Can only move isolated node!");
			}
		} else {
			return ResultError("Invalid entity!");
		}
	}

	Result<void> NodeComponentSystem::Attach(Entity parent, Entity child) {
		bool success = false;
		bool validChild = false;
		bool validParent = false;

		if (subSystem.filter<NodeComponent>(parent, [this, child, &success, &validChild, &validParent](NodeComponent& parentNodeComponent) noexcept {
			if (parentNodeComponent.get_parent() == nullptr) {
				validChild = subSystem.filter<NodeComponent>(child, [&parentNodeComponent, &success](NodeComponent& subNodeComponent) noexcept {
					if (subNodeComponent.get_parent() == nullptr && subNodeComponent.get_left() != nullptr && subNodeComponent.get_right() != nullptr) {
						subNodeComponent.attach(&parentNodeComponent);
						success = true;
					}
				});
			} else {
				validParent = false;
			}
		})) {
			if (validParent) {
				if (validChild) {
					if (success) {
						return {};
					} else {
						return ResultError("Child node is not an isolated node!");
					}
				} else {
					return ResultError("Invalid child entity!");
				}
			} else {
				return ResultError("Parent entity is not root!");
			}
		} else {
			return ResultError("Invalid parent entity!");
		}
	}

	Result<void> NodeComponentSystem::Detach(Entity entity) {
		if (subSystem.filter<NodeComponent>(entity, [](NodeComponent& node) noexcept {
			node.detach([](auto* left_node, auto* right_node) { return true; });
		})) {
			return {};
		} else {
			return ResultError("Invalid entity!");
		}
	}

	Result<std::vector<Entity>> NodeComponentSystem::Query(Entity entity, const std::array<float, 6>& boundingNodeBox, const std::vector<float>& convexCuller) {
		std::vector<Entity> result;
		NodeBox box(Vector4(boundingNodeBox[0], boundingNodeBox[1], boundingNodeBox[2], -1.0f), Vector4(boundingNodeBox[3], boundingNodeBox[4], boundingNodeBox[5], 1.0f));
		if (subSystem.filter<NodeComponent>(entity, [&result, &box, &convexCuller](auto& nodeBase) {
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
		})) {
			return result;
		} else {
			return ResultError("Invalid entity!");
		}
	}

	Result<Entity> NodeComponentSystem::Optimize(Entity entity) {
		Entity result = entity;
		if (subSystem.filter<NodeComponent>(entity, [&result](auto& nodeBase) {
			if (nodeBase.get_parent() == nullptr) {
				NodeComponent* node = static_cast<NodeComponent*>(nodeBase.optimize());
				result = node->GetEntity();
			}
		})) {
			return result;
		} else {
			return ResultError("Invalid entity!");
		}
	}
}
