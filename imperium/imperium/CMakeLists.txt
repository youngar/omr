
set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
# set(_PROTOBUF_PROTOC ${CMAKE_BINARY_DIR})
set(_GRPC_GRPCPP_UNSECURE grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

set(omr_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/omr.pb.cc")
set(omr_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/omr.pb.h")
set(omr_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/omr.grpc.pb.cc")
set(omr_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/omr.grpc.pb.h")

# Proto file
get_filename_component(omr_proto "protos/omr.proto" ABSOLUTE)
get_filename_component(omr_proto_path "${omr_proto}" PATH)

add_custom_command(
  OUTPUT "${omr_proto_srcs}" "${omr_proto_hdrs}" "${omr_grpc_srcs}" "${omr_grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${omr_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${omr_proto}"
  DEPENDS "${omr_proto}")

add_library(imperium SHARED "src/imperium.cpp"
  ${omr_proto_srcs}
  ${omr_grpc_srcs})

target_link_libraries(imperium PUBLIC jitbuilder
  ${_GRPC_GRPCPP_UNSECURE}
  ${_PROTOBUF_LIBPROTOBUF})

target_compile_features(imperium PUBLIC cxx_std_11)

target_include_directories(imperium PUBLIC include/ "${CMAKE_CURRENT_BINARY_DIR}")
