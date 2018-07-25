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

 #include <thread>

 using namespace OMR::Imperium;
 extern bool jitBuilderShouldCompile;

 class SimpleMethod : public TR::MethodBuilder
    {
    public:
       SimpleMethod(TR::TypeDictionary *d, TR::JitBuilderRecorder *recorder)
       : MethodBuilder(d, recorder)
       {
       DefineLine(LINETOSTR(__LINE__));
       DefineFile(__FILE__);
       DefineName("increment");
       DefineParameter("value", Int32);
       DefineReturnType(Int32);

       // DefineLine(LINETOSTR(__LINE__));
       // // cout << LINETOSTR(__LINE__) << " // CHECKING VALUE OF DEFINELINE \n";
       // DefineFile(__FILE__);
       //
       // DefineName("increment");
       //
       // // COMPLICATED SIMPLE.cpp
       // DefineParameter("value1", Int32);
       // DefineParameter("value2", Int32);
       // DefineReturnType(Int32);
       }

    bool
    buildIL()
       {
       // std::cout << "SimpleMethod::buildIL() running!\n";

       // ORIGINAL SIMPLE.cpp
       Return(
          Add(
             Load("value"),
             ConstInt32(1)));

       // // COMPLICATED SIMPLE.cpp
       // Return(
       //    Add(
       //      Add(
       //        Load("value1"),
       //        ConstInt32(17)
       //      ),
       //      Add(
       //        Load("value2"),
       //        ConstInt32(14))));
       return true;
       }
 };

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
      omrthread_init_library();

      attachSelf();

      _writerStatus = ThreadStatus::INITIALIZATION;
      _readerStatus = ThreadStatus::INITIALIZATION;
      std::cout << "Initializing Monitor..." << '\n';

      if(J9THREAD_SUCCESS != omrthread_monitor_init(&_monitor, 0))
         std::cout << "ERROR INITIALIZING monitor" << '\n';
     }

  ClientChannel::~ClientChannel()
     {
        std::cout << " *** ClientChannel destructor called!!" << '\n';
        shutdown();
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

  void ClientChannel::generateIL(const char * fileName)
     {
     std::cout << "Step 1: initialize JIT\n";
     bool initialized = initializeJit();
     if (!initialized)
        {
        std::cerr << "FAIL: could not initialize JIT\n";
        exit(-1);
        }

     std::cout << "Step 2: define type dictionary\n";
     TR::TypeDictionary types;
     // Create a recorder so we can directly control the file for this particular test
     TR::JitBuilderRecorderTextFile recorder(NULL, fileName);

     std::cout << "Step 3: compile method builder\n";
     SimpleMethod method(&types, &recorder);

     //TODO Hack to be able to turn compiling off a global level
     jitBuilderShouldCompile = false;

     auto fe = JitBuilder::FrontEnd::instance();
     auto codeCacheManager = fe->codeCacheManager();
     auto codeCache = codeCacheManager.getFirstCodeCache();
     void * mem = codeCache->getWarmCodeAlloc();

     uint8_t *entry = 0; // uint8_t ** , address = &entry

     std::cout << " *********************** " << (mem) << " *********************** " << '\n';

     // comp() is in OMRCompilation.hpp
     // warmAlloc = frontend.codecache.getWarmAlloc();
     // entry - warmAlloc = size of compiled code --> pass in ClientMessage

     std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
     int32_t rc = compileMethodBuilder(&method, &entry); // run in client thread
     std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
     auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

     std::cout << "Duration for compileMethodBuilder in client: " << duration << " microseconds." << '\n';
     }

  void ClientChannel::shutdown()
  {
    if(J9THREAD_SUCCESS != omrthread_monitor_destroy(_monitor))
       std::cout << "ERROR WHILE destroying monitor" << '\n';
    omrthread_detach(omrthread_self());
    omrthread_shutdown_library();
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
     if(J9THREAD_SUCCESS != omrthread_monitor_enter(_monitor))
        std::cout << "NO_JOBS_LEFT: ERROR WHILE ENTERING" << '\n';

     _writerStatus = ThreadStatus::NO_JOBS_LEFT; // at this point _readerStatus should be RUNNING
     std::cout << "\n\n*!*!*!*!*!*!*! NO JOBS LEFT *!*!*!*!*!*!*!" << "\n\n";

     if(J9THREAD_SUCCESS != omrthread_monitor_notify_all(_monitor))
        std::cout << "NO_JOBS_LEFT: ERROR WHILE NOTIFYING ALL" << '\n';

     if(J9THREAD_SUCCESS != omrthread_monitor_exit(_monitor))
       std::cout << "NO_JOBS_LEFT: ERROR WHILE EXITING" << '\n';

     }

  void ClientChannel::waitForThreadsCompletion()
     {
     while (_writerStatus != ThreadStatus::SHUTDOWN_COMPLETE || _readerStatus != ThreadStatus::SHUTDOWN_COMPLETE) {
     }

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

  void ClientChannel::waitForMonitor()
     {
     if(0 != omrthread_monitor_enter(_monitor))
        std::cout << "ERROR WHILE ENTERING on wait monitor" << '\n';

     // std::cout << "WAITING ON MONITOR..." << '\n';
     intptr_t tt = omrthread_monitor_wait(_monitor);

     if(J9THREAD_SUCCESS != tt)
        std::cout << "ERROR WHILE WAITING ON monitor, error code: " << tt << '\n';

     std::cout << "MONITOR RELEASED. READY TO GO!" << '\n';

     if(0 != omrthread_monitor_exit(_monitor))
        std::cout << "ERROR WHILE Exiting on wait monitor" << '\n';
     }

  std::vector<std::string> ClientChannel::readFilesAsString(char * fileNames [], int numOfFiles)
     {
      std::vector<std::string> filesString;

      for (size_t i = 0; i < numOfFiles; i++) {
        std::string fileString;
        std::ifstream _file(fileNames[i]);
        std::string line;
        // std::cout << "Reading " << fileNames[i]  << "...\n";
        if(_file.is_open())
           {
            // std::cout << fileNames[i] << " opened." << '\n';
            while(getline(_file,line))
               {
               fileString += line + '\n';
               }

            filesString.push_back(fileString);
            // std::cout << fileString << '\n';
            }
        else
           {
            std::cerr << "Error while trying to open: " << fileNames[i] << '\n';
           }
        }

        return filesString;
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
      if(J9THREAD_SUCCESS != omrthread_monitor_enter(_monitor))
         std::cout << "ERROR WHILE ENTERING MONITOR on addJobToTheQueue" << '\n';

      _queueJobs.push(message);
      std::cout << "++ Added message: " << message.file() << " to the queue." << '\n';
      std::cout << "++ There are now " << _queueJobs.size() << " jobs in the queue." << '\n';
      // Notify that there are new jobs to be processed in the queue
      omrthread_monitor_notify_all(_monitor);

      if(J9THREAD_SUCCESS != omrthread_monitor_exit(_monitor))
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
       while (_stream->Read(&retBytes))
          {
          std::cout << "Client received: " << retBytes.bytestream()
          << ", with size: " << retBytes.size() << '\n';
          // print out address received as well
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

         omrthread_monitor_enter(_monitor);
         std::cout << "Changing monitor status to SHUTDOWN_REQUESTED!!!!!!" << '\n';
         _readerStatus = ThreadStatus::SHUTDOWN_REQUESTED;
         omrthread_monitor_notify_all(_monitor);
         std::cout << "Monitor status changed to SHUTDOWN_REQUESTED!!!!" << '\n';
         if(J9THREAD_SUCCESS != omrthread_monitor_exit(_monitor)) {
            std::cout << "ERROR WHILE EXITING MONITOR on handleRead" << '\n';
         }

         _readerStatus = ThreadStatus::SHUTDOWN_COMPLETE;
     }

  // Called by thread
  void ClientChannel::handleWrite()
     {
       omrthread_monitor_enter(_monitor);
       std::cout << "Entering monitor at handleWrite..." << '\n';

       while(_writerStatus != ThreadStatus::NO_JOBS_LEFT)
          {
          // Sleep until queue is populated
          std::cout << "Waiting inside ClientChannel::handleWrite." << '\n';
          waitForMonitor();

          while(!isQueueEmpty())
            {
             omrthread_monitor_exit(_monitor);
             ClientMessage message = getNextMessage();
             _stream->Write(message);
             std::cout << "JUST WROTE SOMETHING TO THE SERVER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << '\n';
             omrthread_monitor_enter(_monitor);
            }
        }

       _stream->WritesDone();
       std::cout << "Changing status to SHUTDOWN_REQUESTED!!!!" << '\n';
       _writerStatus = ThreadStatus::SHUTDOWN_REQUESTED;
       omrthread_monitor_notify_all(_monitor);

       if(J9THREAD_SUCCESS != omrthread_monitor_exit(_monitor))
          std::cout << "ERROR WHILE EXITING MONITOR on handleWrite" << '\n';

       _writerStatus = ThreadStatus::SHUTDOWN_COMPLETE;
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

        }

     ServerChannel::~ServerChannel()
        {
          std::cout << " *** ServerChannel destructor called!!" << '\n';
          shutdownJit();
        }

     Status ServerChannel::SendMessage(ServerContext* context,
                      ServerReaderWriter<ServerResponse, ClientMessage>* stream)
        {
          std::string serverStr[6] = {"0000011111", "01010101", "11110000", "000111000", "1010101010101", "1100110011001100"};
          int count = 0;

          ClientMessage clientMessage;
          std::cout << "Server waiting for message from client..." << '\n';

          while (stream->Read(&clientMessage))
             {
                std::cout << "Server received file: " << clientMessage.file() << '\n';
                std::cout << "Server entry point address: " << clientMessage.address() << '\n';

                TR::JitBuilderReplayTextFile replay(clientMessage.file());
                TR::JitBuilderRecorderTextFile recorder(NULL, "simple2.out");

                TR::TypeDictionary types;
                uint8_t *entry = 0;

                std::cout << "Step 1: verify output file\n";
                TR::MethodBuilderReplay mb(&types, &replay, &recorder); // Process Constructor

                //************************************************************
                std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
                int32_t rc = compileMethodBuilder(&mb, &entry); // Process buildIL

                // TODO
                // send entry back to client, calculate size and send back to client
                // copy bytes between entry point and code cache warm alloc
                // void * or uint8_t * should give size in bytes

                // Java JITaaS commit: Option to allocate code cache at specified address

                // TODO: all the below stuff occurs on the server side
                // auto fe = JitBuilder::FrontEnd::instance();
                // auto codeCacheManager = fe->codeCacheManager();
                // auto codeCache = codeCacheManager.getFirstCodeCache();
                // void * mem = codeCache->getWarmCodeAlloc();
                //
                // // uint8_t *entry = 0; // uint8_t ** , address = &entry
                //
                // std::cout << " *********************** " << (mem) << " *********************** " << '\n';
                // // (uint8_t *) mem = 0x0000000103800040 ""

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

                // int32_t v, u;
                // v=0; u=1;  std::cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";
                // v=1; u=5;  std::cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";
                // v=10; u=7; std::cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";
                // v=-15; u=33; std::cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";

                //******************************************************************

                ServerResponse e;
                e.set_bytestream(serverStr[count]);
                e.set_size((count + 2) * 64);
                // TODO: set address (defined in proto) to entry address sent by client

                std::cout << "Sending to client: " << count << ": " << serverStr[count] << '\n';
                count++;

                stream->Write(e);
                // Sleeps for 1 second
                // std::this_thread::sleep_for (std::chrono::seconds(1));
             }

         if (context->IsCancelled()) {
           return Status(StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
         }
         return Status::OK;
        }

     bool ServerChannel::RunServer(const char * port)
     {
        // TODO: wrap init Jit in helper func (used in both client and server currently)
        bool initialized = initializeJit();
        if (!initialized)
        {
         std::cerr << "FAIL: could not initialize JIT\n";
         exit(-1);
        }
        else
        {
         std::cout << ">>> JIT INITIALIZED" << '\n';
        }

        std::string server_address(port); // "localhost:50055"
        // ServerChannel service = this;

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;
        server->Wait();
     }
