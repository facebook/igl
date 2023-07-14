#pragma once

#include <assert.h>
#include <cstdint>
#include <vector>

#include "lvk/LVK.h"

/// Pool<> is used only by the implementation
namespace lvk {

template<typename ObjectType, typename ImplObjectType>
class Pool {
  static constexpr uint32_t kListEndSentinel = 0xffffffff;
  struct PoolEntry {
    explicit PoolEntry(ImplObjectType& obj) : obj_(std::move(obj)) {}
    ImplObjectType obj_ = {};
    uint32_t gen_ = 1;
    uint32_t nextFree_ = kListEndSentinel;
  };
  std::vector<PoolEntry> objects_;
  uint32_t freeListHead_ = kListEndSentinel;
  uint32_t numObjects_ = 0;

 public:
  Handle<ObjectType> create(ImplObjectType&& obj) {
    uint32_t idx = 0;
    if (freeListHead_ != kListEndSentinel) {
      idx = freeListHead_;
      freeListHead_ = objects_[idx].nextFree_;
      objects_[idx].obj_ = std::move(obj);
    } else {
      idx = (uint32_t)objects_.size();
      objects_.emplace_back(obj);
    }
    numObjects_++;
    return Handle<ObjectType>(idx, objects_[idx].gen_);
  }
  void destroy(Handle<ObjectType> handle) {
    if (handle.empty())
      return;
    assert(numObjects_ > 0);
    const uint32_t index = handle.index();
    assert(index < objects_.size());
    assert(handle.gen() == objects_[index].gen_); // already deleted
    objects_[index].obj_ = ImplObjectType{};
    objects_[index].gen_++;
    objects_[index].nextFree_ = freeListHead_;
    freeListHead_ = index;
    numObjects_--;
  }
  const ImplObjectType* get(Handle<ObjectType> handle) const {
    if (handle.empty())
      return nullptr;

    const uint32_t index = handle.index();
    assert(index < objects_.size());
    assert(handle.gen() == objects_[index].gen_);
    return &objects_[index].obj_;
  }
  ImplObjectType* get(Handle<ObjectType> handle) {
    if (handle.empty())
      return nullptr;

    const uint32_t index = handle.index();
    assert(index < objects_.size());
    assert(handle.gen() == objects_[index].gen_);
    return &objects_[index].obj_;
  }
  uint32_t numObjects() const {
    return numObjects_;
  }
};

} // namespace lvk
