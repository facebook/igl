/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/uniform/Trait.h>
#include <glm/glm.hpp>
#include <memory>
#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/Shader.h>
#include <igl/Uniform.h>

namespace iglu::uniform {

// ----------------------------------------------------------------------------

namespace {

enum class Alignment { Packed, Aligned };

template<typename T>
struct PackedValue {
  T value;
  static_assert(Trait<T>::kPadding == 0, "!");

  PackedValue() = default;
  explicit PackedValue(T v) : value(std::move(v)) {}

  void* data(Alignment /*unused*/) noexcept {
    return &value;
  }

  [[nodiscard]] const void* data(Alignment /*unused*/) const noexcept {
    return &value;
  }
};

template<typename T>
struct AlignedValue {
  static_assert(Trait<T>::kPadding > 0, "Only T types that require padding should be used!");

  T value;
  uint8_t padding[Trait<T>::kPadding]; // pads the end of the struct to ensure alignment

  AlignedValue() = default;
  explicit AlignedValue(T v) : value(std::move(v)) {}

  void* data(Alignment /*unused*/) noexcept {
    return &value;
  }

  [[nodiscard]] const void* data(Alignment /*unused*/) const noexcept {
    return &value;
  }
};

template<typename U>
struct AlignedElement : AlignedValue<U> {
  using AlignedValue<U>::AlignedValue;
};

// glm::mat3 requires padding within each row, not just at the end
// If aligned data is requested, an update occurs: packed => aligned
template<>
struct AlignedElement<glm::mat3> {
  using Self = AlignedElement<glm::mat3>;
  using AlignedMat3 = Trait<glm::mat3>::Aligned;

  glm::mat3 value; // this is the "source of truth"
  mutable AlignedMat3 valueAligned; // padded element shadows value

  AlignedElement() = default;
  explicit AlignedElement(glm::mat3 v) : value(v) {}

  void* data(Alignment alignment) noexcept {
    return const_cast<void*>(static_cast<const Self*>(this)->data(alignment));
  }

  const void* data(Alignment alignment) const noexcept {
    if (Alignment::Aligned == alignment) {
      Trait<glm::mat3>::toAligned(valueAligned, value); // Sync from source of truth
      return &valueAligned;
    }
    IGL_DEBUG_ASSERT(Alignment::Packed == alignment);
    return &value;
  }
};

template<typename U>
struct AlignedElementInVector : AlignedValue<U> {
  using AlignedValue<U>::AlignedValue;
  AlignedElementInVector& operator=(const U& src) noexcept {
    this->value = src;
    return *this;
  }
};

// glm::mat3 needs specialization b/c requires padding within each row, not just at the end
template<>
struct AlignedElementInVector<glm::mat3> : PackedValue<typename Trait<glm::mat3>::Aligned> {
  using PackedValue<typename Trait<glm::mat3>::Aligned>::PackedValue;
  AlignedElementInVector& operator=(const glm::mat3& src) noexcept {
    Trait<glm::mat3>::toAligned(this->value, src);
    return *this;
  }
};
} // namespace

// ----------------------------------------------------------------------------

// Descriptor, DescriptorValue<T>, DescriptorVector<T>
//
// These classes are intended to be used to encapsulate simple uniforms. In particular,
// it's designed for the use case of small uniforms < 4KB (vs uniform buffers/blocks)
//
// Descriptor is the base interface so you can hold heterogeneous collections
// of different uniforms, i.e. a mix of DescriptorValue<T> and DescriptorVector<T>
// with different T types.
//
// To store the actual uniform data, you instantiate one of the following:
//
// * DescriptorValue<T> is for single T values.
// * DescriptorVector<T> is for a vector of T values.
//
// To submit this uniform to the GPU, use uniform::Encoder.
struct Descriptor {
 protected:
  explicit Descriptor(igl::UniformType type);

 public:
  virtual ~Descriptor() = default;

  [[nodiscard]] virtual const void* data(Alignment alignment) const noexcept = 0;

  [[nodiscard]] virtual size_t numBytes(Alignment alignment) const noexcept = 0;

  [[nodiscard]] virtual size_t size() const noexcept {
    return 1;
  }

  [[nodiscard]] igl::UniformType getType() const noexcept;

  [[nodiscard]] int getIndex(igl::ShaderStage stage) const noexcept;
  void setIndex(igl::ShaderStage stage, int newValue) noexcept;

  using Indices = std::array<int, 2>;
  [[nodiscard]] Indices getIndices() const noexcept {
    return indices_;
  }
  void setIndices(Indices indices) noexcept {
    indices_ = indices;
  }

  void toUniformDescriptor(int location, igl::UniformDesc& outDescriptor) const noexcept;

 private:
  igl::UniformType type_ = igl::UniformType::Invalid;
  Indices indices_ = {-1, -1}; // index for each shader stage
};

// ----------------------------------------------------------------------------

// DescriptorValue<T>
//
// DescriptorValue<T> represents a single value uniform
//
//   glm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f)
//   DescriptorValue<glm::vec4> colorUniform(std::move(red));
//
// You can access the underlying value using pointer semantics:
//
//   glm::vec4& color = *colorUniform;
//
template<typename T>
class DescriptorValue : public Descriptor {
  static constexpr bool kNoPadding = (Trait<T>::kPadding == 0);

  template<typename U>
  using PackedElement = PackedValue<U>;

  using Element = std::conditional_t<kNoPadding, PackedElement<T>, AlignedElement<T>>;

 public:
  using Self = DescriptorValue<T>;

  DescriptorValue() : Descriptor(Trait<T>::kValue) {}
  explicit DescriptorValue(T value) : Descriptor(Trait<T>::kValue), element_(std::move(value)) {}

  [[nodiscard]] const void* data(Alignment alignment) const noexcept override {
    return element_.data(alignment);
  }

  [[nodiscard]] size_t numBytes(Alignment alignment) const noexcept override {
    IGL_DEBUG_ASSERT(sizeForUniformType(getType()) <= sizeof(Element));

    // NOTE: Any padding required for T to be aligned will be present in Element
    return sizeof(T) + (Alignment::Packed == alignment ? 0 : Trait<T>::kPadding);
  }

  const T& operator*() const noexcept {
    return element_.value;
  }

  T& operator*() noexcept {
    return element_.value;
  }

 private:
  Element element_;
};

// ----------------------------------------------------------------------------

// DescriptorVector<T>
//
// DescriptorVector<T> represents a vector of T values. It provides underlying
// storage for the data:
//
//   size_t numParticles = 10;
//   std::vector<glm::vec3> colors;
//   colors.reserve(numParticles);
//   for(int i = 0; i < numParticles; ++i) {
//     colors.emplace_back(1.0, 1.0, (float)i/numParticles);
//   }
//   DescriptorVector<glm::vec3> particleColors(std::move(colors));
//
// You can access the underlying vector using pointer semantics:
//
//   std::vector<glm::vec3>& colors = *particleColors;
//
// NOTE: DescriptorVector will have a parallel internal std::vector where each
// element is aligned for T types that require it.
//
template<typename T, typename Vector = std::vector<T>>
class DescriptorVector : public Descriptor {
  static constexpr bool kNoPadding = (Trait<T>::kPadding == 0);

  struct PackedContainer {
    Vector values;
    PackedContainer() = default;
    explicit PackedContainer(Vector vec) : values(std::move(vec)) {}

    void* data(Alignment /*unused*/) noexcept {
      return values.data();
    }

    [[nodiscard]] const void* data(Alignment /*unused*/) const noexcept {
      return values.data();
    }

    [[nodiscard]] size_t elementSize(Alignment /*unused*/) const noexcept {
      return sizeof(T);
    }
  };

  // Contains both packed and aligned vectors
  // If aligned data is requested, an update occurs: packed => aligned
  struct DualContainer {
    Vector values; // this is the "source of truth"
    // padded elements shadow those in values
    mutable std::vector<AlignedElementInVector<T>> valuesAligned;

    DualContainer() = default;
    explicit DualContainer(Vector vec) : values(std::move(vec)) {}

    void* data(Alignment /*alignment*/) noexcept {
      return const_cast<void*>(static_cast<const Self*>(this)->data());
    }

    const void* data(Alignment alignment) const noexcept {
      if (Alignment::Aligned == alignment) {
        // Sync from source of truth
        const size_t numElements = values.size();
        valuesAligned.resize(numElements);
        for (size_t i = 0; i < numElements; ++i) {
          const auto& src = values[i];
          auto& dst = valuesAligned[i];
          dst = src;
        }
        return valuesAligned.data();
      }
      IGL_DEBUG_ASSERT(Alignment::Packed == alignment);
      return values.data();
    }

    size_t elementSize(Alignment alignment) const noexcept {
      return (Alignment::Packed == alignment ? sizeof(T) : sizeof(AlignedElementInVector<T>));
    }
  };

  using Container = std::conditional_t<kNoPadding, PackedContainer, DualContainer>;

 public:
  using Self = DescriptorVector<T>;

  DescriptorVector() : Descriptor(Trait<T>::kValue) {}
  explicit DescriptorVector(Vector values) :
    Descriptor(Trait<T>::kValue), container_(std::move(values)) {}

  [[nodiscard]] const void* data(Alignment alignment) const noexcept override {
    return container_.data(alignment);
  }

  [[nodiscard]] size_t numBytes(Alignment alignment) const noexcept override {
    size_t elementSize = container_.elementSize(alignment);
    IGL_DEBUG_ASSERT(sizeForUniformType(getType()) <= elementSize);
    return container_.values.size() * elementSize;
  }

  [[nodiscard]] size_t size() const noexcept override {
    return container_.values.size();
  }

  const Vector& operator*() const noexcept {
    return container_.values;
  }

  Vector& operator*() noexcept {
    return container_.values;
  }

 private:
  Container container_;
};

// ----------------------------------------------------------------------------

} // namespace iglu::uniform
