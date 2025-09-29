#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <vector>
#include "rt/ecs/Types.hpp"

namespace rt::ecs {

struct IStorage {
    virtual ~IStorage() = default;
    virtual void remove(Entity e) = 0;
};

template <typename C>
class ComponentStorage : public IStorage {
  public:
    C* get(Entity e) {
        auto it = data_.find(e);
        return it == data_.end() ? nullptr : &it->second;
    }
    C& emplace(Entity e, const C& c = C{}) { return data_[e] = c; }
    void remove(Entity e) override { data_.erase(e); }
    std::unordered_map<Entity, C>& data() { return data_; }
  private:
    std::unordered_map<Entity, C> data_;
};

}
