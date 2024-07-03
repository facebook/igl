/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFunctions.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl::vulkan {

struct VulkanContextConfig;

/// @brief The VulkanExtensions class is a helper class that manages instance and device
/// extensions in Vulkan by enumerating all extensions available for either object and storing the
/// names of the available ones as std::strings. A call to either `enumerate()` or
/// `enumerate(VkPhysicalDevice)` must be performed before the class can be used. After enumeration,
/// this class allows users to enable an object's extension by name by first checking them against
/// all available extensions of that type. Only available extensions are stored as enabled
/// internally. The class also provides helper functions to return all available extensions of a
/// type, checking whether an extension is available without modifying the internal storage of the
/// object, checking if an extension has been enabled for an object and, finally, a method to return
/// a list of all enabled extensions of a type as `const char *`, which is accepted by the
/// VUlkan API
class VulkanExtensions final {
 public:
  /// @brief Helper enumeration to determine which extension is being used. It's converted to a
  /// size_t internally to help access the right list of enumerations
  enum class ExtensionType { Instance = 0, Device };

  explicit VulkanExtensions();
  VulkanExtensions(const VulkanExtensions&) = delete;
  VulkanExtensions operator=(const VulkanExtensions&) = delete;
  ~VulkanExtensions() = default;

  /// @brief Enumerates all instance extensions and stores their names internally in a vector of
  /// std::strings
  void enumerate(const VulkanFunctionTable& vf);

  /// @brief Enumerates all physical device extensions and stores their names internally in a vector
  /// of std::strings
  /// @param The physical device to use as a reference for the enumeration
  void enumerate(const VulkanFunctionTable& vf, VkPhysicalDevice device);

  /// @brief Returns all available extensions of a type
  /// @param extensionType The type of the extensions
  [[nodiscard]] const std::vector<std::string>& allAvailableExtensions(
      ExtensionType extensionType) const;

  /// @brief Returns true if the extension with name equal to `extensionName` of the type
  /// `extensionType` is available and false otherwise
  /// @param extensionName The name of the extension
  /// @param extensionType The type of the extensions to return
  bool available(const char* extensionName, ExtensionType extensionType) const;

  /// @brief Enables the common extensions used in IGL for a particular type. The
  /// `validationEnabled` parameter helps the method enable certain extensions that depend on the
  /// validation layer being enabled or not
  /// @param extensionType The type of the extensions
  /// @param validationEnabled Flag that informs the class whether the Validation Layer is
  /// enabled or not.
  void enableCommonExtensions(ExtensionType extensionType, const VulkanContextConfig& config);

  /// @brief Enables the extension with name `extensionName` of the type `extensionType` if the
  /// extension is available. If an instance or physical device deoesn't support the
  /// extension, this method is a no-op
  /// @param extensionName The name of the extension
  /// @param extensionType The type of the extension
  /// @return True if the extension is available, false otherwise
  bool enable(const char* extensionName, ExtensionType extensionType);

  /// @brief Returns true if an extension with name `extensionName` is enabled and false otherwise.
  /// This method will check the extension against the list of enabled ones for the
  /// instance and the physical device
  /// @param extensionName The name of the extension
  /// @return True if the extension has been enabled, false otherwise
  bool enabled(const char* extensionName) const;

  /// @brief Enables the extension with name `extensionName` of the type `extensionType`.
  /// Use this method to enable prorietory extensions which are not reported in the extensions list.
  /// @param extensionName The name of the extension
  /// @param extensionType The type of the extension
  void forceEnable(const char* extensionName, ExtensionType extensionType);

  /// @brief Returns a vector of `const char *` of all enabled extensions for an instance or phyical
  /// device. This method is particularly useful because Vulkan expects an
  /// array of `const char *` with the names of the extensions to enable
  /// @param extensionType The type of the extensions
  /// @return A vector of `const char *` with all enabled extensions of type `extensionType`. The
  /// return value must not outlive the instance of this class, as the pointers in the returned
  /// vector point to the strings stored internally in this class
  [[nodiscard]] std::vector<const char*> allEnabled(ExtensionType extensionType) const;

 private:
  static constexpr size_t kNumberOfExtensionTypes = 2;
  /// @brief a Vector of a vector of strings. The outer vector stores two vectors, one for each
  /// object type (instance and physical device). The inner vector is the list of all available
  /// extensions for the type
  std::vector<std::vector<std::string>> extensions_;

  /// @brief a Vector of unordered_sets of string. The outer vector stores two sets, one for each
  /// object type (instance and device). The inner set is the list of all extensions enabled for the
  /// type
  std::vector<std::unordered_set<std::string>> enabledExtensions_;
};

} // namespace igl::vulkan
