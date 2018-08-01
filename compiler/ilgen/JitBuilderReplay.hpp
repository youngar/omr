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

 namespace TR { class IlBuilderReplay; }
 namespace TR { class MethodBuilderReplay; }
 namespace TR { class MethodBuilder; }
 namespace TR { class IlBuilder; }

 namespace OMR
 {

 class JitBuilderReplay
    {
    public:

    const char *RECORDER_SIGNATURE = "JBIL"; // JitBuilder IL
    const char *JBIL_COMPLETE      = "Done";
    enum MethodFlag { CONSTRUCTOR_FLAG, BUILDIL_FLAG };

    JitBuilderReplay();
    virtual ~JitBuilderReplay();

    /**
     * @brief Subclasses override these functions to replay from different input formats
     * (helpers)
     */

     virtual void start();
     virtual void initializeMethodBuilder(TR::MethodBuilderReplay * replay);
     virtual bool parseConstructor()              { return false; }
     virtual bool parseBuildIL()                  { return false; }
     virtual void StoreReservedIDs();

     virtual void handleMethodBuilder(uint32_t serviceID, char * tokens)         { }
     virtual void handleDefineLine(TR::MethodBuilder * mb, char * tokens)        { }
     virtual void handleDefineFile(TR::MethodBuilder * mb, char * tokens)        { }
     virtual void handleDefineName(TR::MethodBuilder * mb, char * tokens)        { }
     virtual void handlePrimitiveType(TR::MethodBuilder * mb, char * tokens)     { }
     virtual void handleDefineReturnType(TR::MethodBuilder * mb, char * tokens)  { }

     virtual void handleAdd(TR::IlBuilder * ilmb, char * tokens)        { }
     virtual void handleMul(TR::IlBuilder * ilmb, char * tokens)        { }
     virtual void handleDiv(TR::IlBuilder * ilmb, char * tokens)        { }
     virtual void handleOr(TR::IlBuilder * ilmb, char * tokens)         { }
     virtual void handleAnd(TR::IlBuilder * ilmb, char * tokens)        { }
     virtual void handleXor(TR::IlBuilder * ilmb, char * tokens)        { }
     virtual void handleLoad(TR::IlBuilder * ilmb, char * tokens)       { }
     virtual void handleConstInt32(TR::IlBuilder * ilmb, char * tokens) { }
     virtual void handleForLoop(TR::IlBuilder * ilmb, char * tokens)    { }
     virtual void handleReturn(TR::IlBuilder * ilmb, char * tokens)     { }
     virtual bool handleService(char * service)   { return false; }

     // Define Map that maps IDs to pointers

     typedef const uint32_t                      TypeID;
     typedef void *                              TypePointer;
     virtual void StoreIDPointerPair(TypeID, TypePointer);
     typedef std::map<TypeID, TypePointer> TypeMapPointer;

     /**
      * @brief constant strings used to define output format
      */
     const char *STATEMENT_DEFINENAME                 = "DefineName";
     const char *STATEMENT_DEFINEFILE                 = "DefineFile";
     const char *STATEMENT_DEFINELINESTRING           = "DefineLineString";
     const char *STATEMENT_DEFINELINENUMBER           = "DefineLineNumber";
     const char *STATEMENT_DEFINEPARAMETER            = "DefineParameter";
     const char *STATEMENT_DEFINEARRAYPARAMETER       = "DefineArrayParameter";
     const char *STATEMENT_DEFINERETURNTYPE           = "DefineReturnType";
     const char *STATEMENT_DEFINELOCAL                = "DefineLocal";
     const char *STATEMENT_DEFINEMEMORY               = "DefineMemory";
     const char *STATEMENT_DEFINEFUNCTION             = "DefineFunction";
     const char *STATEMENT_DEFINESTRUCT               = "DefineStruct";
     const char *STATEMENT_DEFINEUNION                = "DefineUnion";
     const char *STATEMENT_DEFINEFIELD                = "DefineField";
     const char *STATEMENT_PRIMITIVETYPE              = "PrimitiveType";
     const char *STATEMENT_POINTERTYPE                = "PointerType";
     const char *STATEMENT_NEWMETHODBUILDER           = "NewMethodBuilder";
     const char *STATEMENT_NEWILBUILDER               = "NewIlBuilder";
     const char *STATEMENT_NEWBYTECODEBUILDER         = "NewBytecodeBuilder";
     const char *STATEMENT_ALLLOCALSHAVEBEENDEFINED   = "AllLocalsHaveBeenDefined";
     const char *STATEMENT_NULLADDRESS                = "NullAddress";
     const char *STATEMENT_CONSTINT8                  = "ConstInt8";
     const char *STATEMENT_CONSTINT16                 = "ConstInt16";
     const char *STATEMENT_CONSTINT32                 = "ConstInt32";
     const char *STATEMENT_CONSTINT64                 = "ConstInt64";
     const char *STATEMENT_CONSTFLOAT                 = "ConstFloat";
     const char *STATEMENT_CONSTDOUBLE                = "ConstDouble";
     const char *STATEMENT_CONSTSTRING                = "ConstString";
     const char *STATEMENT_CONSTADDRESS               = "ConstAddress";
     const char *STATEMENT_INDEXAT                    = "IndexAt";
     const char *STATEMENT_LOAD                       = "Load";
     const char *STATEMENT_LOADAT                     = "LoadAt";
     const char *STATEMENT_LOADINDIRECT               = "LoadIndirect";
     const char *STATEMENT_STORE                      = "Store";
     const char *STATEMENT_STOREOVER                  = "StoreOver";
     const char *STATEMENT_STOREAT                    = "StoreAt";
     const char *STATEMENT_STOREINDIRECT              = "StoreIndirect";
     const char *STATEMENT_CREATELOCALARRAY           = "CreateLocalArray";
     const char *STATEMENT_CREATELOCALSTRUCT          = "CreateLocalStruct";
     const char *STATEMENT_VECTORLOAD                 = "VectorLoad";
     const char *STATEMENT_VECTORLOADAT               = "VectorLoadAt";
     const char *STATEMENT_VECTORSTORE                = "VectorStore";
     const char *STATEMENT_VECTORSTOREAT              = "VectorStoreAt";
     const char *STATEMENT_STRUCTFIELDINSTANCEADDRESS = "StructFieldInstance";
     const char *STATEMENT_UNIONFIELDINSTANCEADDRESS  = "UnionFieldInstance";
     const char *STATEMENT_CONVERTTO                  = "ConvertTo";
     const char *STATEMENT_UNSIGNEDCONVERTTO          = "UnsignedConvertTo";
     const char *STATEMENT_ADD                        = "Add";
     const char *STATEMENT_SUB                        = "Sub";
     const char *STATEMENT_MUL                        = "Mul";
     const char *STATEMENT_DIV                        = "Div";
     const char *STATEMENT_AND                        = "And";
     const char *STATEMENT_OR                         = "Or";
     const char *STATEMENT_XOR                        = "Xor";
     const char *STATEMENT_LESSTHAN                   = "LessThan";
     const char *STATEMENT_GREATERTHAN                = "GreaterThan";
     const char *STATEMENT_NOTEQUALTO                 = "NotEqualTo";
     const char *STATEMENT_APPENDBUILDER              = "AppendBuilder";
     const char *STATEMENT_APPENDBYTECODEBUILDER      = "AppendBytecodeBuilder";
     const char *STATEMENT_GOTO                       = "Goto";
     const char *STATEMENT_UNSIGNEDSHIFTR             = "UnsignedShiftR";
     const char *STATEMENT_RETURN                     = "Return";
     const char *STATEMENT_RETURNVALUE                = "ReturnValue";
     const char *STATEMENT_IFTHENELSE                 = "IfThenElse";
     const char *STATEMENT_IFCMPEQUALZERO             = "IfCmpEqualZero";
     const char *STATEMENT_FORLOOP                    = "ForLoop";
     const char *STATEMENT_CALL                       = "Call";
     const char *STATEMENT_DONECONSTRUCTOR            = "DoneConstructor";

     protected:
     const TR::MethodBuilderReplay *   _mb;
     TypeMapPointer                    _pointerMap;
     uint8_t                           _idSize;

     TypePointer lookupPointer(TypeID id);

     // set method builder

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
