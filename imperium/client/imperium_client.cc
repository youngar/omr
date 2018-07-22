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

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/JitBuilderRecorderTextFile.hpp"
#include "ilgen/JitBuilderReplayTextFile.hpp"
#include "ilgen/MethodBuilderReplay.hpp"
#include "omrthread.h"

#include "imperium/imperium.hpp"

#include <grpcpp/grpcpp.h>

#include "imperium.grpc.pb.h"

 using namespace OMR::Imperium;

using std::cout;
using std::cerr;
extern bool jitBuilderShouldCompile;

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using std::chrono::system_clock;

using imperium::ClientMessage;
using imperium::ServerResponse;
using imperium::ImperiumRPC;

ClientMessage MakeFileString(const std::string& message, int version)
   {
   ClientMessage fileString;

   fileString.set_file(message);
   fileString.set_version(version);
   return fileString;
   }

class SimpleMethod : public TR::MethodBuilder
   {
   public:
      SimpleMethod(TR::TypeDictionary *d, TR::JitBuilderRecorder *recorder)
      : MethodBuilder(d, recorder)
      {

      DefineLine(LINETOSTR(__LINE__));
      // cout << LINETOSTR(__LINE__) << " // CHECKING VALUE OF DEFINELINE \n";
      DefineFile(__FILE__);

      DefineName("increment");

      // ORIGINAL SIMPLE.cpp
      DefineParameter("value", Int32);

      // COMPLICATED SIMPLE.cpp
      // DefineParameter("value1", Int32);
      // DefineParameter("value2", Int32);
      DefineReturnType(Int32);
      }

   bool
   buildIL()
      {
      cout << "SimpleMethod::buildIL() running!\n";

      // MOST SIMPLE SIMPLE.cpp
      // Return(
      //    ConstInt32(88));

      // *****************************************************************
      // ORIGINAL SIMPLE.cpp
      Return(
         Add(
            Load("value"),
            ConstInt32(1)));

      // *****************************************************************
      // COMPLICATED SIMPLE.cpp
      // Return(
      //    Add(
      //      Add(
      //        Load("value1"),
      //        ConstInt32(17)
      //      ),
      //      Add(
      //        Load("value2"),
      //        ConstInt32(14))));

      // IlValue * value1 = Load("value1");
      return true;
      }
};

class OMRClient {

public:
   OMRClient(std::shared_ptr<Channel> channel)
     : stub_(ImperiumRPC::NewStub(channel)) {}

   // void SendOutFile()
   //   {
   //     OMR::Imperium::ClientChannel clientChannel;
   //     clientChannel.createClientThread();
   //
   //     char * temp1 = strdup("First elemenet on the queue");
   //     char * temp2 = strdup("Second elemenet on the queue");
   //     char * temp3 = strdup("Third elemenet on the queue");
   //     char * temp4 = strdup("Fourth duck");
   //     char * temp5 = strdup("Fifth wild duck");
   //     char * temp6 = strdup("Sixth six six the number");
   //     char * temp7 = strdup("Seven the lucky number");
   //     char * temp8 = strdup("Eight wonderlands in the world");
   //     char * temp9 = strdup("Nine the number before ten");
   //
   //     clientChannel.addJobToTheQueue(temp1);
   //     clientChannel.addJobToTheQueue(temp2);
   //     std::cout << "Going to ssssleep zzZZzZzZzZzZZzZzZZzzZzZzZzZ" << '\n';
   //     omrthread_sleep(1500);
   //     clientChannel.addJobToTheQueue(temp3);
   //     clientChannel.addJobToTheQueue(temp4);
   //     omrthread_sleep(700);
   //     clientChannel.addJobToTheQueue(temp5);
   //     clientChannel.addJobToTheQueue(temp6);
   //     clientChannel.addJobToTheQueue(temp7);
   //     omrthread_sleep(1000);
   //     clientChannel.addJobToTheQueue(temp8);
   //     clientChannel.addJobToTheQueue(temp9);
   //
   //     std::cout << "before jobs done..." << '\n';
   //
   //     // Signal that there are no more jobs to be added to the queue
   //     clientChannel.signalNoJobsLeft();
   //
   //     while(!clientChannel.isShutdownComplete())
   //        {
   //        std::cout << "Waiting for SHUTDOWN_COMPLETE signal from thread..." << '\n';
   //        clientChannel.waitForMonitor();
   //        }
   //
   //     std::cout << "AFTER AFTER AFTER" << '\n';
   //     omrthread_sleep(1500);
   //
   //     cout << "Step 1: initialize JIT\n";
   //     bool initialized = initializeJit();
   //     if (!initialized)
   //        {
   //        cerr << "FAIL: could not initialize JIT\n";
   //        exit(-1);
   //        }
   //
   //     cout << "Step 2: define type dictionary\n";
   //     TR::TypeDictionary types;
   //     // Create a recorder so we can directly control the file for this particular test
   //     TR::JitBuilderRecorderTextFile recorder(NULL, "simple.out");
   //
   //     cout << "Step 3: compile method builder\n";
   //     SimpleMethod method(&types, &recorder);
   //
   //     //TODO Hack to be able to turn compiling off a global level
   //     jitBuilderShouldCompile = false;
   //
   //     uint8_t *entry = 0; // uint8_t ** , address = &entry
   //     int32_t rc = compileMethodBuilder(&method, &entry); // run in client thread
   //
   //   // With the ClientContext we can set deadlines for a particular response
   //   ClientContext context;
   //   ClientMessage fileString; // SEND
   //   ServerResponse returnedBytes; // receive
   //
   //   std::string clientStr = "This is a string that will be substituted by a file\n";
   //   fileString.set_file(clientStr);
   //   fileString.set_version(34);
   //
   //   Status status = stub_->SendOutFile(&context, fileString, &returnedBytes);
   //
   //   std::cout << "Client received the following message from the server:" << '\n';
   //   std::cout << "Byte stream: " << returnedBytes.bytestream() << '\n';
   //   std::cout << "Size: " << returnedBytes.size() << '\n';
   //
   //   if(status.ok())
   //      {
   //        std::cout << "Client finished successfully." << '\n';
   //      }
   //   else
   //      {
   //        std::cout << status.error_code() << ": " << status.error_message()
   //                  << std::endl;
   //        std::cout << "RPC failed" << '\n';
   //      }
   //   }

   // Implement SendOutFiles bidirectional streaming for client
   void SendMessage()
      {
      ClientContext context;
      system_clock::time_point deadline = std::chrono::system_clock::now() +
         std::chrono::milliseconds(5500); // 5 seconds
      // Deadline for all the responses to reach the client
      context.set_deadline(deadline);

      std::shared_ptr<ClientReaderWriter<ClientMessage, ServerResponse>> stream(stub_->SendMessage(&context));

      std::thread writer([stream]() {
        std::vector<ClientMessage> files{
          MakeFileString("This is the FIRST message.", 3241),
          MakeFileString("Looks like I am the SECOND message.", 9991),
          MakeFileString("Wow, another one!! I'm the THIRD message.", 33),
          MakeFileString("This is it!! I am the LAST and FOURTH message.", 385)};
        for (const ClientMessage& ff : files) {
          std::cout << "Sending file: " << ff.file() << std::endl;
          std::cout << "With version: " << ff.version() << std::endl;

          stream->Write(ff);
          std::this_thread::sleep_for (std::chrono::milliseconds(500));
          }
        stream->WritesDone();
      });

     ServerResponse retBytes;
     while (stream->Read(&retBytes))
        {
        std::cout << "Client received: " << retBytes.bytestream()
        << ", with size: " << retBytes.size() << '\n';
        }

     writer.join();
     Status status = stream->Finish();

     if (!status.ok())
       {
       std::cout << "OMR SendOutFiles rpc failed." << std::endl;
       std::cout << status.error_message() << '\n';
       std::cout << "Error details: " << status.error_details() << '\n';
       std::cout << "Status error code: " << status.error_code() << '\n';
       }
     }

   private:
      std::unique_ptr<ImperiumRPC::Stub> stub_;
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  // OMRClient client(grpc::CreateChannel(
  //     "localhost:50055", grpc::InsecureChannelCredentials()));

  // OMR::Imperium::ClientChannel client(grpc::CreateChannel(
  //     "localhost:50055", grpc::InsecureChannelCredentials()));

  char * fileName = strdup("simple.out");
  ClientChannel client;
  client.initClient("192.168.0.20:50055"); // Maybe make it private and call it in the constructor?
  client.generateIL(fileName);


  uint8_t *entry = 0;
  char * fileNames[] = {fileName, strdup("simple3.out")};
  // Param 1: File names as array of char *
  // Param 2: Number of file names
  std::vector<std::string> files = ClientChannel::readFilesAsString(fileNames, 2);
  //
  ClientMessage m = client.constructMessage(files.at(0), reinterpret_cast<uint64_t>(&entry));
  ClientMessage m1 = client.constructMessage(files.at(1), reinterpret_cast<uint64_t>(&entry));
  ClientMessage m2 = client.constructMessage(files.at(0), reinterpret_cast<uint64_t>(&entry));

  // Populate queue with .out files produced by this same method
  client.addMessageToTheQueue(m);
  omrthread_sleep(400);
  client.addMessageToTheQueue(m1);
  client.addMessageToTheQueue(m2);
  omrthread_sleep(100);
  client.addMessageToTheQueue(m1);
  client.addMessageToTheQueue(m);
  client.addMessageToTheQueue(m2);
  omrthread_sleep(500);
  client.signalNoJobsLeft();

  client.waitForThreadsCompletion();
  std::cout << "ABOUT TO CALL CLIENT DESCRUCTOR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << '\n';
  return 0;
}
