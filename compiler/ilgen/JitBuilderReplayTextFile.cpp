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
 #include "ilgen/MethodBuilder.hpp"
 #include "ilgen/JitBuilderReplay.hpp"
 #include "ilgen/TypeDictionary.hpp"
  #include "ilgen/IlBuilder.hpp"

 OMR::JitBuilderReplayTextFile::JitBuilderReplayTextFile(const char *fileName)
    : TR::JitBuilderReplay(), _file(fileName, std::fstream::in), _isFile(true)
    {
    start();
    std::cout << "File constructor..." << '\n';
    std::cout << ">>> Start reading 2nd line and BEYOND!" << '\n';
    }

 OMR::JitBuilderReplayTextFile::JitBuilderReplayTextFile(std::string fileString)
    : TR::JitBuilderReplay(), _isFile(false), _fileStream(std::istringstream(fileString))
    {
    start();
    std::cout << "File as a string constructor..." << '\n';
    std::cout << ">>> Start reading 2nd line and BEYOND!" << '\n';
    }

void
OMR::JitBuilderReplayTextFile::start()
   {
   TR::JitBuilderReplay::start();
   processFirstLineFromTextFile();
   }

char *
OMR::JitBuilderReplayTextFile::getLineAsChar()
   {
   char * lineAsChar;
   std::string line;

   if(_isFile)
      {
      std::getline(_file, line);
      }
   else
      {
      std::getline(_fileStream, line);
      }

   lineAsChar = getTokensFromLine(line);
   return lineAsChar;
   }

void
OMR::JitBuilderReplayTextFile::processFirstLineFromTextFile()
   {
    char * token;
    token = getLineAsChar();

    while (token != NULL) {
       token = std::strtok(NULL, SPACE);
      }
   }

char *
OMR::JitBuilderReplayTextFile::getTokensFromLine(std::string currentLine)
   {
      char * line = strdup(currentLine.c_str());
      char * token = std::strtok(line, SPACE);

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
             // std::cout << "Token found: " << token << ". uint32_t #: " << IDtoReturn << '\n';
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

       char * idPointer = getServiceStringFromToken(strLen, tokens);

       // std::cout << "Storing ID: " << ID << ", and pointer: " << idPointer << ", to the map." << '\n';
       StoreIDPointerPair(ID, idPointer);
   }

char *
OMR::JitBuilderReplayTextFile::getServiceStringFromToken(uint32_t strLen, char * tokens)
   {
     char idPointerStr[strLen];
     strncpy(idPointerStr, &tokens[1], sizeof(idPointerStr));
     idPointerStr[strLen] = '\0';

     return strdup(idPointerStr);
   }

const char *
OMR::JitBuilderReplayTextFile::getServiceStringFromMap(char ** tokens)
   {
     char * temp = *tokens;
     temp = std::strtok(NULL, SPACE);
     uint32_t serviceID = getNumberFromToken(temp);

     const char * serviceString = static_cast<const char *>(lookupPointer(serviceID));
     temp = std::strtok(NULL, SPACE);

     *tokens = temp;

     return serviceString;
   }

bool
OMR::JitBuilderReplayTextFile::handleService(BuilderFlag builderFlag, char * tokens)
   {
       bool handleServiceFlag = true;

       uint32_t mbID = getNumberFromToken(tokens);

       if(builderFlag == METHOD_BUILDER)
          {
              // divides work into handleServiceMethodBuilder and handleServiceIlBuilder
              handleServiceFlag = handleServiceMethodBuilder(mbID, tokens);
          }
       else if(builderFlag == IL_BUILDER)
          {
              // divided work for handleServiceIlBuilder
              handleServiceFlag = handleServiceIlBuilder(mbID, tokens);
          }

       return handleServiceFlag;
   }

bool
OMR::JitBuilderReplayTextFile::handleServiceMethodBuilder(uint32_t mbID, char * tokens)
   {
   TR::MethodBuilder * mb = static_cast<TR::MethodBuilder *>(lookupPointer(mbID));

   const char * serviceString = getServiceStringFromMap(&tokens);

   // At this point we already have a builder and the service string to be called
   // Therefore, tokens will contain 1 to 3 tokens at the most

   // Here we have the builder and the service to be called
   // Starting to check if else if statements to call each service
   if(strcmp(serviceString, STATEMENT_NEWMETHODBUILDER) == 0)
      {
      handleMethodBuilder(mbID, tokens);
      }
   else if(strcmp(serviceString, STATEMENT_DEFINELINESTRING) == 0)
      {
      handleDefineLine(mb, tokens);
      }
   else if(strcmp(serviceString, STATEMENT_DEFINEFILE) == 0)
      {
      handleDefineFile(mb, tokens);
      }
   else if(strcmp(serviceString, STATEMENT_DEFINENAME) == 0)
      {
      handleDefineName(mb, tokens);
      }
   else if(strcmp(serviceString, STATEMENT_DEFINEPARAMETER) == 0)
      {
      handleDefineParameter(mb, tokens);
      }
   else if(strcmp(serviceString, STATEMENT_PRIMITIVETYPE) == 0)
      {
      handlePrimitiveType(mb, tokens);
      }
   else if(strcmp(serviceString, STATEMENT_DEFINERETURNTYPE) == 0)
      {
      handleDefineReturnType(mb, tokens);
      }
   else if(strcmp(serviceString, STATEMENT_DONECONSTRUCTOR) == 0)
      {
      std::cout << "Constructor DONE. About to call buildIl.\n";
      return false;
      }

   return true;
   }


bool
OMR::JitBuilderReplayTextFile::handleServiceIlBuilder(uint32_t mbID, char * tokens)
   {
      TR::IlBuilder * ilmb = static_cast<TR::IlBuilder *>(lookupPointer(mbID));

      const char * serviceString = getServiceStringFromMap(&tokens);

      if(strcmp(serviceString, STATEMENT_CONSTINT32) == 0)
         {
         handleConstInt32(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_LOAD) == 0)
         {
         handleLoad(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_ADD) == 0)
         {
         handleAdd(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_RETURNVALUE) == 0)
         {
         handleReturnValue(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_STORE) == 0)
         {
         handleStore(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_SUB) == 0)
         {
         handleSub(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_MUL) == 0)
         {
         handleMul(ilmb, tokens);
         }      
      else if(strcmp(serviceString, STATEMENT_DIV) == 0)
         {
         handleDiv(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_AND) == 0)
         {
         handleAnd(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_OR) == 0)
         {
         handleOr(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_XOR) == 0)
         {
         handleXor(ilmb, tokens);
         }
      else if (strcmp(serviceString, STATEMENT_LESSTHAN) == 0) 
         {
         handleLessThan(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_NEWILBUILDER) == 0) 
         {
         handleNewIlBuilder(ilmb, tokens);
         } 
      else if(strcmp(serviceString, STATEMENT_IFTHENELSE) == 0)
         {
         handleIfThenElse(ilmb, tokens);
         }
      else if(strcmp(serviceString, STATEMENT_FORLOOP) == 0)
         {
         handleForLoop(ilmb, tokens);
         }
      else
         {
         TR_ASSERT_FATAL(0, "handleServiceIlBuilder asked to handle unknown serive %s", serviceString);
         }

      return true;
   }

void
OMR::JitBuilderReplayTextFile::handleMethodBuilder(uint32_t serviceID, char * tokens)
   {
      // Sanity check to see if ID from token matches ID from methodBuilder
      std::cout << "Calling handleMethodBuilder helper.\n";

      uint32_t mbID = getNumberFromToken(tokens);
      if(serviceID == mbID)
         {
         std::cout << "MethodBuilder IDs match passed.\n";
         }
      else
         {
         TR_ASSERT_FATAL(0, "MethodBuilder IDs do not match. Found ID %d in map while %d was passed as a parameter.", serviceID, mbID);
         }
   }

void
OMR::JitBuilderReplayTextFile::handleDefineLine(TR::MethodBuilder * mb, char * tokens)
   {
      std::cout << "Calling handleDefineLine helper.\n";
      uint32_t strLen = getNumberFromToken(tokens);
      tokens = std::strtok(NULL, SPACE);

      const char * defineLineParam = getServiceStringFromToken(strLen, tokens);

      mb->DefineLine(defineLineParam);
   }

void
OMR::JitBuilderReplayTextFile::handleDefineFile(TR::MethodBuilder * mb, char * tokens)
   {
     std::cout << "Calling handleDefineFile helper.\n";
     uint32_t strLen = getNumberFromToken(tokens);
     tokens = std::strtok(NULL, SPACE);

     const char * defineFileParam = getServiceStringFromToken(strLen, tokens);

     mb->DefineFile(defineFileParam);
   }

void
OMR::JitBuilderReplayTextFile::handleDefineName(TR::MethodBuilder * mb, char * tokens)
   {
     std::cout << "Calling handleDefineName helper.\n";
     uint32_t strLen = getNumberFromToken(tokens);
     tokens = std::strtok(NULL, SPACE);

     const char * defineNameParam = getServiceStringFromToken(strLen, tokens);

     mb->DefineName(defineNameParam);
   }

void
OMR::JitBuilderReplayTextFile::handleDefineParameter(TR::MethodBuilder * mb, char * tokens)
   {
    // B2 S9 T7 "5 [value]"
    // DefineParameter("value", Int32);
    std::cout << "Calling handleDefineParameter helper.\n";

    uint32_t typeID = getNumberFromToken(tokens);
    TR::IlType *type = static_cast<TR::IlType *>(lookupPointer(typeID));
    tokens = std::strtok(NULL, SPACE);

    uint32_t strLen = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);
    const char * defineParameterParam = getServiceStringFromToken(strLen, tokens);

    mb->DefineParameter(defineParameterParam, type);
   }

void OMR::JitBuilderReplayTextFile::handlePrimitiveType(TR::MethodBuilder * mb, char * tokens)
   {
    std::cout << "Calling handlePrimitiveType helper.\n";
    // Store IlType in map with respective ID from tokens
    // tokens: "T7 3"
    // Key: 7, Value: IlValue(3)
    uint32_t ID = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);
    uint32_t value = getNumberFromToken(tokens);

    TR::DataType dt((TR::DataTypes)value);
    TR::IlType *type = mb->typeDictionary()->PrimitiveType(dt);

    StoreIDPointerPair(ID, type);
   }

void
OMR::JitBuilderReplayTextFile::handleDefineReturnType(TR::MethodBuilder * mb, char * tokens)
   {
     std::cout << "Calling handleDefineReturnType helper.\n";
     uint32_t ID = getNumberFromToken(tokens);

     TR::IlType * returnTypeParam = static_cast<TR::IlType *>(lookupPointer(ID));

     mb->DefineReturnType(returnTypeParam);
   }

void
OMR::JitBuilderReplayTextFile::handleConstInt32(TR::IlBuilder * ilmb, char * tokens)
   {
   // Put into map: key ID, value IlValue*
   std::cout << "Calling handleConstInt32 helper.\n";

   uint32_t ID = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);
   uint32_t value = getNumberFromToken(tokens);

   // std::cout << "Found ID: " << ID << ", and value: " << value << ". Storing into map.\n";

   IlValue * val = ilmb->ConstInt32(value);
   StoreIDPointerPair(ID, val);
   }


void
OMR::JitBuilderReplayTextFile::handleLoad(TR::IlBuilder * ilmb, char * tokens)
   {
   // Def S12 "4 [Load]"
   // B2 S12 V11 "5 [value]"
   // Load("value")
   std::cout << "Calling handleLoad helper.\n";

   uint32_t ID = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);

   uint32_t strLen = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);
   const char * defineLoadParam = getServiceStringFromToken(strLen, tokens);

   IlValue * loadVal = ilmb->Load(defineLoadParam);
   StoreIDPointerPair(ID, loadVal);
   }



   void
   OMR::JitBuilderReplayTextFile::handleStore(TR::IlBuilder * ilmb, char * tokens)
      {
      // Def S22 "5 [Store]"
      //B2 S22 "7 [result2]" V21
      //B2 S13 V23 "7 [result2]"
      std::cout << "Calling handleStore helper.\n";

    //  uint32_t ID = getNumberFromToken(tokens);
    //  tokens = std::strtok(NULL, SPACE);

      uint32_t strLen = getNumberFromToken(tokens);
      tokens = std::strtok(NULL, SPACE);
      const char * defineStoreParam = getServiceStringFromToken(strLen, tokens);
      tokens = std::strtok(NULL, SPACE);
      uint32_t valuetoStore = getNumberFromToken(tokens);

      TR::IlValue * paramIlValue = static_cast<TR::IlValue *>(lookupPointer(valuetoStore));

      ilmb->Store(defineStoreParam, paramIlValue);
      //StoreIDPointerPair(ID, loadVal);
      }

void
OMR::JitBuilderReplayTextFile::handleAdd(TR::IlBuilder * ilmb, char * tokens)
   {
   // Def S16 "3 [Add]"
   // B2 S16 V15 V11 V13
   std::cout << "Calling handleAdd helper.\n";

   uint32_t IDtoStore = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);

   uint32_t param1ID = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);
   uint32_t param2ID = getNumberFromToken(tokens);

   TR::IlValue * param1IlValue = static_cast<TR::IlValue *>(lookupPointer(param1ID));
   TR::IlValue * param2IlValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));

   TR::IlValue * addResult = ilmb->Add(param1IlValue, param2IlValue);

   StoreIDPointerPair(IDtoStore, addResult);
   }

//Add Sub June.27
   void
   OMR::JitBuilderReplayTextFile::handleSub(TR::IlBuilder * ilmb, char * tokens)
      {
      // Def S16 "3 [Sub]"
      // B2 S16 V15 V11 V13
      std::cout << "Calling handleSub helper.\n";

      uint32_t IDtoStore = getNumberFromToken(tokens);
      tokens = std::strtok(NULL, SPACE);

      uint32_t param1ID = getNumberFromToken(tokens);
      tokens = std::strtok(NULL, SPACE);
      uint32_t param2ID = getNumberFromToken(tokens);

      TR::IlValue * param1IlValue = static_cast<TR::IlValue *>(lookupPointer(param1ID));
      TR::IlValue * param2IlValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));

      TR::IlValue * subResult = ilmb->Sub(param1IlValue, param2IlValue);

      StoreIDPointerPair(IDtoStore, subResult);
      }

  void
  OMR::JitBuilderReplayTextFile::handleMul(TR::IlBuilder * ilmb, char * tokens)
    {
    // Def S16 "3 [Mul]"
    // B2 S16 V15 V11 V13
    std::cout << "Calling handleMul helper.\n";

    uint32_t IDtoStore = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);

    uint32_t param1ID = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);
    uint32_t param2ID = getNumberFromToken(tokens);

    TR::IlValue * param1IlValue = static_cast<TR::IlValue *>(lookupPointer(param1ID));
    TR::IlValue * param2IlValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));
    TR::IlValue * addResult = ilmb->Mul(param1IlValue, param2IlValue);

    StoreIDPointerPair(IDtoStore, addResult);
    }

  void
  OMR::JitBuilderReplayTextFile::handleDiv(TR::IlBuilder * ilmb, char * tokens)
    {
    std::cout << "Calling handleDiv helper.\n";
    
    uint32_t IDtoStore = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);

    uint32_t param1ID = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);
    uint32_t param2ID = getNumberFromToken(tokens);

    TR::IlValue * param1IlValue = static_cast<TR::IlValue *>(lookupPointer(param1ID));
    TR::IlValue * param2IlValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));

    TR::IlValue * divResult = ilmb->Div(param1IlValue, param2IlValue);

    StoreIDPointerPair(IDtoStore, divResult);
    }

void
OMR::JitBuilderReplayTextFile::handleAnd(TR::IlBuilder * ilmb, char * tokens)
   {
   std::cout << "Calling handleAnd helper.\n";

   uint32_t IDtoStore = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);

   uint32_t param1ID = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);
   uint32_t param2ID = getNumberFromToken(tokens);

   TR::IlValue * param1IlValue = static_cast<TR::IlValue *>(lookupPointer(param1ID));
   TR::IlValue * param2IlValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));

   TR::IlValue * andResult = ilmb->And(param1IlValue, param2IlValue);

   StoreIDPointerPair(IDtoStore, andResult);
   }

void
OMR::JitBuilderReplayTextFile::handleOr(TR::IlBuilder * ilmb, char * tokens)
   {
   std::cout << "Calling handleOr helper.\n";

   uint32_t IDtoStore = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);

   uint32_t param1ID = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);
   uint32_t param2ID = getNumberFromToken(tokens);

   TR::IlValue * param1IlValue = static_cast<TR::IlValue *>(lookupPointer(param1ID));
   TR::IlValue * param2IlValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));

   TR::IlValue * orResult = ilmb->Or(param1IlValue, param2IlValue);

   StoreIDPointerPair(IDtoStore, orResult);
   }

void
OMR::JitBuilderReplayTextFile::handleXor(TR::IlBuilder * ilmb, char * tokens)
   {
   std::cout << "Calling handleXor helper.\n";

   uint32_t IDtoStore = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);

   uint32_t param1ID = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);
   uint32_t param2ID = getNumberFromToken(tokens);

   TR::IlValue * param1IlValue = static_cast<TR::IlValue *>(lookupPointer(param1ID));
   TR::IlValue * param2IlValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));

   TR::IlValue * xorResult = ilmb->Xor(param1IlValue, param2IlValue);

   StoreIDPointerPair(IDtoStore, xorResult);
   }

// Add New Builder June.29
  void
  OMR::JitBuilderReplayTextFile::handleNewIlBuilder(TR::IlBuilder * ilmb, char * tokens)
   {
   // Def S20 "12 [NewIlBuilder]"
   // B2 S20 B19
   std::cout << "Calling handleNewBuilder helper.\n";

   uint32_t IDtoStore = getNumberFromToken(tokens);
   tokens = std::strtok(NULL, SPACE);


   TR::IlBuilder * newBuilder = ilmb->OrphanBuilder();//???error: cannot
      //initialize a variable of type 'TR::IlValue *' with an rvalue of type
      //'void'

   StoreIDPointerPair(IDtoStore, newBuilder);
   }

// Add LessThan June.29
  void
  OMR::JitBuilderReplayTextFile::handleLessThan(TR::IlBuilder * ilmb, char * tokens){
    // Def S17 "8 [LessThan]"
    // B2 S17 V16 V12 V14
    std::cout << "Calling handleLessThan helper.\n";

    uint32_t IDtoStore = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);

    uint32_t param1ID = getNumberFromToken(tokens);
    tokens = std::strtok(NULL, SPACE);
    uint32_t param2ID = getNumberFromToken(tokens);

    TR::IlValue * leftValue  = static_cast<TR::IlValue *>(lookupPointer(param1ID));
    TR::IlValue * rightValue = static_cast<TR::IlValue *>(lookupPointer(param2ID));

    TR::IlValue * lessThanResult = ilmb->LessThan(leftValue, rightValue);

    StoreIDPointerPair(IDtoStore, lessThanResult);

  }



   // Add IfThenElse June.29
   void
   OMR::JitBuilderReplayTextFile::handleIfThenElse(TR::IlBuilder * ilmb, char * tokens)
      {
      // B2 S13 V22 "14 [lessThanResult]
      // Def S23 "10 [IfThenElse]"
      // B2 S23 B19 B21 V22
      // B19 S15 V24 1
      // B19 S18 "6 [result]" V24
      // B21 S15 V25 0
      // B21 S18 "6 [result]" V25
      std::cout << "Calling handleIfThenElse helper.\n";

      uint32_t IfID = getNumberFromToken(tokens);
      tokens = std::strtok(NULL, SPACE);

      uint32_t ElseID = getNumberFromToken(tokens);
      tokens = std::strtok(NULL, SPACE);
      uint32_t conditionID = getNumberFromToken(tokens);

      TR::IlBuilder * IfBlock   = static_cast<TR::IlBuilder *>(lookupPointer(IfID));
      
      TR::IlValue * conditionValue = static_cast<TR::IlValue *>(lookupPointer(conditionID));

      if(ElseID == 0)
         ilmb->IfThen(&IfBlock, conditionValue);
      else {
        TR::IlBuilder * ElseBlock = static_cast<TR::IlBuilder *>(lookupPointer(ElseID));
        ilmb->IfThenElse(&IfBlock, &ElseBlock, conditionValue);
      }
         
      }

   void
   OMR::JitBuilderReplayTextFile::handleForLoop(TR::IlBuilder * ilmb, char * tokens)
      {
      // Def S30 "7 [ForLoop]" 
      // B2 S30  1 "1 [i]" B29 Def Def V26 V27 V28 
      std::cout << "Calling handleForLoop helper.\n";

      bool boolParam;
      uint32_t boolean = getNumberFromToken(tokens);

      if(boolean == 1)
          boolParam = true;
      else
          boolParam = false;
      
      tokens = std::strtok(NULL, SPACE);
      uint32_t strLen = getNumberFromToken(tokens);
      tokens = std::strtok(NULL, SPACE);

      char * indVar = getServiceStringFromToken(strLen, tokens);
      tokens = std::strtok(NULL, SPACE);

      uint32_t builderID = getNumberFromToken(tokens);
      TR::IlBuilder * body = static_cast<TR::IlBuilder *>(lookupPointer(builderID));
      tokens = std::strtok(NULL, SPACE);

      TR::IlBuilder * breakKey;
      TR::IlBuilder * continueKey;

      uint32_t breakID = getNumberFromToken(tokens);
      breakKey = static_cast<TR::IlBuilder *>(lookupPointer(breakID));
         
      tokens = std::strtok(NULL, SPACE);

      uint32_t continueID = getNumberFromToken(tokens);
      continueKey = static_cast<TR::IlBuilder *>(lookupPointer(continueID));

      tokens = std::strtok(NULL, SPACE);

      uint32_t initID = getNumberFromToken(tokens);
      TR::IlValue * initValue = static_cast<TR::IlValue *>(lookupPointer(initID));
      tokens = std::strtok(NULL, SPACE);

      uint32_t iterateID = getNumberFromToken(tokens);
      TR::IlValue * iterateValue = static_cast<TR::IlValue *>(lookupPointer(iterateID));
      tokens = std::strtok(NULL, SPACE);
      
      uint32_t incrementID = getNumberFromToken(tokens);
      TR::IlValue * incrementValue = static_cast<TR::IlValue *>(lookupPointer(incrementID));

      if(breakID == 0 && continueID == 0)
         ilmb->ForLoop(boolParam, indVar, &body, NULL, NULL, initValue, iterateValue, incrementValue);
      else if(breakID == 0)
         ilmb->ForLoop(boolParam, indVar, &body, NULL, &continueKey, initValue, iterateValue, incrementValue);
      else if(continueID == 0)
         ilmb->ForLoop(boolParam, indVar, &body, &breakKey, NULL, initValue, iterateValue, incrementValue);
      else
         ilmb->ForLoop(boolParam, indVar, &body, &breakKey, &continueKey, initValue, iterateValue, incrementValue);
     }


void
OMR::JitBuilderReplayTextFile::handleReturnValue(TR::IlBuilder * ilmb, char * tokens)
   {
      std::cout << "Calling handleReturnValue helper.\n";

      uint32_t ID = getNumberFromToken(tokens);
      TR::IlValue * value = static_cast<TR::IlValue *>(lookupPointer(ID));

      ilmb->Return(value);
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
            tokens = getLineAsChar();

            uint32_t id = getNumberFromToken(tokens);

            if(id == 0) // It's DEF
               {
                   // std::cout << "I am DEF: " << tokens << '\n';
                   // Store something in the table
                   addIDPointerPairToMap(tokens);
               }
            else if (tokens != NULL)  // Its probably a call to MethodBuilder
               {
                   // std::cout << "I am a builder call: " << tokens << '\n';
                   constructorFlag = handleService(METHOD_BUILDER, tokens);
               }
         }
   }

bool
OMR::JitBuilderReplayTextFile::parseBuildIL()
   {
   std::cout << "Parsing BuildIl...\n";

   char * tokens;
   bool buildIlFlag = true;

   std::string textLine;

   while(buildIlFlag)
      {
      tokens = getLineAsChar();

      uint32_t id = getNumberFromToken(tokens);

      if(id == 0) // It's DEF
         {
            if (tokens[0] == 'I')
               {
                  // std::cout << "I am ID1: " << tokens << '\n';
                  buildIlFlag = false;
               }
            else
               {
                  // std::cout << "I am DEF: " << tokens << '\n';
                  // Store something in the table
                  addIDPointerPairToMap(tokens);
               }
         }
      else if (tokens != NULL)  // Its probably a call to MethodBuilder
         {
             // std::cout << "I am a builder call: " << tokens << '\n';
             buildIlFlag = handleService(IL_BUILDER, tokens);
         }
      }
      return buildIlFlag;
   }
    // Do the processing here?
    // Useful http://www.cplusplus.com/reference/string/string/
