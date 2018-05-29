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

 #ifndef OMR_JITBUILDERREPLAY_INCL
 #define OMR_JITBUILDERREPLAY_INCL


 #ifndef TR_JITBUILDERREPLAY_DEFINED
 #define TR_JITBUILDERREPLAY_DEFINED
 #define PUT_OMR_JITBUILDERREPLAY_INTO_TR
 #endif

 #include <iostream>
 #include <fstream>
 #include <map>

 namespace OMR
 {

 class JitBuilderReplay
    {
    public:
    JitBuilderReplay();
    virtual ~JitBuilderReplay();
    };

 } // namespace OMR


 #if defined(PUT_OMR_JITBUILDERREPLAY_INTO_TR)

 namespace TR
 {
    class JitBuilderReplay : public OMR::JitBuilderReplay
       {
       public:
          JitBuilderReplay()
             : OMR::JitBuilderReplay()
             { }
          virtual ~JitBuilderReplay()
             { }
       };

 } // namespace TR

 #endif // defined(PUT_OMR_JITBUILDERREPLAY_INTO_TR)

 #endif // !defined(OMR_JITBUILDERREPLAY_INCL)
