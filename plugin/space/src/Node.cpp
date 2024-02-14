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
		lua.set_current<&NodeSystem::Attach>("Attach");
		lua.set_current<&NodeSystem::Detach>("Detach");
	}

	bool NodeSystem::Create(Entity entity, Ref&& ref) {
		return subSystem.insert(entity, Node(entity, std::move(ref)));
	}

	void NodeSystem::Move(Entity entity, const std::array<float, 6>& boundingBox) {
		subSystem.for_entity<Node>(entity, [&boundingBox](Node& node) noexcept {
			node.set_key(Box(Vector(boundingBox[0], boundingBox[1], boundingBox[2]), Vector(boundingBox[3], boundingBox[4], boundingBox[5])));
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

	std::vector<Entity> NodeSystem::Query(Entity entity, const std::array<float, 6>& boundingBox) {
		std::vector<Entity> result;
		Box box(Vector(boundingBox[0], boundingBox[1], boundingBox[2]), Vector(boundingBox[3], boundingBox[4], boundingBox[5]));
		subSystem.for_entity<Node>(entity, [&result, &box](auto& nodeBase) {
			nodeBase.query<true>(box, [&result](auto& subNodeBase) {
				result.emplace_back(static_cast<Node&>(subNodeBase).GetEntity());
				return true;
			});
		});

		return result;
	}
}
