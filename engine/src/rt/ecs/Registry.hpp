#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <vector>
#include <algorithm>
#include <utility>
#include "rt/ecs/Types.hpp"
#include "rt/ecs/Storage.hpp"
#include "rt/ecs/System.hpp"

namespace rt::ecs {

class Registry;

class EntityHandle {
  public:
    EntityHandle(Registry& r, Entity e) : r_(r), e_(e) {}
    operator Entity() const { return e_; }

    template <typename C, typename... Args>
    EntityHandle& add(Args&&... args);

    template <typename C>
    C* get();

  private:
    Registry& r_;
    Entity e_;
};

class Registry {
  public:
    EntityHandle create() {
        Entity e = ++last_;
        alive_.push_back(e);
        return EntityHandle(*this, e);
    }

    EntityHandle createHandle() { return create(); }

    EntityHandle handle(Entity e) { return EntityHandle(*this, e); }

    void destroy(Entity e) {
        for (auto& [_, store] : stores_) store->remove(e);
        alive_.erase(std::remove(alive_.begin(), alive_.end(), e), alive_.end());
    }

    template <typename C>
    ComponentStorage<C>& storage() {
        auto key = std::type_index(typeid(C));
        auto it = stores_.find(key);
        if (it == stores_.end()) {
            auto ptr = std::make_unique<ComponentStorage<C>>();
            auto raw = ptr.get();
            stores_.emplace(key, std::move(ptr));
            return *raw;
        }
        return *static_cast<ComponentStorage<C>*>(it->second.get());
    }

    template <typename C>
    C& emplace(Entity e, const C& c = C{}) { return storage<C>().emplace(e, c); }

    template <typename C, typename... Args>
    C& emplace(Entity e, Args&&... args) { return storage<C>().emplace(e, C{std::forward<Args>(args)...}); }

    template <typename C>
    C* get(Entity e) { return storage<C>().get(e); }

    template <typename C>
    auto& all() { return storage<C>().data(); }
    template <typename C>
    auto& getAll() { return storage<C>().data(); }
    template <typename C>
    auto& getall() { return storage<C>().data(); }

    void addSystem(std::unique_ptr<System> sys) { systems_.push_back(std::move(sys)); }

    void update(float dt) {
        for (auto& s : systems_) s->update(*this, dt);
    }

    const std::vector<Entity>& alive() const { return alive_; }

  private:
    Entity last_ = 0;
    std::vector<Entity> alive_;
    std::unordered_map<std::type_index, std::unique_ptr<IStorage>> stores_;
    std::vector<std::unique_ptr<System>> systems_;

    friend class EntityHandle;
};

template <typename C, typename... Args>
inline EntityHandle& EntityHandle::add(Args&&... args) {
    r_.emplace<C>(e_, std::forward<Args>(args)...);
    return *this;
}

template <typename C>
inline C* EntityHandle::get() { return r_.get<C>(e_); }

}
