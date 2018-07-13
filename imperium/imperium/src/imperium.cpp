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

 #include "imperium/imperium.hpp"

  /*
   * Initializes OMR thread library
   * Attaches self (main) thread
   * And initializes the monitor
   */
  ServerChannel::ServerChannel()
     {
      omrthread_init_library();

      attachSelf();

      _monitorStatus = MonitorStatus::INITIALIZING;

      std::cout << "Initializing Monitors..." << '\n';

      if(J9THREAD_SUCCESS != omrthread_monitor_init(&_monitor, 0))
         std::cout << "ERROR INITIALIZING monitor" << '\n';

      std::cout << "SLEEPING zzZZzZzZzZzZZzZzZZzzZzZzZzZzzZzZ " << '\n';
      omrthread_sleep(2000);

      _monitorStatus = MonitorStatus::INITIALIZING_COMPLETE;
     }

  ServerChannel::~ServerChannel()
     {
        std::cout << " *** ServerChannel destructor called!!" << '\n';

        destroy();
        omrthread_detach(omrthread_self());
        omrthread_shutdown_library();
     }

  void ServerChannel::destroy() {
    if(J9THREAD_SUCCESS != omrthread_monitor_destroy(_monitor))
       std::cout << "ERROR WHILE destroying monitor" << '\n';
  }

  omrthread_t ServerChannel::attachSelf()
     {
       // Attach self thread
       omrthread_t thisThread;
       omrthread_attach_ex(&thisThread,
                           J9THREAD_ATTR_DEFAULT /* default attr */);
       return thisThread;
     }

  void ServerChannel::createWorkThread()
     {
       omrthread_entrypoint_t entpoint = helperThread;
       omrthread_t newThread;

       intptr_t ret1 = omrthread_create(&newThread, 0, J9THREAD_PRIORITY_MAX, 0, entpoint, (void *) this);
     }

  // Signal that there are no more jobs to be added to the queue
  void ServerChannel::setJobDone()
     {
     _monitorStatus = MonitorStatus::JOBS_DONE;
     }

  bool ServerChannel::isShutdownComplete()
     {
     return _monitorStatus == MonitorStatus::SHUTDOWN_COMPLETE;
     }

  int ServerChannel::helperThread(void *data)
    {
     ServerChannel *s = (ServerChannel *)data;

     std::cout << " Compiler: initializing\n";
     s->handleCall();

     return 1;
     }

  void ServerChannel::waitForMonitor()
     {
     if(0 != omrthread_monitor_enter(_monitor))
        std::cout << "ERROR WHILE ENTERING on wait monitor" << '\n';

     std::cout << "WAITING ON MONITOR..." << '\n';

     intptr_t tt = omrthread_monitor_wait(_monitor);
     if(J9THREAD_SUCCESS != tt)
        std::cout << "ERROR WHILE WAITING ON monitor, error code: " << tt << '\n';

     std::cout << "MONITOR RELEASED. READY TO GO!" << '\n';

     if(0 != omrthread_monitor_exit(_monitor))
        std::cout << "ERROR WHILE Exiting on wait monitor" << '\n';
     }

  bool ServerChannel::addJobToTheQueue(char * job)
     {
      if(J9THREAD_SUCCESS != omrthread_monitor_enter(_monitor))
         std::cout << "ERROR WHILE ENTERING MONITOR on addJobToTheQueue" << '\n';

      _queueJobs.push(job);
      std::cout << "++ Added job: " << job << " to the queue." << '\n';
      std::cout << "++ There are now " << _queueJobs.size() << " jobs in the queue." << '\n';
      // Notify that there are new jobs to be processed in the queue
      omrthread_monitor_notify_all(_monitor);

      if(J9THREAD_SUCCESS != omrthread_monitor_exit(_monitor))
         std::cout << "ERROR WHILE EXITING MONITOR on addJobToTheQueue" << '\n';
      return true;
     }

  bool ServerChannel::isQueueEmpty()
     {
      return _queueJobs.empty();
     }

  char * ServerChannel::getNextJob()
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
  void ServerChannel::handleCall()
     {
       omrthread_monitor_enter(_monitor);
       std::cout << "Entering monitor at handleCall..." << '\n';

       std::cout << "Inside test entry Point ************************************" << '\n';
       std::cout << "Inside test entry Point $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$" << '\n';
       omrthread_sleep(1000);
       std::cout << "Inside test entry Point ####################################" << '\n';
       std::cout << "Inside test entry Point *********^^^^^^^^^&&&&&&&&&&(()()()())" << '\n';

       while(_monitorStatus != MonitorStatus::JOBS_DONE)
          {
          // Sleep until queue is populated
          waitForMonitor();

          while(!isQueueEmpty())
             {
              omrthread_monitor_exit(_monitor);
              char * job = getNextJob();
              omrthread_sleep(500);
              omrthread_monitor_enter(_monitor);
             }
          }

       std::cout << "Changing status to SHUTDOWN_COMPLETE!!!!" << '\n';
       _monitorStatus = MonitorStatus::SHUTDOWN_COMPLETE;
       omrthread_monitor_notify_all(_monitor);

       if(J9THREAD_SUCCESS != omrthread_monitor_exit(_monitor))
          std::cout << "ERROR WHILE EXITING MONITOR on handleCall" << '\n';
     }
