/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/netservice/apple/StreamApple.h>

#include <igl/Common.h>

#import <Foundation/Foundation.h>

// ----------------------------------------------------------------------------

@interface IGLShellStreamDelegateAdapter : NSObject <NSStreamDelegate> {
  igl::shell::netservice::StreamAdapterApple* owner_; // weak ref
}
@end

@implementation IGLShellStreamDelegateAdapter

- (instancetype)initWithOwner:(igl::shell::netservice::StreamAdapterApple*)owner {
  IGL_ASSERT(owner);
  self = [super init];
  if (self) {
    owner_ = owner;
  }
  return self;
}

- (void)stream:(NSStream*)stream handleEvent:(NSStreamEvent)eventCode {
  using namespace igl::shell::netservice;

  IGL_ASSERT(stream == owner_->nsStream());
  Stream::Event event = Stream::Event::None;
  switch (eventCode) {
  case NSStreamEventNone:
    event = Stream::Event::None;
    break;
  case NSStreamEventOpenCompleted:
    event = Stream::Event::OpenCompleted;
    break;
  case NSStreamEventHasBytesAvailable:
    event = Stream::Event::HasBytesAvailable;
    break;
  case NSStreamEventHasSpaceAvailable:
    event = Stream::Event::HasSpaceAvailable;
    break;
  case NSStreamEventErrorOccurred:
    event = Stream::Event::ErrorOccurred;
    break;
  case NSStreamEventEndEncountered:
    event = Stream::Event::EndEncountered;
    break;
  }

  if (owner_) {
    auto& observer = owner_->stream()->observer();
    if (observer) {
      observer(*owner_->stream(), event);
    }
  }
}
@end

// ----------------------------------------------------------------------------

namespace igl::shell::netservice {

// ----------------------------------------------------------------------------

StreamAdapterApple::~StreamAdapterApple() {
  delegate_ = nil;
  stream_ = nil;
  owner_ = nullptr;
}

bool StreamAdapterApple::initialize(Stream* owner, NSStream* stream) noexcept {
  IGL_ASSERT(owner && stream);
  bool result = IGL_VERIFY(nullptr == owner_ && nil == stream_);
  if (result) {
    owner_ = owner;
    stream_ = stream;
    delegate_ = [[IGLShellStreamDelegateAdapter alloc] initWithOwner:this];
  }
  return result;
}

void StreamAdapterApple::open() noexcept {
  [stream_ open];
  [stream_ setDelegate:delegate_];
  [stream_ scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

Stream::Status StreamAdapterApple::status() const noexcept {
  auto appleStatus = [stream_ streamStatus];
  switch (appleStatus) {
  case NSStreamStatusAtEnd:
    return Stream::Status::AtEnd;
  case NSStreamStatusClosed:
    return Stream::Status::Closed;
  case NSStreamStatusError:
    return Stream::Status::Error;
  case NSStreamStatusNotOpen:
    return Stream::Status::NotOpen;
  case NSStreamStatusOpen:
    return Stream::Status::Open;
  case NSStreamStatusOpening:
    return Stream::Status::Opening;
  case NSStreamStatusReading:
    return Stream::Status::Reading;
  case NSStreamStatusWriting:
    return Stream::Status::Writing;
  }
  IGL_UNREACHABLE_RETURN(Stream::Status::Error);
}

void StreamAdapterApple::close() noexcept {
  [stream_ removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  [stream_ close];
}

// ----------------------------------------------------------------------------

int InputStreamApple::read(uint8_t* outBuffer, size_t maxLength) const noexcept {
  IGL_ASSERT(outBuffer);
  return [inputStream() read:outBuffer maxLength:maxLength];
}

bool InputStreamApple::getBuffer(uint8_t*& outBuffer, size_t& outLength) const noexcept {
  IGL_ASSERT(outBuffer);
  return [inputStream() getBuffer:&outBuffer length:&outLength];
}

bool InputStreamApple::hasBytesAvailable() const noexcept {
  return [inputStream() hasBytesAvailable];
}

// ----------------------------------------------------------------------------

int OutputStreamApple::write(const uint8_t* buffer, size_t maxLength) noexcept {
  IGL_ASSERT(buffer);
  return [outputStream() write:buffer maxLength:maxLength];
}

bool OutputStreamApple::hasSpaceAvailable() const noexcept {
  return [outputStream() hasSpaceAvailable];
}

// ----------------------------------------------------------------------------

} // namespace igl::shell::netservice

// ----------------------------------------------------------------------------
