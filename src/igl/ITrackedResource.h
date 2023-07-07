/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/IResourceTracker.h>
#include <memory>

namespace igl {

class IResourceTracker;

/**
 * @brief ITrackedResource enables resources to be tracked by IResourceTracker
 * Resources that want to be tracked should derive from ITrackedResource and ensure
 * initResourceTracker() is called during initialization
 */
template<class T>
class ITrackedResource {
 public:
  virtual ~ITrackedResource() {
    if (resourceTracker_) {
      resourceTracker_->willDelete(static_cast<T&>(*this));
    }
  }

  /**
   * @brief initResourceTracker() sets up tracking with the tracker
   * When the resource is destroyed, it will automatically deregister itself with the tracker
   */
  void initResourceTracker(std::shared_ptr<IResourceTracker> tracker) {
    if (IGL_VERIFY(!resourceTracker_)) {
      resourceTracker_ = std::move(tracker);
      if (resourceTracker_) {
        resourceTracker_->didCreate(static_cast<T&>(*this));
      }
    }
  }

 private:
  std::shared_ptr<IResourceTracker> resourceTracker_;
};

} // namespace igl
