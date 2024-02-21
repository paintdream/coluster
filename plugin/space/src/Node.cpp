#include "Node.h"

namespace coluster {
	Node::Node() noexcept {}
	Node::Node(Entity e, Ref&& r) noexcept : entity(e), ref(std::move(r)) {}
	Node::~Node() noexcept {
		if (ref) {
			Warp* currentWarp = Warp::get_current_warp();
			assert(currentWarp != nullptr);
			lua_State* L = currentWarp->GetLuaRoot();
			assert(L != nullptr);
			LuaState(L).deref(std::move(ref));
		}
	}

	NodeSystem::NodeSystem(Space& s) : space(s) {
		space.GetSystems().attach(subSystem);
	}

	NodeSystem::~NodeSystem() noexcept {
		space.GetSystems().detach(subSystem);
	}

	void NodeSystem::lua_initialize(LuaState lua, int index) {}
	void NodeSystem::lua_finalize(LuaState lua, int index) {}
	void NodeSystem::lua_registar(LuaState lua) {
		lua.set_current<&NodeSystem::Create>("Create");
		lua.set_current<&NodeSystem::Move>("Move");
		lua.set_current<&NodeSystem::GetObject>("GetObject");
		lua.set_current<&NodeSystem::SetObject>("SetObject");
		lua.set_current<&NodeSystem::Query>("Query");
		lua.set_current<&NodeSystem::Attach>("Attach");
		lua.set_current<&NodeSystem::Detach>("Detach");
		lua.set_current<&NodeSystem::Optimize>("Optimize");
	}

	bool NodeSystem::Create(Entity entity, Ref&& ref) {
		return subSystem.insert(entity, Node(entity, std::move(ref)));
	}

	Ref NodeSystem::GetObject(LuaState lua, Entity entity) {
		Ref ref;

		subSystem.for_entity<Node>(entity, [&lua, &ref](Node& node) noexcept {
			ref = lua.make_value(node.GetEntityObject());
		});

		return ref;
	}

	void NodeSystem::SetObject(LuaState lua, Entity entity, Ref&& ref) {
		subSystem.for_entity<Node>(entity, [&lua, &ref](Node& node) noexcept {
			lua.deref(std::move(node.GetEntityObject()));
			node.GetEntityObject() = std::move(ref);
		});
	}

	void NodeSystem::Move(Entity entity, const std::array<float, 6>& boundingBox) {
		subSystem.for_entity<Node>(entity, [&boundingBox](Node& node) noexcept {
			node.set_key(Box(Vector3(boundingBox[0], boundingBox[1], boundingBox[2]), Vector3(boundingBox[3], boundingBox[4], boundingBox[5])));
		});
	}

	bool NodeSystem::Attach(Entity parent, Entity child) {
		bool success = false;
		subSystem.for_entity<Node>(parent, [this, child, &success](Node& parentNode) noexcept {
			subSystem.for_entity<Node>(child, [&parentNode, &success](Node& subNode) noexcept {
				subNode.set_parent(&parentNode);
				success = true;
			});
		});

		return success;
	}

	bool NodeSystem::Detach(Entity entity) {
		bool success = false;
		subSystem.for_entity<Node>(entity, [&success](Node& node) noexcept {
			if (node.get_parent() != nullptr) {
				success = true;
			}

			node.set_parent(nullptr);
		});

		return success;
	}

	std::vector<Entity> NodeSystem::Query(Entity entity, const std::array<float, 6>& boundingBox, const std::vector<float>& convexCuller) {
		std::vector<Entity> result;
		Box box(Vector3(boundingBox[0], boundingBox[1], boundingBox[2]), Vector3(boundingBox[3], boundingBox[4], boundingBox[5]));
		subSystem.for_entity<Node>(entity, [&result, &box, &convexCuller](auto& nodeBase) {
			nodeBase.query<true>(box, [&result](auto& subNodeBase) {
				result.emplace_back(static_cast<Node&>(subNodeBase).GetEntity());
				return true;
			}, [&convexCuller](auto& bound) {
				if (convexCuller.size() < 4)
					return true;

				static constexpr Vector4 half(0.5f, 0.5f, 0.5f, 0.5f);
				Vector4 begin = Vector4(bound.first, -1.0f);
				Vector4 end = Vector4(bound.second, 1.0f);
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

	Entity NodeSystem::Optimize(Entity entity) {
		Entity result = entity;
		subSystem.for_entity<Node>(entity, [&result](auto& nodeBase) {
			if (nodeBase.get_parent() == nullptr) {
				Node* node = static_cast<Node*>(nodeBase.optimize());
				result = node->GetEntity();
			}
		});

		return result;
	}
}
