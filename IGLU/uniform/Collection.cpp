/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/uniform/Collection.h>
#include <IGLU/uniform/Descriptor.h>

#include <algorithm>

namespace iglu::uniform {

void Collection::update(const Collection& changes) {
  for (const auto& [key, value] : changes.descriptors_) {
    // Update should only modify values already in receiver; catch caller error otherwise
    IGL_DEBUG_ASSERT(descriptors_.find(key) != descriptors_.cend());
    IGL_DEBUG_ASSERT(descriptors_.find(key)->second->getType() == value->getType());
    auto indices = descriptors_[key]->getIndices(); // grab before old desc is nuked
    descriptors_[key] = value;
    descriptors_[key]->setIndices(indices); // propagate indices to descriptors
  }
}

void Collection::set(const igl::NameHandle& name, std::unique_ptr<Descriptor> value) {
  if (descriptors_.find(name) == descriptors_.cend()) {
    names_.push_back(name);
  }
  descriptors_[name] = std::move(value);
}

void Collection::clear(const igl::NameHandle& name) {
  names_.erase(std::remove(names_.begin(), names_.end(), name), names_.end());
  descriptors_.erase(name);
}

std::vector<igl::NameHandle> Collection::getNames() const noexcept {
  IGL_LOG_INFO_ONCE("Collection::getNames() is deprecated. Use Collection::names() instead\n");
  std::vector<igl::NameHandle> ret;
  ret.reserve(descriptors_.size());
  for (const auto& desc : descriptors_) {
    ret.push_back(desc.first);
  }
  return ret;
}

bool Collection::contains(const igl::NameHandle& name) const {
  return descriptors_.find(name) != descriptors_.end();
}

const Descriptor& Collection::get(const igl::NameHandle& name) const {
  auto it = descriptors_.find(name);
  IGL_DEBUG_ASSERT(descriptors_.cend() != it); // already exists
  IGL_DEBUG_ASSERT(it->second); // unique_ptr not null
  return *(it->second);
}

Descriptor& Collection::get(const igl::NameHandle& name) {
  return const_cast<Descriptor&>(static_cast<const Collection*>(this)->get(name));
}

bool Collection::operator==(const Collection& rhs) const noexcept {
  return descriptors_ == rhs.descriptors_;
}

bool Collection::operator!=(const Collection& rhs) const noexcept {
  return !operator==(rhs);
}

} // namespace iglu::uniform
