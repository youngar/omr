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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/JitBuilderRecorderTextFile.hpp"
#include "ilgen/JitBuilderReplayTextFile.hpp"
#include "ilgen/MethodBuilderReplay.hpp"
#include "IterativeFib.hpp"

extern bool jitBuilderShouldCompile;

IterativeFibonnaciMethod::IterativeFibonnaciMethod(TR::TypeDictionary *types, TR::JitBuilderRecorder *recorder)
   : MethodBuilder(types, recorder)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_iter"); // defines _method
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
IterativeFibonnaciMethod::buildIL()
   {
   TR::IlBuilder *returnN = NULL;
   IfThen(&returnN,
      LessThan(
         Load("n"),
         ConstInt32(2)));

   // Only executed if "n < 2"
   returnN->Return(
   returnN->   Load("n"));

   Store("LastSum",
      ConstInt32(0));

   Store("Sum",
      ConstInt32(1));

   TR::IlBuilder *iloop = NULL;
   ForLoopUp("i", &iloop,
           ConstInt32(1),
           Load("n"),
           ConstInt32(1));

   iloop->Store("tempSum",
   iloop->   Add(
   iloop->      Load("Sum"),
   iloop->      Load("LastSum")));
   iloop->Store("LastSum",
   iloop->   Load("Sum"));
   iloop->Store("Sum",
   iloop->   Load("tempSum"));

   Return(
      Load("Sum"));

   return true;
   }


int
main(int argc, char *argv[])
   {
   printf("Step 1: initialize JIT\n");
   bool initialized = initializeJit();
   if (!initialized)
      {
      fprintf(stderr, "FAIL: could not initialize JIT\n");
      exit(-1);
      }

   printf("Step 2: define type dictionary\n");
   TR::TypeDictionary types;

   // Create a recorder so we can directly control the file for this particular test
   TR::JitBuilderRecorderTextFile recorder(NULL, "iterFib.out");

   printf("Step 3: compile method builder\n");
   IterativeFibonnaciMethod iterFibMethodBuilder(&types, &recorder);
   
   //TODO Hack to be able to turn compiling off a global level
   jitBuilderShouldCompile = false;
   
   uint8_t *entry=0;
   int32_t rc = compileMethodBuilder(&iterFibMethodBuilder, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   if(jitBuilderShouldCompile)
      {
      printf("Step 4: invoke compiled code and print results\n");
      IterativeFibFunctionType *iter_fib=(IterativeFibFunctionType *)entry;
      for (int32_t n=0;n < 20;n++)
         printf("fib(%2d) = %d\n", n, iter_fib(n));
      }
   else
      { 
      printf("Step 5: Replay\n");
      jitBuilderShouldCompile = true;
      TR::JitBuilderReplayTextFile replay("iterFib.out");
      TR::JitBuilderRecorderTextFile recorder2(NULL, "iterFib2.out");

      TR::TypeDictionary types2;
      uint8_t *entry2 = 0;

      printf("Step 6: verify output file\n");
      TR::MethodBuilderReplay mb(&types2, &replay, &recorder2); // Process Constructor
      rc = compileMethodBuilder(&mb, &entry2); // Process buildIL
      if (rc != 0)
         {
         fprintf(stderr,"FAIL: compilation error %d\n", rc);
         exit(-2);
         }
      
      IterativeFibFunctionType *iter_fib2=(IterativeFibFunctionType *)entry2;
   
      for (int32_t n=0;n < 20;n++) {
         printf("fib(%2d) = %d\n", n, iter_fib2(n));
         }
      }

   printf ("Step 7: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
