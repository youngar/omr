/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2018, 2018
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

 #include <stdint.h>
 #include <iostream>
 #include <fstream>

 #include "infra/Assert.hpp"
 #include "ilgen/JitBuilderReplayTextFile.hpp"


 OMR::JitBuilderReplayTextFile::JitBuilderReplayTextFile(const char *fileName)
    : TR::JitBuilderReplay()
    {
    std::cout << "Hey I'm in JitBuilderReplayTextFile()\n";
    // start(); // Start reading/parsing the file
    // start would be implemented in JitBuilderReplay (super class)
    }

    // Do the processing here?
    // Useful http://www.cplusplus.com/reference/string/string/
