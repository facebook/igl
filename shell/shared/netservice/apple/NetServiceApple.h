/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <shell/shared/netservice/NetService.h>
#include <shell/shared/netservice/apple/StreamApple.h>
#include <string>

@class NSNetService;
@protocol NSNetServiceDelegate;

namespace igl::shell::netservice {

class NetServiceApple final : public NetService {
 public:
  NetServiceApple(std::string_view domain, std::string_view type, std::string_view name);
  NetServiceApple(NSNetService* netService);
  ~NetServiceApple() final;

  void publish() noexcept final;
  [[nodiscard]] std::shared_ptr<InputStream> getInputStream() const noexcept final;
  [[nodiscard]] std::shared_ptr<OutputStream> getOutputStream() const noexcept final;
  [[nodiscard]] std::string getName() const noexcept final;

  [[nodiscard]] InputStreamApple& inputStream() const noexcept {
    return *inputStream_;
  }

  [[nodiscard]] OutputStreamApple& outputStream() const noexcept {
    return *outputStream_;
  }

 protected:
  void initialize();

 private:
  NSNetService* netService_ = nil;
  id<NSNetServiceDelegate> netServiceDelegateAdapter_ = nil;
  std::shared_ptr<InputStreamApple> inputStream_;
  std::shared_ptr<OutputStreamApple> outputStream_;
};

} // namespace igl::shell::netservice
