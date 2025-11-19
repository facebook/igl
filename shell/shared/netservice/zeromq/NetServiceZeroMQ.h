/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <shell/shared/netservice/NetService.h>
#include <shell/shared/netservice/apple/StreamApple.h>

@class NSNetService;
@protocol NSNetServiceDelegate;

namespace igl::shell::netservice {

class NetServiceZeroMQ : public NetService {
 public:
  NetServiceZeroMQ(std::string_view domain, std::string_view type, std::string_view name);
  NetServiceZeroMQ(NSNetService* netService);
  ~NetServiceZeroMQ() override;

  void publish() noexcept override;
  std::shared_ptr<InputStream> getInputStream() const noexcept override;
  std::shared_ptr<OutputStream> getOutputStream() const noexcept override;

  InputStreamApple& inputStream() const noexcept {
    return *inputStream_;
  }

  OutputStreamApple& outputStream() const noexcept {
    return *outputStream_;
  }

 protected:
  void initialize();

 private:
  NSNetService* netService_ = nil;
  id<NSNetServiceDelegate> netServiceDelegateAdapter_ = nil;
  std::shared_ptr<InputStreamZeroMQ> inputStream_;
  std::shared_ptr<OutputStreamZeroMQ> outputStream_;
};

} // namespace igl::shell::netservice
