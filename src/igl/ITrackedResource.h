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
      resourceTracker_->willDelete(castThisAsTPtr());
    }
  }

  /**
   * @brief initResourceTracker() sets up tracking with the tracker
   * When the resource is destroyed, it will automatically deregister itself with the tracker
   */
  void initResourceTracker(std::shared_ptr<IResourceTracker> tracker, const std::string & name = "") {
    if (IGL_VERIFY(!resourceTracker_)) {
      resourceTracker_ = std::move(tracker);
      resourceName_ = name;
      if (resourceTracker_) {
        resourceTracker_->didCreate(static_cast<T*>(this));
      }
    }
  }
    
  virtual const std::string & getResourceName() const { return resourceName_;}

 private:
  // ITrackedResource destructor is called after the `T` object is partially destructed
  // and ubsan complaints about vptr mismatch on this line :
  //  return static_cast<T*>(this);
  // So we prevent UBSan by turning off vptr sanitizer on this function
  //
  // So we prevent UBSan from analyzing pointers that are never dereferenced anyway, by //@fb-only
  // We are not seeing actual undefined behavior  //@fb-only
  // @fb-only
  // @fb-only
#if defined(__clang__)
  __attribute__((no_sanitize("vptr")))
#endif
  inline T*
  castThisAsTPtr() {
    return static_cast<T*>(this);
  }

  std::shared_ptr<IResourceTracker> resourceTracker_;
  std::string resourceName_;
};

} // namespace igl
