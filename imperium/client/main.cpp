/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <queue>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/JitBuilderRecorderTextFile.hpp"
#include "ilgen/JitBuilderReplayTextFile.hpp"
#include "ilgen/MethodBuilderReplay.hpp"
#include "Simple.hpp"

#include "omrthread.h"
// #include "../../../compiler/infra/OMRMonitor.hpp"

using std::cout;
using std::cerr;
extern bool jitBuilderShouldCompile;

class ServerCall {

  public:

  enum MonitorStatus {
    SHUTING_DOWN,
    SHUTDOWN_COMPLETE,
    INITIALIZING,
    INITIALIZING_COMPLETE,
    JOBS_DONE
  };

  /*
   * Initializes OMR thread library
   * Attaches self (main) thread
   * And initializes the monitor
   */
  ServerCall()
     {
      if(J9THREAD_SUCCESS != omrthread_init_library()){
         std::cout << "ERROR INITIALIZING thread library" << '\n';
}
      attachSelf();

      _monitorStatus = MonitorStatus::INITIALIZING;

      std::cout << "Initializing Monitors..." << '\n';

      if(J9THREAD_SUCCESS != omrthread_monitor_init(&monitor, 0))
         std::cout << "ERROR INITIALIZING monitor" << '\n';

      std::cout << "SLEEPING zzZZzZzZzZzZZzZzZZzzZzZzZzZzzZzZ " << '\n';
      omrthread_sleep(2000);

      _monitorStatus = MonitorStatus::INITIALIZING_COMPLETE;
     }

  ~ServerCall()
     {}

  void destroy() {
    if(J9THREAD_SUCCESS != omrthread_monitor_destroy(monitor))
       std::cout << "ERROR WHILE destroying monitor" << '\n';
  }

  omrthread_t attachSelf()
     {
       // Attach self thread
       omrthread_t thisThread;
       omrthread_attach_ex(&thisThread,
                           J9THREAD_ATTR_DEFAULT /* default attr */);
       return thisThread;
     }

  void createWorkThread()
     {
       omrthread_entrypoint_t entpoint = helperThread;
       omrthread_t newThread;

       intptr_t ret1 = omrthread_create(&newThread, 0, J9THREAD_PRIORITY_MAX, 0, entpoint, (void *)this);
     }

  // Signal that there are no more jobs to be added to the queue
  void setJobDone()
     {
     _monitorStatus = MonitorStatus::JOBS_DONE;
     }

  bool isShutdownComplete()
     {
     return _monitorStatus == MonitorStatus::SHUTDOWN_COMPLETE;
     }

  static int helperThread(void *data)
    {
     ServerCall *s = (ServerCall *)data;

     std::cout << " Compiler: initializing\n";
     s->handleCall();

     return 1;
     }

  MonitorStatus getMonitorStatus()
    {
    return _monitorStatus;
    }

  void waitForMonitor()
     {
     if(0 != omrthread_monitor_enter(monitor))
        std::cout << "ERROR WHILE ENTERING on wait monitor" << '\n';

     std::cout << "WAITING ON MONITOR..." << '\n';

     intptr_t tt = omrthread_monitor_wait(monitor);
     if(J9THREAD_SUCCESS != tt)
        std::cout << "ERROR WHILE WAITING ON monitor, error code: " << tt << '\n';

     std::cout << "MONITOR RELEASED. READY TO GO!" << '\n';

     if(0 != omrthread_monitor_exit(monitor))
        std::cout << "ERROR WHILE Exiting on wait monitor" << '\n';
     }

  bool addJobToTheQueue(char * job)
     {
      if(J9THREAD_SUCCESS != omrthread_monitor_enter(monitor))
         std::cout << "ERROR WHILE ENTERING MONITOR on addJobToTheQueue" << '\n';

      _queueJobs.push(job);
      std::cout << "++ Added job: " << job << " to the queue." << '\n';
      std::cout << "++ There are now " << _queueJobs.size() << " jobs in the queue." << '\n';
      // Notify that there are new jobs to be processed in the queue
      omrthread_monitor_notify_all(monitor);

      if(J9THREAD_SUCCESS != omrthread_monitor_exit(monitor))
         std::cout << "ERROR WHILE EXITING MONITOR on addJobToTheQueue" << '\n';
      return true;
     }

  bool isQueueEmpty()
     {
      return _queueJobs.empty();
     }

  char * getNextJob()
     {
      if(isQueueEmpty())
         {
          std::cout << "ERROR: Tried to retrieve from empty queue. Returning NULL" << '\n';
          return NULL;
         }
      char * nextJob = _queueJobs.front();
      _queueJobs.pop();
      std::cout << "-- About to process job: " << nextJob << " from the queue..." << '\n';
      std::cout << "-- There are now " << _queueJobs.size() << " jobs in the queue." << '\n';
      return nextJob;
     }

  // Called by thread
  void handleCall()
     {
       omrthread_monitor_enter(monitor);
       std::cout << "Entering monitor at handleCall..." << '\n';

       std::cout << "Inside test entry Point ************************************" << '\n';
       std::cout << "Inside test entry Point $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$" << '\n';
       omrthread_sleep(1000);
       //for (size_t i = 0; i < 100000000000000; i++) {int u = i + 5 * 100 / 5;}
       std::cout << "Inside test entry Point ####################################" << '\n';
       //for (size_t c = 0; c < 100000000000000; c++) {int yy = c + 5 * 100 / 5;}
       std::cout << "Inside test entry Point *********^^^^^^^^^&&&&&&&&&&(()()()())" << '\n';

       while(_monitorStatus != MonitorStatus::JOBS_DONE)
          {
          // Sleep until queue is populated
          waitForMonitor();

          while(!isQueueEmpty())
             {
              omrthread_monitor_exit(monitor);
              char * job = getNextJob();
              omrthread_sleep(500);
              omrthread_monitor_enter(monitor);
             }
          }

       std::cout << "Changing status to SHUTDOWN_COMPLETE!!!!" << '\n';
       _monitorStatus = MonitorStatus::SHUTDOWN_COMPLETE;
       omrthread_monitor_notify_all(monitor);

       if(J9THREAD_SUCCESS != omrthread_monitor_exit(monitor))
          std::cout << "ERROR WHILE EXITING MONITOR on handleCall" << '\n';
     }

  private:
    omrthread_monitor_t monitor;
    MonitorStatus _monitorStatus;
    std::queue<char *> _queueJobs;
};

int
main1(int argc, char *argv[])
   {
   cout << "Step 1: initialize JIT\n";
   bool initialized = initializeJit();
   if (!initialized)
      {
      cerr << "FAIL: could not initialize JIT\n";
      exit(-1);
      }

   cout << "Step 2: define type dictionary\n";
   TR::TypeDictionary types;
   // Create a recorder so we can directly control the file for this particular test
   TR::JitBuilderRecorderTextFile recorder(NULL, "simple.out");

   cout << "Step 3: compile method builder\n";
   SimpleMethod method(&types, &recorder);

   //TODO Hack to be able to turn compiling off a global level
   jitBuilderShouldCompile = false;

   uint8_t *entry = 0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   // If compiling test the compiled code
   if (jitBuilderShouldCompile)
      {
      cout << "Step 4: invoke compiled code and print results\n";
      // typedef int32_t (SimpleMethodFunction)(int32_t);
      typedef int32_t (SimpleMethodFunction)(int32_t);
      SimpleMethodFunction *increment = (SimpleMethodFunction *) entry;

      // cout << "Returning: " << increment() << "\n";

      // *****************************************************************
      // MOST SIMPLE SIMPLE.cpp
      int32_t v;
      v=0; cout << "increment(" << v << ") == " << increment(v) << "\n";
      v=1; cout << "increment(" << v << ") == " << increment(v) << "\n";
      v=10; cout << "increment(" << v << ") == " << increment(v) << "\n";
      v=-15; cout << "increment(" << v << ") == " << increment(v) << "\n";


      // *****************************************************************
      // COMPLICATED SIMPLE.cpp
      // int32_t v, u;
      // v=0; u=1;  cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";
      // v=1; u=5;  cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";
      // v=10; u=7; cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";
      // v=-15; u=33; cout << "increment(" << v << "+" << u << ") == " << increment(v,u) << "\n";
      }
   //If not compiling verify the output file....
   else
      {
      cout << "Step 5: Replay\n";

      // *********************************************************************************
      char * temp1 = strdup("First elemenet on the queue");
      char * temp2 = strdup("Second elemenet on the queue");
      char * temp3 = strdup("Third elemenet on the queue");
      char * temp4 = strdup("Fourth duck");
      char * temp5 = strdup("Fifth wild duck");
      char * temp6 = strdup("Sixth six six the number");
      char * temp7 = strdup("Seven the lucky number");
      char * temp8 = strdup("Eight wonderlands in the world");
      char * temp9 = strdup("Nine the number before ten");

      ServerCall serverCall;
      serverCall.createWorkThread();

      serverCall.addJobToTheQueue(temp1);
      serverCall.addJobToTheQueue(temp2);
      std::cout << "Going to ssssleep zzZZzZzZzZzZZzZzZZzzZzZzZzZ" << '\n';
      omrthread_sleep(1500);
      serverCall.addJobToTheQueue(temp3);
      serverCall.addJobToTheQueue(temp4);
      omrthread_sleep(700);
      serverCall.addJobToTheQueue(temp5);
      serverCall.addJobToTheQueue(temp6);
      serverCall.addJobToTheQueue(temp7);
      omrthread_sleep(1000);
      serverCall.addJobToTheQueue(temp8);
      serverCall.addJobToTheQueue(temp9);

      std::cout << "before jobs done..." << '\n';

      // Signal that there are no more jobs to be added to the queue
      serverCall.setJobDone();

      while(!serverCall.isShutdownComplete())
         {
         std::cout << "Waiting for SHUTDOWN_COMPLETE signal from thread..." << '\n';
         serverCall.waitForMonitor();
         }

      std::cout << "AFTER AFTER AFTER" << '\n';
      omrthread_sleep(1500);

      // *********************************************************************************

      jitBuilderShouldCompile = true;
      TR::JitBuilderReplayTextFile replay("simple.out");
      TR::JitBuilderRecorderTextFile recorder2(NULL, "simple2.out");

      TR::TypeDictionary types2;
      uint8_t *entry2 = 0;

      cout << "Step 6: verify output file\n";
      TR::MethodBuilderReplay mb(&types2, &replay, &recorder2); // Process Constructor
      int32_t rc = compileMethodBuilder(&mb, &entry2); // Process buildIL

      omrthread_detach(omrthread_self());
      omrthread_shutdown_library();
      }

   cout << "Step 7: shutdown JIT\n";
   shutdownJit();
   }



SimpleMethod::SimpleMethod(TR::TypeDictionary *d, TR::JitBuilderRecorder *recorder)
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
SimpleMethod::buildIL()
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
