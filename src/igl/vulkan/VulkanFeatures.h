/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

#if !defined(VK_QCOM_multiview_per_view_viewports)
// this is copied from `vulkan_core.h`, just in case we compile using some old Vulkan headers
// NOLINTBEGIN(modernize-use-using)
// NOLINTBEGIN(readability-identifier-naming)
#define VK_QCOM_multiview_per_view_viewports 1
#define VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS_SPEC_VERSION 1
#define VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS_EXTENSION_NAME "VK_QCOM_multiview_per_view_viewports"
typedef struct VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM {
  VkStructureType sType;
  void* pNext;
  VkBool32 multiviewPerViewViewports;
} VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM;
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_VIEWPORTS_FEATURES_QCOM \
  (VkStructureType)1000488000
// NOLINTEND(readability-identifier-naming)
// NOLINTEND(modernize-use-using)
#endif // VK_QCOM_multiview_per_view_viewports

namespace igl::vulkan {

class VulkanContext;

/// @brief Creates and maintains a list of feature structures for checking feature availability and
/// feature selection. This class provides a way to quickly enable the default and required
/// features. This class also manages instance and device extensions in Vulkan by enumerating all
/// extensions available for either object and storing the names of the available ones as
/// std::strings. A call to either `enumerate()` or `enumerate(VkPhysicalDevice)` must be performed
/// before the class can be used. After enumeration, this class allows users to enable an object's
/// extension by name by first checking them against all available extensions of that type. Only
/// available extensions are stored as enabled internally. The class also provides helper functions
/// to return all available extensions of a type, checking whether an extension is available without
/// modifying the internal storage of the object, checking if an extension has been enabled for an
/// object and, finally, a method to return a list of all enabled extensions of a type as `const
/// char *`, which is accepted by the Vulkan API
class VulkanFeatures final {
 public:
  /// @brief Helper enumeration to determine which extension is being used. It's converted to a
  /// size_t internally to help access the right list of enumerations
  enum class ExtensionType { Instance = 0, Device };

  explicit VulkanFeatures(VulkanContextConfig config) noexcept;

  /// @brief Populates the VkPhysicalDeviceFeatures2 and its pNext chain for a Vulkan context
  void populateWithAvailablePhysicalDeviceFeatures(const VulkanContext& context,
                                                   VkPhysicalDevice physicalDevice) noexcept;

  /// @brief Checks the features enabled in this class against the ones passed
  /// in as a parameter in 'availableFeatures'. If a requested feature is not present, the class
  /// logs the message and returns a failure
  [[nodiscard]] Result checkSelectedFeatures(
      const VulkanFeatures& availableFeatures) const noexcept;

  VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2{};

  // Vulkan 1.1
  VkPhysicalDeviceSamplerYcbcrConversionFeatures featuresSamplerYcbcrConversion{};
  VkPhysicalDeviceShaderDrawParametersFeatures featuresShaderDrawParameters{};
  VkPhysicalDeviceMultiviewFeatures featuresMultiview{};

  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR featuresBufferDeviceAddress{};
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT featuresDescriptorIndexing{};
  VkPhysicalDevice16BitStorageFeatures features16BitStorage{};

  // Vulkan 1.2
  VkPhysicalDeviceShaderFloat16Int8Features featuresShaderFloat16Int8{};
  VkPhysicalDeviceIndexTypeUint8FeaturesEXT featuresIndexTypeUint8{};
  VkPhysicalDeviceSynchronization2FeaturesKHR featuresSynchronization2{};
  VkPhysicalDeviceTimelineSemaphoreFeaturesKHR featuresTimelineSemaphore{};
  VkPhysicalDeviceFragmentDensityMapFeaturesEXT featuresFragmentDensityMap{};
  VkPhysicalDeviceVulkanMemoryModelFeaturesKHR featuresVulkanMemoryModel{};
  VkPhysicalDevice8BitStorageFeaturesKHR features8BitStorage{};
  VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR featuresUniformBufferStandardLayout{};
  VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM featuresMultiviewPerViewViewports{};

  // We need to reassemble the feature chain because of the pNext pointers
  VulkanFeatures& operator=(const VulkanFeatures& other) noexcept;

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
  void enableCommonInstanceExtensions(const VulkanContextConfig& config);
  void enableCommonDeviceExtensions(const VulkanContextConfig& config);

 public:
  friend class Device;
  friend class VulkanContext;

  // A copy of the config used by the VulkanContext
  VulkanContextConfig config_{};

  // NOLINTBEGIN(readability-identifier-naming)
  bool has_VK_EXT_descriptor_indexing = false;
  bool has_VK_EXT_fragment_density_map = false;
  bool has_VK_EXT_headless_surface = false;
  bool has_VK_EXT_index_type_uint8 = false;
  bool has_VK_EXT_queue_family_foreign = false;
  bool has_VK_KHR_8bit_storage = false;
  bool has_VK_KHR_buffer_device_address = false;
  bool has_VK_KHR_create_renderpass2 = false;
  bool has_VK_KHR_shader_non_semantic_info = false;
  bool has_VK_KHR_synchronization2 = false;
  bool has_VK_KHR_timeline_semaphore = false;
  bool has_VK_KHR_uniform_buffer_standard_layout = false;
  bool has_VK_KHR_vulkan_memory_model = false;
  bool has_VK_QCOM_multiview_per_view_viewports = false;
  // NOLINTEND(readability-identifier-naming)

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

  /// @brief Assembles the feature chain for the VkPhysicalDeviceFeatures2 structure by connecting
  /// the existing/required feature structures and their pNext chain.
  void assembleFeatureChain(const VulkanContextConfig& config) noexcept;
  bool hasExtension(const char* ext) const;

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
  [[nodiscard]] bool enabled(const char* extensionName) const;

  /// @brief Returns a vector of `const char *` of all enabled extensions for an instance or phyical
  /// device. This method is particularly useful because Vulkan expects an
  /// array of `const char *` with the names of the extensions to enable
  /// @param extensionType The type of the extensions
  /// @return A vector of `const char *` with all enabled extensions of type `extensionType`. The
  /// return value must not outlive the instance of this class, as the pointers in the returned
  /// vector point to the strings stored internally in this class
  [[nodiscard]] std::vector<const char*> allEnabled(ExtensionType extensionType) const;

  std::vector<VkExtensionProperties> extensionProps_;
};

} // namespace igl::vulkan
