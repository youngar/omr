###############################################################################
# Copyright (c) 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
# set(_PROTOBUF_PROTOC ${CMAKE_BINARY_DIR})
set(_GRPC_GRPCPP_UNSECURE grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

set(imperium_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/imperium.pb.cc")
set(imperium_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/imperium.pb.h")
set(imperium_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/imperium.grpc.pb.cc")
set(imperium_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/imperium.grpc.pb.h")

# Proto file
get_filename_component(imperium_proto "protos/imperium.proto" ABSOLUTE)
get_filename_component(imperium_proto_path "${imperium_proto}" PATH)

add_custom_command(
  OUTPUT "${imperium_proto_srcs}" "${imperium_proto_hdrs}" "${imperium_grpc_srcs}" "${imperium_grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${imperium_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${imperium_proto}"
  DEPENDS "${imperium_proto}")

# add new .cpp files HERE
add_library(imperium SHARED "src/imperium.cpp"
  ${imperium_proto_srcs}
  ${imperium_grpc_srcs})

target_link_libraries(imperium PUBLIC jitbuilder
  ${_GRPC_GRPCPP_UNSECURE}
  ${_PROTOBUF_LIBPROTOBUF})

target_compile_features(imperium PUBLIC cxx_std_11)

target_include_directories(imperium PUBLIC include/ "${CMAKE_CURRENT_BINARY_DIR}")
