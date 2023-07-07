# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.


function(igl_set_folder target folder_name)
  set_property(TARGET ${target} PROPERTY FOLDER ${folder_name})
endfunction()

function(igl_set_cxxstd target cpp_version)
  set_property(TARGET ${target} PROPERTY CXX_STANDARD ${cpp_version})
  set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
endfunction()
