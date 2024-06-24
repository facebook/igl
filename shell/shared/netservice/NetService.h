/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <memory>
#include <shell/shared/netservice/Stream.h>

namespace igl::shell::netservice {

class NetService {
 public:
  struct Delegate {
    virtual ~Delegate() = default;
    virtual void willPublish(NetService& sender) noexcept = 0;
    virtual void didNotPublish(NetService& sender, int errorCode, int errorDomain) noexcept = 0;
    virtual void didPublish(NetService& sender) noexcept = 0;
    virtual void willResolve(NetService& sender) noexcept = 0;
    virtual void didNotResolve(NetService& sender, int errorCode, int errorDomain) noexcept = 0;
    virtual void didResolveAddress(NetService& sender) noexcept = 0;
    virtual void didStop(NetService& sender) noexcept = 0;
    virtual void didAcceptConnection(NetService& sender,
                                     std::shared_ptr<InputStream> inputStream,
                                     std::shared_ptr<OutputStream> outputStream) noexcept = 0;
  };

  virtual ~NetService() = default;
  virtual void publish() noexcept = 0;

  [[nodiscard]] virtual std::shared_ptr<InputStream> getInputStream() const noexcept = 0;
  [[nodiscard]] virtual std::shared_ptr<OutputStream> getOutputStream() const noexcept = 0;
  [[nodiscard]] virtual std::string getName() const noexcept = 0;

  [[nodiscard]] Delegate* delegate() const noexcept {
    return delegate_.get();
  }

  void setDelegate(std::unique_ptr<Delegate> delegate) noexcept {
    IGL_ASSERT(delegate && delegate.get());
    delegate_ = std::move(delegate);
  }

 private:
  std::unique_ptr<Delegate> delegate_;
};

} // namespace igl::shell::netservice
