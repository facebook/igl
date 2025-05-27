/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <igl/Common.h>

namespace igl {
class IDevice;
}

namespace iglu::state_pool {

///--------------------------------------
/// MARK: - IStatePool

/// Abstract class that describes a state pool of `TStateObject` objects,
/// that are created based off of a given `TDescriptor` and cached in a pool.
template<class TDescriptor, class TStateObject>
class IStatePool {
 public:
  /// Get or create a given state object, given a descriptor.
  virtual std::shared_ptr<TStateObject> getOrCreate(igl::IDevice& dev,
                                                    const TDescriptor& desc,
                                                    igl::Result* outResult) = 0;

  virtual ~IStatePool() = default;
};

///--------------------------------------
/// MARK: - LRUStatePool

template<class TDescriptor, class TStateObject>
class LRUStatePool : public IStatePool<TDescriptor, TStateObject> {
 public:
  void setCacheSize(uint32_t maxCacheSize) {
    maxCacheSize_ = maxCacheSize;
    while (stateList_.size() >= maxCacheSize_) {
      deleteLastUsed();
    }
  }

  // Implememnts LRU Cache https://www.geeksforgeeks.org/lru-cache-implementation/
  // Gets or creates a state object and moves the strong reference to it to the beginning of the
  // pool's queue so it remains in cache longer if frequently used
  std::shared_ptr<TStateObject> getOrCreate(igl::IDevice& dev,
                                            const TDescriptor& desc,
                                            igl::Result* outResult) final {
    auto it = stateMap_.find(desc);
    if (it != stateMap_.cend()) {
      // Cache hit
      if (it->second != stateList_.begin()) {
        stateList_.splice(stateList_.begin(), stateList_, it->second);
      }
    } else {
      // Cache miss
      if (stateList_.size() >= maxCacheSize_) {
        // If the cache is full delete the last used
        deleteLastUsed();
      }

      // Add new element to start of queue
      auto stateResource = createStateObject(dev, desc, outResult);

      if (!IGL_DEBUG_VERIFY(stateResource != nullptr)) {
        return nullptr;
      }

      auto stateDesc = desc; // force r-value c-tor: pair(U1&& x, U2&& y)
      stateList_.push_front(TStateItem(std::move(stateDesc), std::move(stateResource)));
      it = stateMap_.insert(std::make_pair(desc, stateList_.begin())).first;
    }

    return it->second->second;
  }

 private:
  using TStateItem = std::pair<TDescriptor, std::shared_ptr<TStateObject>>;
  virtual std::shared_ptr<TStateObject> createStateObject(igl::IDevice& dev,
                                                          const TDescriptor& desc,
                                                          igl::Result* outResult) = 0;

  void deleteLastUsed() {
    auto last = stateList_.back();
    stateList_.pop_back();
    stateMap_.erase(last.first);
  }

  // Queue to store the values
  std::list<TStateItem> stateList_;

  // Map to check existance
  std::unordered_map<TDescriptor, typename std::list<TStateItem>::iterator> stateMap_;

  uint32_t maxCacheSize_ = 1024; // maximum capacity of cache
};

} // namespace iglu::state_pool
