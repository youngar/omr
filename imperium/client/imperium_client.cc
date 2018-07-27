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

#include <string>
#include <vector>
#include <cstdio>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/JitBuilderRecorderTextFile.hpp"

#include "imperium/imperium.hpp"

using namespace OMR::Imperium;
using std::cout;

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
      }

   bool
   buildIL()
      {
      // ORIGINAL SIMPLE.cpp
      Return(
         Add(
            Load("value"),
            ConstInt32(5)));
      return true;
      }
};

int main(int argc, char** argv) {
  char * fileName = std::tmpnam(NULL);
  ClientChannel client("localhost:50055");
  // client.initClient("localhost:50055"); // Maybe make it private and call it in the constructor?

  omrthread_init_library();
  bool initialized = initializeJit();
  if (!initialized)
     {
     std::cerr << "FAIL: could not initialize JIT\n";
     exit(-1);
     }

  for (size_t i = 0; i < 1; i++)
     {
       TR::TypeDictionary types;
       // Create a recorder so we can directly control the file for this particular test
       TR::JitBuilderRecorderTextFile recorder(NULL, fileName);

       std::cout << "Step #: compile method builder\n";
       SimpleMethod method(&types, &recorder);
       uint8_t * entryPoint;
       client.requestCompile(fileName, &entryPoint, &method);
     }

  std::cout << "ABOUT TO CALL CLIENT DESTRUCTOR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << '\n';

  client.shutdown();
  omrthread_shutdown_library();
  shutdownJit();
  return 0;
}
