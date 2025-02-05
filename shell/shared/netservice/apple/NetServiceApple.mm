/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/shared/netservice/apple/NetServiceApple.h>

#include <igl/Common.h>

// ----------------------------------------------------------------------------

@interface IGLShellNetServiceDelegateAdapter : NSObject <NSNetServiceDelegate> {
  igl::shell::netservice::NetServiceApple* owner_; // weak ref
}
@end

@implementation IGLShellNetServiceDelegateAdapter
- (id)initWithOwner:(igl::shell::netservice::NetServiceApple*)owner {
  self = [super init];
  if (self) {
    owner_ = owner;
  }
  return self;
}

- (void)netServiceWillPublish:(NSNetService*)sender {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    target->willPublish(*owner_);
  }
}

- (void)netService:(NSNetService*)sender
     didNotPublish:(NSDictionary<NSString*, NSNumber*>*)errorDict {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    int errorCode = [[errorDict valueForKey:NSNetServicesErrorCode] intValue];
    int errorDomain = [[errorDict valueForKey:NSNetServicesErrorDomain] intValue];
    target->didNotPublish(*owner_, errorCode, errorDomain);
  }
}

- (void)netServiceDidPublish:(NSNetService*)sender {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    target->didPublish(*owner_);
  }
}

- (void)netServiceWillResolve:(NSNetService*)sender {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    target->willResolve(*owner_);
  }
}

- (void)netService:(NSNetService*)sender
     didNotResolve:(NSDictionary<NSString*, NSNumber*>*)errorDict {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    int errorCode = [[errorDict valueForKey:NSNetServicesErrorCode] intValue];
    int errorDomain = [[errorDict valueForKey:NSNetServicesErrorDomain] intValue];
    target->didNotResolve(*owner_, errorCode, errorDomain);
  }
}

- (void)netServiceDidResolveAddress:(NSNetService*)sender {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    target->didResolveAddress(*owner_);
  }
}

- (void)netServiceDidStop:(NSNetService*)sender {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    target->didStop(*owner_);
  }
}

- (void)netService:(NSNetService*)sender
    didAcceptConnectionWithInputStream:(NSInputStream*)inputStream
                          outputStream:(NSOutputStream*)outputStream {
  auto* target = (owner_ ? owner_->delegate() : nullptr);
  if (target) {
    auto input = std::make_shared<igl::shell::netservice::InputStreamApple>();
    input->initialize(inputStream);
    auto output = std::make_shared<igl::shell::netservice::OutputStreamApple>();
    output->initialize(outputStream);

    target->didAcceptConnection(*owner_, std::move(input), std::move(output));
  }
}

@end

// ----------------------------------------------------------------------------

namespace igl::shell::netservice {
namespace {
NSString* fromStringView(std::string_view str) {
  return [[NSString alloc] initWithBytes:str.data()
                                  length:str.size()
                                encoding:NSUTF8StringEncoding];
}
} // namespace

NetServiceApple::NetServiceApple(std::string_view domainStr,
                                 std::string_view typeStr,
                                 std::string_view nameStr) {
  NSString* domain = fromStringView(domainStr);
  NSString* type = fromStringView(typeStr);
  NSString* name = fromStringView(nameStr);

  // Use port 0 to assign random (available) port
  // Otherwise publishing with NSNetServiceListenForConnections fails
  netService_ = [[NSNetService alloc] initWithDomain:domain type:type name:name port:0];

  initialize();
}

NetServiceApple::NetServiceApple(NSNetService* netService) : netService_(netService) {
  initialize();

  NSInputStream* inputStream = nil;
  NSOutputStream* outputStream = nil;
  [netService_ getInputStream:&inputStream outputStream:&outputStream];
  IGL_DEBUG_ASSERT(inputStream);
  IGL_DEBUG_ASSERT(outputStream);

  inputStream_->initialize(inputStream);
  outputStream_->initialize(outputStream);
}

void NetServiceApple::initialize() {
  if (IGL_DEBUG_VERIFY(nil == netServiceDelegateAdapter_)) {
    netServiceDelegateAdapter_ = [[IGLShellNetServiceDelegateAdapter alloc] initWithOwner:this];
    [netService_ setDelegate:netServiceDelegateAdapter_];

    inputStream_ = std::make_shared<InputStreamApple>();
    outputStream_ = std::make_shared<OutputStreamApple>();
  }
}

NetServiceApple::~NetServiceApple() {
  [netService_ setDelegate:nil];
  netServiceDelegateAdapter_ = nil;
  netService_ = nil;
}

void NetServiceApple::publish() noexcept {
  [netService_ publishWithOptions:NSNetServiceListenForConnections];
}

std::shared_ptr<InputStream> NetServiceApple::getInputStream() const noexcept {
  return inputStream_;
}

std::shared_ptr<OutputStream> NetServiceApple::getOutputStream() const noexcept {
  return outputStream_;
}

std::string NetServiceApple::getName() const noexcept {
  return {[[netService_ name] UTF8String]};
}

} // namespace igl::shell::netservice
