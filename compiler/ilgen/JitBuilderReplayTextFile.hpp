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

 #include <iostream>
 #include <fstream>
 #include <map>

 namespace OMR
 {

 class JitBuilderReplayTextFile : public TR::JitBuilderReplay
    {
    public:
    JitBuilderReplayTextFile(const char *fileName);

    void start();
    void processFirstLineFromTextFile();
    std::string getLineFromTextFile();
    char * getTokensFromLine(std::string);

    bool parseConstructor();
    bool parseBuildIl();

    void addIDPointerPairToMap(char * tokens);

    bool handleService(char * tokens);

    // typedef uint32_t              TypeID;

    // Def S4 "16 [DefineLineString]"
    // B2 S4 "3 [132]"
    // Should we insert in the map map.insert(4, pointer to DefineLineString)
    // and then on the next line call _mb.DefineLineString with parameter 132?
    // Or
    // In the first line we know we will call DefineLine() passing a string as a parameter
    // In the next line we construct DefineLine("132") and store in the map
    // the pair [4: pointer to DefineLine("132")]
    uint32_t getNumberFromToken(char * token);

    const char * SPACE = " ";

    private:
    std::fstream _file;

    // extract tokens from line -- strtok
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
          virtual ~JitBuilderReplayTextFile()
             { }
       };

 } // namespace TR

 #endif // defined(PUT_OMR_JITBUILDERREPLAY_TEXTFILE_INTO_TR)

 #endif // !defined(OMR_JITBUILDERREPLAY_TEXTFILE_INCL)
