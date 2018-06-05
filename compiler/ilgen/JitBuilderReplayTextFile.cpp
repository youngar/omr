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
 #include <cstring>
 #include <iostream>
 #include <fstream>
 #include "infra/Assert.hpp"
 #include "ilgen/JitBuilderReplayTextFile.hpp"


 OMR::JitBuilderReplayTextFile::JitBuilderReplayTextFile(const char *fileName)
    : TR::JitBuilderReplay(), _file(fileName, std::fstream::in)
    {
    // std::cout << "Hey I'm in JitBuilderReplayTextFile()\n";
    // start(); // Start reading/parsing the file
    // start would be implemented in JitBuilderReplay (super class)
    start();
    std::cout << ">>> Start reading 2nd line and BEYOND!" << '\n';
    parseConstructor();
    }

void
OMR::JitBuilderReplayTextFile::start() {
   TR::JitBuilderReplay::start();
   processFirstLineFromTextFile();
}

std::string
OMR::JitBuilderReplayTextFile::getLineFromTextFile()
   {
   std::string line;
   std::getline(_file, line);
   // std::cout << ">>> " << line << '\n';
   return line;
   }

void
OMR::JitBuilderReplayTextFile::processFirstLineFromTextFile()
   {
      char * token = getTokensFromLine(getLineFromTextFile());

      // TR_ASSERT(16 < 10, "Test assertion with digit:  %d", 100);

      while (token != NULL) {
         std::cout << token << '\n';
         token = std::strtok(NULL, SPACE);
      }
   }

char *
OMR::JitBuilderReplayTextFile::getTokensFromLine(std::string currentLine)
   {
      char * line = strdup(currentLine.c_str());

      char * token = std::strtok(line, SPACE);
      //
      // while (token != NULL) {
      //    std::cout << token << '\n';
      //    token = std::strtok(NULL, SPACE);
      // }
      return token;
   }

uint32_t
OMR::JitBuilderReplayTextFile::getNumberFromToken(char * token)
   {
       uint32_t IDtoReturn;

       if(isdigit(token[0]))
          return atoi(&token[0]);

       if(token == NULL)
          {
             std::cerr << "Token is NULL.\n";
             IDtoReturn = 0;
          }
       else if(token[1])
          {
             IDtoReturn = atoi(&token[1]);
             std::cout << "Token found: " << token << ". uint32_t #: " << IDtoReturn << '\n';
          }
       else
          {
             TR_ASSERT_FATAL(0, "Unexpected short token: %s", token);
          }
       return IDtoReturn;
   }

void
OMR::JitBuilderReplayTextFile::addIDPointerPairToMap(char * tokens)
   {
       // Tokes: "Def S3 [16 "DefileLineString"]"
       tokens = std::strtok(NULL, SPACE);
       uint32_t ID = getNumberFromToken(tokens);
       tokens = std::strtok(NULL, SPACE);
       uint32_t strLen = getNumberFromToken(tokens);
       tokens = std::strtok(NULL, SPACE);
       char idPointer[strLen];

       std::cout << "&tokens[1]: " << &tokens[1] << '\n';
       strncpy(idPointer, &tokens[1], sizeof(idPointer));
       idPointer[strLen] = '\0';

       std::cout << "Storing ID: " << ID << ", and pointer: " << idPointer << ", to the map." << '\n';
       StoreIDPointerPair(ID, idPointer);
       std::cout << "idPointer: " << idPointer << '\n';
   }

bool
OMR::JitBuilderReplayTextFile::handleService(char * tokens)
   {
       // Each if statement for each service will handle the tokens accordingly
       // tokens: "B2 S5 "14 [src/Simple.cpp]""
       // First check what service the second token holds
       // call helper on service with parameters:
       //         builder ID
       //         rest of tokens 

       return false;
   }

bool
OMR::JitBuilderReplayTextFile::parseConstructor()
   {
      std::cout << "Parsing Constructor...\n";
      // get a line
      // get tokens from the line
      // if DEF is seen: eg. Def A# "length [STRING]" --> 4 tokens
         // Def
         // A#
         // "length <-- use this length to find out how many chars to keep from next token after first char
         // [STRING]"
         // put pointer value at given ID in table --> what is this pointer value?????
      // if B[#] is seen == operation is seen: eg. B2 S4
         // B2
         char * tokens;
         bool constructorFlag = true;

         std::string textLine;

         while(constructorFlag)
         {
            textLine = getLineFromTextFile();
            tokens = getTokensFromLine(textLine);

            uint32_t id = getNumberFromToken(tokens);

            if(id == 0) // Its DEF
               {
                   std::cout << "I am DEF: " << tokens << '\n';
                   // Store something in the table
                   addIDPointerPairToMap(tokens);
               }
            else if (tokens != NULL)  // Its probably a call to MethodBuilder
               {
                   std::cout << "I am a builder call: " << tokens << '\n';
                   constructorFlag = handleService(tokens);
               }
         }
   }

bool
OMR::JitBuilderReplayTextFile::parseBuildIl()
   {

   }
    // Do the processing here?
    // Useful http://www.cplusplus.com/reference/string/string/
