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
 ******************************************************************************/

 #ifndef OMR_JITBUILDERREPLAY_TEXTFILE_INCL
 #define OMR_JITBUILDERREPLAY_TEXTFILE_INCL


 #ifndef TR_JITBUILDERREPLAY_TEXTFILE_DEFINED
 #define TR_JITBUILDERREPLAY_TEXTFILE_DEFINED
 #define PUT_OMR_JITBUILDERREPLAY_TEXTFILE_INTO_TR
 #endif


 #include "ilgen/JitBuilderReplay.hpp"
 #include "ilgen/IlBuilderRecorder.hpp"
 // #include "JitBuilderReplay.hpp" // Works for gRPC
 // #include "IlBuilderRecorder.hpp"

 #include <iostream>
 #include <sstream>
 #include <fstream>
 #include <map>

 namespace OMR
 {

 class JitBuilderReplayTextFile : public TR::JitBuilderReplay
    {
    public:

    // enum to handle Service method to distinguish between a MethodBuilder and an IlBuilder
    enum BuilderFlag {METHOD_BUILDER, IL_BUILDER};

    JitBuilderReplayTextFile(const char *fileName);
    JitBuilderReplayTextFile(std::string fileString);

    void start();
    void processFirstLineFromTextFile();
    char * getLineAsChar();
    char * getTokensFromLine(std::string);

    bool parseConstructor();
    bool parseBuildIL();

    void addIDPointerPairToMap(char * tokens);
    char * getServiceStringFromToken(uint32_t strLen, char * tokens);
    const char * getServiceStringFromMap(char ** tokens);

    bool handleService(BuilderFlag builderFlag, char * tokens);
    bool handleServiceMethodBuilder(uint32_t mbID, char * tokens);
    bool handleServiceIlBuilder(uint32_t mbID, char * tokens);

    void handleMethodBuilder(uint32_t serviceID, char * tokens);
    void handleDefineLine(TR::MethodBuilder * mb, char * tokens);
    void handleDefineFile(TR::MethodBuilder * mb, char * tokens);
    void handleDefineName(TR::MethodBuilder * mb, char * tokens);
    void handleDefineParameter(TR::MethodBuilder * mb, char * tokens);
    void handlePrimitiveType(TR::MethodBuilder * mb, char * tokens);
    void handleDefineReturnType(TR::MethodBuilder * mb, char * tokens);

    void handleConstInt32(TR::IlBuilder * ilmb, char * tokens);
    void handleLoad(TR::IlBuilder * ilmb, char * tokens);
    void handleAdd(TR::IlBuilder * ilmb, char * tokens);
    void handleSub(TR::IlBuilder * ilmb, char * tokens);
    void handleMul(TR::IlBuilder * ilmb, char * tokens);
    void handleDiv(TR::IlBuilder * ilmb, char * tokens);
    void handleAnd(TR::IlBuilder * ilmb, char * tokens);
    void handleOr(TR::IlBuilder * ilmb, char * tokens);
    void handleXor(TR::IlBuilder * ilmb, char * tokens);
    void handleStore(TR::IlBuilder * ilmb, char * tokens);
    void handleForLoop(TR::IlBuilder * ilmb, char * tokens);

    void handleNewIlBuilder(TR::IlBuilder * ilmb, char * tokens);
    void handleLessThan(TR::IlBuilder * ilmb, char * tokens);
    void handleIfThenElse(TR::IlBuilder * ilmb, char * tokens);

    void handleReturnValue(TR::IlBuilder * ilmb, char * tokens);

    uint32_t getNumberFromToken(char * token);

    const char * SPACE = " ";
    const char * NEW_LINE = "\r\n";

    private:
    std::fstream _file;
    std::istringstream _fileStream;
    bool _isFile;

    };

 } // namespace OMR


 #if defined(PUT_OMR_JITBUILDERREPLAY_TEXTFILE_INTO_TR)

 namespace TR
 {
    class JitBuilderReplayTextFile : public OMR::JitBuilderReplayTextFile
       {
       public:
          JitBuilderReplayTextFile(const char *fileName)
             : OMR::JitBuilderReplayTextFile(fileName)
             { }
          JitBuilderReplayTextFile(std::string fileString)
             : OMR::JitBuilderReplayTextFile(fileString)
             { }
          virtual ~JitBuilderReplayTextFile()
             { }
       };

 } // namespace TR

 #endif // defined(PUT_OMR_JITBUILDERREPLAY_TEXTFILE_INTO_TR)

 #endif // !defined(OMR_JITBUILDERREPLAY_TEXTFILE_INCL)
