/*******************************************************************************
 * Copyright (c) 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include <grpc/support/log.h>

#ifdef BAZEL_BUILD
#include "../protos/imperium.grpc.pb.h"
#else
#include "imperium.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using grpc::StatusCode;
//
using imperium::ClientMessage;
using imperium::ServerResponse;
using imperium::ImperiumRPC;

class OMRServerImpl final : public ImperiumRPC::Service {

   public:

      // Status SendOutFile(ServerContext* context, const FileString* fileString,
      //                   ExecBytes* execBytes) override {
      //
      //    std::cout << "File version from client: " << fileString->version() << '\n';
      //    std::cout << "File:" << '\n';
      //    std::cout << fileString->file() << '\n';
      //
      //    execBytes->set_size(512);
      //    execBytes->set_bytestream("0001101011010");
      //
      //    return Status::OK;
      //    }

     // Implement SendOutFiles bidirectional streaming for server
     Status SendMessage(ServerContext* context,
                       ServerReaderWriter<ServerResponse, ClientMessage>* stream) override {

        std::string serverStr[4] = {"0000011111", "01010101", "11110000", "000111000"};
        int count = 0;

        ClientMessage ff;
        while (stream->Read(&ff))
           {
              std::cout << "Server received file: " << ff.file() << '\n';
              std::cout << "Server file version: " << ff.version() << '\n';
              std::cout << "Count: " << count << '\n';

              ServerResponse e;
              e.set_bytestream(serverStr[count]);
              e.set_size((count + 2) * 64);

              std::cout << "Sending to client: " << serverStr[count] << '\n';
              count++;
              stream->Write(e);
              // Sleeps for 1 second
              std::this_thread::sleep_for (std::chrono::seconds(1));
           }

        if (context->IsCancelled()) {
          return Status(StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
        }

        return Status::OK;
        }

};

void RunServer() {
  std::string server_address("localhost:50055");
  OMRServerImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
