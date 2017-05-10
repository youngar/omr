/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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

#include "JBTestUtil.hpp"

#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/VirtualMachineState.hpp"

#define EXPECT_BCIEQ(expected_index, actual_index) do {\
   EXPECT_EQ((expected_index), (actual_index)) << "Unexpected bytecode index (implies bytecode was either missed or erroneously added in worklist)."; \
   } while (0)

#define REPORT_VMSTATE(b,s)

// BC0:  r = p;
// BC1:  r++;
//       if (r > 10) goto BC3;
// BC2:  if (r > 5)  goto BC5;
// BC4:  if (r < 3)  goto BC1; // loop back
// BC9:  return 9;
//
// BC5:  if (r < 8) goto BC11;
// BC17: return 17;
//
// BC7:  if (r < 17) goto BC1; // loop back
// BC8:  return 8;
//
//
// BC3:  if (r > 15) goto BC7;
// BC6:  if (r > 12) goto BC13;
// BC16: return 16;

// BC7:  return 7;
// BC11: return 11;
// BC13: return 13;

// no BC10, BC12, BC14, BC15, BC18, BC19

int32_t
computeExpectedResult(int32_t p)
   {
   // BC0
   int32_t r = p;
BC1:
   r++;
   if (r > 10)
      {
      // BC3
      if (r > 15)
         {
         // BC7
         if (r < 17)
            goto BC1;
         else
            return 8; // BC8
         }
      else
         {
         // BC6
         if (r > 12)
            return 13; // BC13
         else
            return 16; // BC16
         }
      }
   else
      {
      // BC2
      if (r > 5)
         {
         // BC5
         if (r < 8)
            return 11; // BC11
         else
            return 17; // BC17
         }
      else
         {
         // BC4
         if (r < 3)
            goto BC1;
         else
            return 9; // BC9
         }
      }
   }

typedef int32_t (*WorklistTestMethodFunction)(int32_t);

DEFINE_BUILDER( WorklistTestMethod,
                Int32,
                PARAM("path", Int32) )
   {
   const char *bcName[20] = {
      "bc0",  "bc1",  "bc2",  "bc3",  "bc4",  "bc5",  "bc6",  "bc7",  "bc8",  "bc9",
      "bc10", "bc11", "bc12", "bc13", "bc14", "bc15", "bc16", "bc17", "bc18", "bc19" };

   TR::BytecodeBuilder *builders[20];
   for (int32_t i=0;i < 20;i++)
      builders[i] = OrphanBytecodeBuilder(i, (char *)bcName[i]);

   OMR::VirtualMachineState *vmState = new OMR::VirtualMachineState();
   setVMState(vmState);

   Store("result",
      Load("path"));

   // should add 0 to the worklist
   AppendBuilder(builders[0]);

   TR::BytecodeBuilder *b;

   // BCI 0
   int32_t bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(0, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->AddFallThroughBuilder(builders[1]);

   // BCI 1
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(1, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Store("result",
   b->   Add(
   b->      Load("result"),
   b->      ConstInt32(1)));
   b->IfCmpGreaterThan(builders[3],
   b->   Load("result"),
   b->   ConstInt32(10));
   b->AddFallThroughBuilder(builders[2]);

   // BCI 2
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(2, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpGreaterThan(builders[5],
   b->   Load("result"),
   b->   ConstInt32(5));
   b->AddFallThroughBuilder(builders[4]);

   // BCI 3
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(3, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpGreaterThan(builders[7],
   b->   Load("result"),
   b->   ConstInt32(15));
   b->AddFallThroughBuilder(builders[6]);

   // BCI 4
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(4, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpLessThan(builders[1], // should not revisit 1
   b->   Load("result"),
   b->   ConstInt32(3));
   b->AddFallThroughBuilder(builders[9]);

   // BCI 5
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(5, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpLessThan(builders[11],
   b->   Load("result"),
   b->   ConstInt32(8));
   b->AddFallThroughBuilder(builders[17]);

   // BCI 6
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(6, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpGreaterThan(builders[13],
   b->   Load("result"),
   b->   ConstInt32(12));
   b->AddFallThroughBuilder(builders[16]);

   // BCI 7
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(7, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpLessThan(builders[1],   // should not revisit 1
   b->   Load("result"),
   b->   ConstInt32(17));
   b->AddFallThroughBuilder(builders[8]);

   // BCI 8
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(8, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(8));

   // BCI 9
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(9, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(9));

   // BCI 11
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(11, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(11));

   // BCI 13
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(13, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(13));

   // BCI 16
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(16, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(16));

   // BCI 17
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(17, bci);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(17));

   // should be no unvisited bytecodes left
   bci = GetNextBytecodeFromWorklist();
   EXPECT_BCIEQ(bci,-1);

   return true;
   }

class WorklistTest : public JitBuilderTest {};

TEST_F(WorklistTest, test)
   {
   WorklistTestMethodFunction worklistTestMethod;
   ASSERT_COMPILE(TR::TypeDictionary, WorklistTestMethod, worklistTestMethod);

   for (int32_t p=0;p < 20;p++)
      {
      int32_t worklistTestResult = worklistTestMethod(p);
      ASSERT_EQ(computeExpectedResult(p), worklistTestResult)
            << "Path " << p << " generated incorrect result.";
      }
   }
