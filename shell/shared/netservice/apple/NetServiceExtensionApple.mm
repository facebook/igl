/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/netservice/apple/NetServiceExtensionApple.h>

#include <shell/shared/netservice/apple/NetServiceApple.h>

@interface IGLShellNetBrowserServiceDelegateAdapter : NSObject <NSNetServiceBrowserDelegate> {
  igl::shell::netservice::NetServiceExtensionApple* owner_; // weak ref
}
@end

@implementation IGLShellNetBrowserServiceDelegateAdapter

- (instancetype)initWithOwner:(igl::shell::netservice::NetServiceExtensionApple*)owner {
  IGL_ASSERT(owner);
  self = [super init];
  if (self) {
    owner_ = owner;
  }
  return self;
}

- (void)netServiceBrowserDidStopSearch:(NSNetServiceBrowser*)browser {
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)browser didNotSearch:(NSDictionary*)errorDict {
  IGL_ASSERT(owner_);
  owner_->stopSearch();
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)browser
           didFindService:(NSNetService*)netService
               moreComing:(BOOL)moreComing {
  IGL_ASSERT(owner_);
  auto& delegate = owner_->delegate();
  bool keepSearching = moreComing;
  if (delegate) {
    auto service = std::make_unique<igl::shell::netservice::NetServiceApple>(netService);
    keepSearching = delegate(*owner_, std::move(service), moreComing);
  }
  if (!keepSearching) {
    owner_->stopSearch();
  }
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)browser
         didRemoveService:(NSNetService*)netService
               moreComing:(BOOL)moreComing {
  IGL_ASSERT(owner_);
  auto delegate = owner_->delegate();
  if (delegate) {
    // delegate->didRemoveService(*owner_, service);
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

NetServiceExtensionApple::~NetServiceExtensionApple() {
  netServiceBrowser_ = nil;
  netServiceBrowserDelegate_ = nil;
}

bool NetServiceExtensionApple::initialize(igl::shell::Platform& /*platform*/) noexcept {
  if (!netServiceBrowser_) {
    netServiceBrowser_ = [[NSNetServiceBrowser alloc] init];
    netServiceBrowserDelegate_ =
        [[IGLShellNetBrowserServiceDelegateAdapter alloc] initWithOwner:this];
    netServiceBrowser_.delegate = netServiceBrowserDelegate_;
  }

  return true;
}

std::unique_ptr<NetService> NetServiceExtensionApple::create(std::string_view domain,
                                                             std::string_view type,
                                                             std::string_view name,
                                                             int /*port*/) const noexcept {
  return std::make_unique<NetServiceApple>(domain, type, name);
}

void NetServiceExtensionApple::search(std::string_view domain,
                                      std::string_view type) const noexcept {
  if (IGL_VERIFY(!domain.empty())) {
    NSString* serviceType = fromStringView(type);
    NSString* serviceDomain = fromStringView(domain); // @"local."
    [netServiceBrowser_ searchForServicesOfType:serviceType inDomain:serviceDomain];
  }
}

void NetServiceExtensionApple::stopSearch() const noexcept {
  if (IGL_VERIFY(netServiceBrowser_)) {
    [netServiceBrowser_ stop];
  }
}

IGL_API IGLShellExtension* IGLShellExtension_NewIglShellNetService() {
  auto extension = new NetServiceExtensionApple;
  return static_cast<IGLShellExtension*>(extension);
}

} // namespace igl::shell::netservice

// TODO: Find better way to prevent stripping
@interface IglShellApple_NetServiceExtension : NSObject
@end

@implementation IglShellApple_NetServiceExtension
+ (IGLShellExtension_NewCFunction)cFunc {
  return igl::shell::netservice::IGLShellExtension_NewIglShellNetService;
}
@end
