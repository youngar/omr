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

#include "imperium/imperium.hpp"

using namespace OMR::Imperium;
using std::cout;

int main(int argc, char** argv) {
  char * fileName = strdup("simple.out");
  ClientChannel client;
  client.initClient("localhost:50055"); // Maybe make it private and call it in the constructor?
  client.generateIL(fileName);

  uint8_t *entry = 0;
  char * fileNames[] = {fileName, strdup("simple3.out")};

  // Param 1: File names as array of char *
  // Param 2: Number of file names
  // TODO: get rid of hardcoded array size
  std::vector<std::string> files = ClientChannel::readFilesAsString(fileNames, 2);

  ClientMessage m = client.constructMessage(files.at(0), reinterpret_cast<uint64_t>(&entry));
  ClientMessage m1 = client.constructMessage(files.at(1), reinterpret_cast<uint64_t>(&entry));
  ClientMessage m2 = client.constructMessage(files.at(0), reinterpret_cast<uint64_t>(&entry));

  // Populate queue with .out files produced by the same client instance
  for(int i = 0; i < 1000; i++) {
    client.addMessageToTheQueue(m);
    client.addMessageToTheQueue(m1);
    // omrthread_sleep(200);
  }

  std::cout << "ABOUT TO CALL CLIENT DESTRUCTOR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << '\n';
  // TODO: figure out what is causing Segmentation fault: 11 :(
  //       server is hanging for a moment, Seg fault happens on client side
  return 0;
}
