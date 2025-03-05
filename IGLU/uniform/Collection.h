/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/uniform/Descriptor.h>
#include <igl/Common.h>
#include <igl/NameHandle.h>

#include <memory>
#include <unordered_map>

namespace iglu::uniform {

// Collection
//
// Holds a collection of uniform Descriptor instances keyed by igl::NameHandle
//
// To submit uniforms to the GPU, use uniform::Encoder.
struct Collection {
 public:
  Collection() = default;

  void update(const Collection& changes);

  // Sets the given uniform value.
  void set(const igl::NameHandle& name, std::unique_ptr<Descriptor> value);
  void clear(const igl::NameHandle& name);

  template<typename T>
  void set(const igl::NameHandle& name, T value) {
    auto& uniform = getOrCreate<T>(name);
    *uniform = std::move(value);
  }

  // Gets the reference of the descriptor with the given name and type.
  // If a descriptor with that name doesn't exist, a new descriptor with the given
  // name and type is created.
  //
  // ## Implementation Notes
  //
  // To implement getOrCreate, we use SFINAE (https://en.cppreference.com/w/cpp/types/enable_if)
  // to conditionally remove functions from overload resolution based on type traits
  // and to provide separate function overloads for different type traits:
  //
  // 1. for a T type that's *not* a vector
  // 2. for a T type that's a vector
  //
  // ## Appendix: Alternative
  //
  // An alternative that doesn't rely on SFINAE trickery:
  //
  //   struct Detail {
  //     template <typename T, bool isSpecialization> struct DescSelector;
  //
  //     template <typename T>
  //     struct DescSelector<T, false> { using Type = DescriptorValue<T>; };
  //
  //     template <typename T>
  //     struct DescSelector<T, true> { using Type = DescriptorVector<typename T::value_type>; };
  //
  //     template <typename T, bool B>
  //     using DescSelector_t = typename DescSelector<T, B>::Type;
  //   };
  //
  //   template<typename T>
  //   auto& getOrCreate(const igl::NameHandle& name) {
  //     using Desc = Detail::DescSelector_t<T, IsSpecialization<T, std::vector>::value>;
  //     auto& uniform = findOrCreate<Desc>(name);
  //     return uniform;
  //   }

  // 1. Override for a T type that's *not* a vector
  template<typename T,
           std::enable_if_t<!IsSpecialization<T, std::vector>::value, std::nullptr_t> = nullptr>
  auto& getOrCreate(const igl::NameHandle& name) {
    using Desc = DescriptorValue<T>;
    auto& uniform = findOrCreate<Desc>(name);
    return uniform;
  }

  // 2. Override for a T type that's a vector
  template<typename T,
           std::enable_if_t<IsSpecialization<T, std::vector>::value, std::nullptr_t> = nullptr>
  auto& getOrCreate(const igl::NameHandle& name) {
    using Desc = DescriptorVector<typename T::value_type>;
    auto& uniform = findOrCreate<Desc>(name);
    return uniform;
  }

  // Gets the reference of the descriptor with the given name
  const Descriptor& get(const igl::NameHandle& name) const;
  Descriptor& get(const igl::NameHandle& name);

  // Checks if the name is in the collection
  bool contains(const igl::NameHandle& name) const;

  // DEPRECATED: use names() instead
  // Gets the list of NameHandles
  [[nodiscard]] std::vector<igl::NameHandle> getNames() const noexcept;

  const std::vector<igl::NameHandle>& names() const noexcept {
    return names_;
  }

  bool operator==(const Collection& rhs) const noexcept;
  bool operator!=(const Collection& rhs) const noexcept;

 private:
  template<typename Desc>
  Desc& findOrCreate(const igl::NameHandle& name) {
    auto& entry = descriptors_[name];
    // Create entry with the type Desc if it doesn't exist
    if (!entry) {
      entry = std::make_unique<Desc>();
      names_.push_back(name);
    }
    return static_cast<Desc&>(*entry);
  }

 private:
  std::unordered_map<igl::NameHandle, std::shared_ptr<Descriptor>> descriptors_;
  std::vector<igl::NameHandle> names_;
};

} // namespace iglu::uniform
