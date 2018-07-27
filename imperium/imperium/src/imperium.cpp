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

 #include "Jit.hpp"
 #include "jitbuilder/env/FrontEnd.hpp"
 #include "ilgen/TypeDictionary.hpp"
 #include "ilgen/JitBuilderRecorderTextFile.hpp"
 #include "ilgen/JitBuilderReplayTextFile.hpp"
 #include "ilgen/MethodBuilderReplay.hpp"
 #include "runtime/CodeCacheManager.hpp"
 #include "imperium/imperium.hpp"

 #include <sys/mman.h>

 #include <thread>

 using namespace OMR::Imperium;
 extern bool jitBuilderShouldCompile;

   /****************
   * ClientChannel *
   ****************/

  /*
   * Initializes OMR thread library
   * Attaches self (main) thread
   * And initializes the monitor
   */
  ClientChannel::ClientChannel()
     {

      attachSelf();

      jitBuilderShouldCompile = false;

      _writerStatus = ThreadStatus::INITIALIZATION;
      _readerStatus = ThreadStatus::INITIALIZATION;
      std::cout << "Initializing Monitor..." << '\n';

      if(J9THREAD_SUCCESS != omrthread_monitor_init(&_threadMonitor, 0))
         std::cout << "ERROR INITIALIZING monitor" << '\n';

      if(J9THREAD_SUCCESS != omrthread_monitor_init(&_queueMonitor, 0))
        std::cout << "ERROR INITIALIZING monitor" << '\n';
     }

  ClientChannel::~ClientChannel()
     {
        std::cout << " *** ClientChannel destructor called!!" << '\n';
     }

  void ClientChannel::SendMessage()
     {
     std::cout << "Calling SendMessage from ClientChannel..." << '\n';

     // create code cache and send initial to server (message: init code cache)
     _stream = _stub->SendMessage(&_context);

     // ClientMessage --> CompileRequest
     // ServerReponse --> CompileComplete

     // Call createWriterThread which creates a thread that writes
     // to the server based on the queue
     // It will handle sending off .out files to the server
     createWriterThread();

     // Call createReaderThread which creates a thread that waits
     // and listens for messages from the server
     createReaderThread();
     }

  void ClientChannel::requestCompile(char * fileName, uint8_t ** entry, TR::MethodBuilder *mb)
     {
       int32_t rc = compileMethodBuilder(mb, entry);

       std::string fileString;
       std::ifstream _file(fileName);
       std::string line;

       if(_file.is_open())
          {
           while(getline(_file,line))
              {
              fileString += line + '\n';
              }
           }
       else
          {
           std::cerr << "Error while trying to open: " << fileName << '\n';
          }

      ClientMessage m = constructMessage(fileString, reinterpret_cast<uint64_t>(entry));
      addMessageToTheQueue(m);
      std::remove(fileName);
     }

  void ClientChannel::shutdown()
  {
    signalNoJobsLeft();
    waitForThreadsCompletion();

    if(J9THREAD_SUCCESS != omrthread_monitor_destroy(_threadMonitor))
       std::cout << "ERROR WHILE destroying monitor" << '\n';
    if(J9THREAD_SUCCESS != omrthread_monitor_destroy(_queueMonitor))
      std::cout << "ERROR WHILE destroying monitor" << '\n';

    omrthread_detach(omrthread_self());
    std::cout << " *** ClientChannel shutdown COMPLETE" << '\n';
  }

  omrthread_t ClientChannel::attachSelf()
     {
       // Attach self thread
       omrthread_t thisThread;
       omrthread_attach_ex(&thisThread,
                           J9THREAD_ATTR_DEFAULT /* default attr */);
       return thisThread;
     }

  void ClientChannel::createReaderThread()
     {
       omrthread_entrypoint_t entpoint = readerThread;
       omrthread_t newThread;

       intptr_t ret1 = omrthread_create(&newThread, 0, J9THREAD_PRIORITY_MAX, 0, entpoint, (void *) this);
     }

  void ClientChannel::createWriterThread()
     {
       omrthread_entrypoint_t entpoint = writerThread;
       omrthread_t newThread;

       intptr_t ret1 = omrthread_create(&newThread, 0, J9THREAD_PRIORITY_MAX, 0, entpoint, (void *) this);
     }

  // Signal that there are no more jobs to be added to the queue
  void ClientChannel::signalNoJobsLeft()
     {

     // Must grab both monitors before sending the shutdown signal
     if(J9THREAD_SUCCESS != omrthread_monitor_enter(_threadMonitor))
        std::cout << "NO_JOBS_LEFT: ERROR WHILE ENTERING" << '\n';
     if(J9THREAD_SUCCESS != omrthread_monitor_enter(_queueMonitor))
       std::cout << "NO_JOBS_LEFT: ERROR WHILE ENTERING" << '\n';

     if (_writerStatus != ThreadStatus::SHUTDOWN_COMPLETE) {
       _writerStatus = ThreadStatus::SHUTDOWN_REQUESTED;
     }
     if (_readerStatus != ThreadStatus::SHUTDOWN_COMPLETE) {
       _readerStatus = ThreadStatus::SHUTDOWN_REQUESTED;
     }
     std::cout << "\n\n*!*!*!*!*!*!*! NO JOBS LEFT *!*!*!*!*!*!*!" << "\n\n";

     if(J9THREAD_SUCCESS != omrthread_monitor_notify_all(_threadMonitor))
        std::cout << "NO_JOBS_LEFT: ERROR WHILE NOTIFYING ALL" << '\n';
     if(J9THREAD_SUCCESS != omrthread_monitor_exit(_threadMonitor))
       std::cout << "NO_JOBS_LEFT: ERROR WHILE EXITING" << '\n';

     // The writer thread could be waiting for more jobs to be added to the queue
     if(J9THREAD_SUCCESS != omrthread_monitor_notify_all(_queueMonitor))
       std::cout << "NO_JOBS_LEFT: ERROR WHILE NOTIFYING ALL" << '\n';
     if(J9THREAD_SUCCESS != omrthread_monitor_exit(_queueMonitor))
       std::cout << "NO_JOBS_LEFT: ERROR WHILE EXITING" << '\n';
     }

  void ClientChannel::waitForThreadsCompletion()
     {

     if(J9THREAD_SUCCESS != omrthread_monitor_enter(_threadMonitor))
        std::cout << "NO_JOBS_LEFT: ERROR WHILE ENTERING" << '\n';

     while(_writerStatus != ThreadStatus::SHUTDOWN_COMPLETE || _readerStatus != ThreadStatus::SHUTDOWN_COMPLETE) {
        omrthread_monitor_wait(_threadMonitor);
     }

     if(J9THREAD_SUCCESS != omrthread_monitor_exit(_threadMonitor))
       std::cout << "NO_JOBS_LEFT: ERROR WHILE EXITING" << '\n';

     std::cout << "\n\n*!*!*!*!*!*!*! FINISHED WAITING FOR THREAD COMPLETION *!*!*!*!*!*!*!" << "\n\n";
     }

  int ClientChannel::readerThread(void * data)
     {
       ClientChannel *s = (ClientChannel *) data;
       s->handleRead();

       return 1;
     }

  int ClientChannel::writerThread(void * data)
    {
     ClientChannel *s = (ClientChannel *) data;
     s->handleWrite();

     return 1;
     }

  ClientMessage ClientChannel::constructMessage(std::string file, uint64_t address)
     {
       ClientMessage clientMessage;
       clientMessage.set_file(file);
       clientMessage.set_address(address);
       // std::cout << "Constructing message with entry point address: " << address << '\n';

       return clientMessage;
     }

  bool ClientChannel::addMessageToTheQueue(ClientMessage message)
     {
      if(J9THREAD_SUCCESS != omrthread_monitor_enter(_queueMonitor))
         std::cout << "ERROR WHILE ENTERING MONITOR on addJobToTheQueue" << '\n';

      _queueJobs.push(message);
      std::cout << "++ Added message: " << message.file() << " to the queue." << '\n';
      std::cout << "++ There are now " << _queueJobs.size() << " jobs in the queue." << '\n';

      // Notify that there are new jobs to be processed in the queue
      omrthread_monitor_notify_all(_queueMonitor);

      if(J9THREAD_SUCCESS != omrthread_monitor_exit(_queueMonitor))
         std::cout << "ERROR WHILE EXITING MONITOR on addJobToTheQueue" << '\n';

      return true;
     }

  bool ClientChannel::isQueueEmpty()
     {
      return _queueJobs.empty();
     }

  ClientMessage ClientChannel::getNextMessage()
     {
      if(isQueueEmpty())
         {
          TR_ASSERT_FATAL(0, "Queue is empty, there are no messages.");
         }
      ClientMessage nextMessage = _queueJobs.front();
      _queueJobs.pop();
      std::cout << "-- About to process job from the queue..." << '\n';
      std::cout << "-- There are now " << _queueJobs.size() << " jobs in the queue." << '\n';
      return nextMessage;
     }

  void ClientChannel::handleRead()
     {
       std::cout << "******************************** Calling handle read..." << '\n';
       ServerResponse retBytes;
            // TODO: we don't need to read results if we're shutting down.  Right
            // now it will block until the stream is closed.
            static int count = 0;
            while(_stream->Read(&retBytes))
              {
              // print out address received as well
              std::cout << "Client received: " << retBytes.instructions() << ", with size: " << retBytes.size() << ". Count: " << (++count) << '\n';
              void * virtualCodeAddress = mmap(
                                        NULL,
                                        retBytes.size(),
                                        PROT_READ | PROT_WRITE | PROT_EXEC,
                                        MAP_ANONYMOUS | MAP_PRIVATE,
                                        0,
                                        0);
              memcpy(virtualCodeAddress, (const void *)retBytes.instructions().c_str(), retBytes.size());
              typedef int32_t (SimpleMethodFct)(int32_t);
              SimpleMethodFct *incr = (SimpleMethodFct *) virtualCodeAddress;

                int32_t v;
                v=3; std::cout << "incr(" << v << ") == " << incr(v) << "\n";
                v=6; std::cout << "incr(" << v << ") == " << incr(v) << "\n";
                v=12; std::cout << "incr(" << v << ") == " << incr(v) << "\n";
                v=-19; std::cout << "incr(" << v << ") == " << incr(v) << "\n";
              }

       // Check if status is OK
       Status status = _stream->Finish();

       if (!status.ok())
         {
         std::cout << "OMR SendOutFiles rpc failed." << std::endl;
         std::cout << status.error_message() << '\n';

         if(status.error_code() == grpc::UNKNOWN)
            {
              std::cout << "UNKNOWN error at server side!!!!!" << '\n';
              std::cout << "Server side application throws an exception (or does something other than returning a Status code to terminate an RPC)" << '\n';
            }

         std::cout << "Error details: " << status.error_details() << '\n';
         std::cout << "Status error code: " << status.error_code() << '\n';
         }

         omrthread_monitor_enter(_threadMonitor);
         std::cout << "readerStatus changed to SHUTDOWN_COMPLETE!!!!" << '\n';
         _readerStatus = ThreadStatus::SHUTDOWN_COMPLETE;
         omrthread_monitor_notify_all(_threadMonitor);
         omrthread_exit(_threadMonitor);
     }

  // Called by thread
  void ClientChannel::handleWrite()
     {

       omrthread_monitor_enter(_queueMonitor);
       while(_writerStatus != ThreadStatus::SHUTDOWN_REQUESTED || !isQueueEmpty())
          {
          if (!isQueueEmpty())
            {
            ClientMessage message = getNextMessage();

            // exit the monitor so more items can be added to the queue
            omrthread_monitor_exit(_queueMonitor);

            _stream->Write(message);
            std::cout << "JUST WROTE SOMETHING TO THE SERVER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << '\n';

            omrthread_monitor_enter(_queueMonitor);
            } else
            {
              std::cout << "Waiting inside ClientChannel::handleWrite." << '\n';
              omrthread_monitor_wait(_queueMonitor);
            }
          }

       omrthread_monitor_exit(_queueMonitor);

       _stream->WritesDone();

       omrthread_monitor_enter(_threadMonitor);
       std::cout << "writeStatus changed to SHUTDOWN_COMPLETE!!!!" << '\n';
       _writerStatus = ThreadStatus::SHUTDOWN_COMPLETE;
       omrthread_monitor_notify_all(_threadMonitor);
       omrthread_exit(_threadMonitor);
       }

     bool ClientChannel::initClient(const char * port)
       {
       if(_stub != NULL)
          {
            TR_ASSERT_FATAL(0, "Client was already initialized. Quiting...");
          }
       std::shared_ptr<Channel> channel = grpc::CreateChannel(
              port, grpc::InsecureChannelCredentials());

       _stub = ImperiumRPC::NewStub(channel);

       SendMessage();
       }

     /****************
     * ServerChannel *
     ****************/

     ServerChannel::ServerChannel()
        {
          omrthread_t thisThread;
          omrthread_attach_ex(&thisThread,
                              J9THREAD_ATTR_DEFAULT /* default attr */);

          if(J9THREAD_SUCCESS != omrthread_monitor_init(&_compileMonitor, 0))
             std::cout << "ERROR INITIALIZING monitor" << '\n';
        }

     ServerChannel::~ServerChannel()
        {
          if(J9THREAD_SUCCESS != omrthread_monitor_destroy(_compileMonitor))
             std::cout << "ERROR WHILE destroying monitor" << '\n';

          omrthread_detach(omrthread_self());
        }

     Status ServerChannel::SendMessage(ServerContext* context,
                      ServerReaderWriter<ServerResponse, ClientMessage>* stream)
        {

          ClientMessage clientMessage;
          std::cout << "Server waiting for message from client..." << '\n';

          omrthread_t thisThread;
          omrthread_attach_ex(&thisThread,
                              J9THREAD_ATTR_DEFAULT /* default attr */);

          while (stream->Read(&clientMessage))
             {
                std::cout << "Server received file: " << clientMessage.file() << '\n';
                std::cout << "Server entry point address: " << clientMessage.address() << '\n';

                omrthread_monitor_enter(_compileMonitor);
                TR::JitBuilderReplayTextFile replay(clientMessage.file());
                TR::JitBuilderRecorderTextFile recorder(NULL, "simple2.out");

                std::cout << "################################################# Thread ID: " << thisThread << '\n';

                TR::TypeDictionary types;
                uint8_t *entry = 0;

                std::cout << "Step 1: verify output file\n";
                TR::MethodBuilderReplay mb(&types, &replay, &recorder); // Process Constructor

                //************************************************************
                std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

                // Only one thread can compile at a time.

                int32_t rc = compileMethodBuilder(&mb, &entry); // Process buildIL
                omrthread_monitor_exit(_compileMonitor);

                // TODO
                // send entry back to client, calculate size and send back to client
                // copy bytes between entry point and code cache warm alloc
                // void * or uint8_t * should give size in bytes

                // Java JITaaS commit: Option to allocate code cache at specified address

                // TODO: all the below stuff occurs on the server side
                auto fe = JitBuilder::FrontEnd::instance();
                auto codeCacheManager = fe->codeCacheManager();
                auto codeCache = codeCacheManager.getFirstCodeCache();
                void * mem = codeCache->getWarmCodeAlloc();

                uint64_t sizeCode = (uint64_t) mem - (uint64_t)entry;

                //
                // // uint8_t *entry = 0; // uint8_t ** , address = &entry
                //
                std::cout << " *********************** " << (mem) << " *********************** " << '\n';
                std::cout << " *********************** " << (void*)(entry) << " *********************** " << '\n';
                std::cout << " *********************** " << sizeCode << " *********************** " << '\n';
                // (uint8_t *) mem = 0x0000000103800040 ""



                std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

                std::cout << "Duration for compileMethodBuilder in server: " << duration << " microseconds." << '\n';
                //************************************************************
                // TODO: remove function calls and put in the client side
                //       need to send back compiled code, then run that code on client
                typedef int32_t (SimpleMethodFunction)(int32_t);
                SimpleMethodFunction *increment = (SimpleMethodFunction *) entry;

                int32_t v;
                v=0; std::cout << "increment(" << v << ") == " << increment(v) << "\n";
                v=1; std::cout << "increment(" << v << ") == " << increment(v) << "\n";
                v=10; std::cout << "increment(" << v << ") == " << increment(v) << "\n";
                v=-15; std::cout << "increment(" << v << ") == " << increment(v) << "\n";

                //******************************************************************

                ServerResponse e;
                e.set_instructions((char*)entry);
                e.set_size(sizeCode);
                // TODO: set address (defined in proto) to entry address sent by client

                stream->Write(e);
                // Sleeps for 1 second
                // std::this_thread::sleep_for (std::chrono::seconds(1));
             }

         omrthread_detach(thisThread);

         if (context->IsCancelled()) {
           return Status(StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
         }
         return Status::OK;
        }

     bool ServerChannel::RunServer(const char * port)
     {
        // TODO: wrap init Jit in helper func (used in both client and server currently)

        std::string server_address(port); // "localhost:50055"

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;
        server->Wait();
     }
