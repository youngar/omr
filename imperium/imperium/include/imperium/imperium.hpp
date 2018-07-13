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

 #include <cstring>
 #include <queue>
 #include <iostream>
 #include <stdlib.h>

 #include "omrthread.h"

 // namespace OMR
 // {

 class ServerChannel
    {
    public:
    enum MonitorStatus {
      SHUTTING_DOWN,
      SHUTDOWN_COMPLETE,
      INITIALIZING,
      INITIALIZING_COMPLETE, // INITIALIZATION ******
      JOBS_DONE // nothing left to be added to the queue -- RENAME ******
    };

    ServerChannel();
    ~ServerChannel();

    void createWorkThread();
    void setJobDone(); // RENAME ACCORDING TO NEW MONITOR STATUS NAME ******
    bool isShutdownComplete();
    void waitForMonitor();
    bool addJobToTheQueue(char * job);
    bool isQueueEmpty();

    private:
    omrthread_monitor_t _monitor;
    MonitorStatus _monitorStatus;
    std::queue<char *> _queueJobs;
    void destroy(); // RENAME ******
    omrthread_t attachSelf();
    static int helperThread(void * data);
    char * getNextJob();
    void handleCall();
    };

 // } // namespace OMR
 //
 // namespace TR
 // {
 //    class ServerChannel : public OMR::ServerChannel
 //       {
 //       public:
 //          ServerChannel()
 //            : OMR::ServerChannel()
 //            { }
 //          virtual ~ServerChannel()
 //            { }
 //       };
 // } // namespace TR
