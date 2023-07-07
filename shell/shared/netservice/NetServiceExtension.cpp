/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/netservice/NetServiceExtension.h>

namespace igl::shell::netservice {

const char* NetServiceExtension::Name() noexcept {
  return "IglShellNetService";
}

} // namespace igl::shell::netservice
