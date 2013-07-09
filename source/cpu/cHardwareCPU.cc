/*
 *  cHardwareCPU.cc
 *  Avida
 *
 *  Called "hardware_cpu.cc" prior to 11/17/05.
 *  Copyright 1999-2011 Michigan State University. All rights reserved.
 *  Copyright 1999-2003 California Institute of Technology.
 *
 *
 *  This file is part of Avida.
 *
 *  Avida is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  Avida is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License along with Avida.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "cHardwareCPU.h"

#include "avida/core/UniverseDriver.h"

#include "avida/private/systematics/SexualAncestry.h"

#include "cAvidaContext.h"
#include "cCPUTestInfo.h"
#include "cEnvironment.h"
#include "cHardwareManager.h"
#include "cHardwareTracer.h"
#include "cInstSet.h"
#include "cOrganism.h"
#include "cOrgMessage.h"
#include "cPhenotype.h"
#include "cPopulation.h"
#include "cPopulationCell.h"
#include "cReaction.h"
#include "cReactionLib.h"
#include "cReactionProcess.h"
#include "cResourceDef.h"
#include "cStateGrid.h"
#include "cStringUtil.h"
#include "cTestCPU.h"
#include "cWorld.h"

#include "AvidaTools.h"

#include <climits>
#include <fstream>
#include <cmath>

using namespace std;
using namespace Avida;
using namespace AvidaTools;
using namespace Avida::Hardware::InstructionFlags;


StaticTableInstLib<cHardwareCPU::tMethod>* cHardwareCPU::s_inst_slib = cHardwareCPU::initInstLib();

StaticTableInstLib<cHardwareCPU::tMethod>* cHardwareCPU::initInstLib(void)
{
  struct NOPEntry {
    const Apto::String name;
    int nop_mod;
    inline NOPEntry(const Apto::String& name, int nop_mod) : name(name), nop_mod(nop_mod) { ; }
  };
  static const NOPEntry s_n_array[] = {
    NOPEntry("nop-A", REG_AX),
    NOPEntry("nop-B", REG_BX),
    NOPEntry("nop-C", REG_CX)
  };
  
  static const StaticTableInstLib<tMethod>::MethodEntry s_f_array[] = {
    /*
     Note: all entries of NOPEntry s_n_array must have corresponding
     in the same order in StaticTableInstLib<tMethod>::MethodEntry s_f_array, and these entries must
     be the first elements of s_f_array.
     */
    StaticTableInstLib<tMethod>::MethodEntry("nop-A", &cHardwareCPU::Inst_Nop, INST_CLASS_NOP, (DEFAULT | NOP), "No-operation instruction; modifies other instructions"),
    StaticTableInstLib<tMethod>::MethodEntry("nop-B", &cHardwareCPU::Inst_Nop, INST_CLASS_NOP, (DEFAULT | NOP), "No-operation instruction; modifies other instructions"),
    StaticTableInstLib<tMethod>::MethodEntry("nop-C", &cHardwareCPU::Inst_Nop, INST_CLASS_NOP, (DEFAULT | NOP), "No-operation instruction; modifies other instructions"),
    
    StaticTableInstLib<tMethod>::MethodEntry("nop-X", &cHardwareCPU::Inst_Nop, INST_CLASS_NOP, 0, "True no-operation instruction: does nothing"),
    StaticTableInstLib<tMethod>::MethodEntry("if-equ-0", &cHardwareCPU::Inst_If0, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if ?BX?==0, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-not-0", &cHardwareCPU::Inst_IfNot0, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if ?BX?!=0, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-equ-0-defaultAX", &cHardwareCPU::Inst_If0_defaultAX, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if ?AX?==0, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-not-0-defaultAX", &cHardwareCPU::Inst_IfNot0_defaultAX, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if ?AX?!=0, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-n-equ", &cHardwareCPU::Inst_IfNEqu, INST_CLASS_CONDITIONAL, DEFAULT, "Execute next instruction if ?BX?!=?CX?, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-equ", &cHardwareCPU::Inst_IfEqu, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if ?BX?==?CX?, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-grt-0", &cHardwareCPU::Inst_IfGr0, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-grt", &cHardwareCPU::Inst_IfGr, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if->=-0", &cHardwareCPU::Inst_IfGrEqu0, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if->=", &cHardwareCPU::Inst_IfGrEqu, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-les-0", &cHardwareCPU::Inst_IfLess0, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-less", &cHardwareCPU::Inst_IfLess, INST_CLASS_CONDITIONAL, DEFAULT, "Execute next instruction if ?BX? < ?CX?, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-<=-0", &cHardwareCPU::Inst_IfLsEqu0, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-<=", &cHardwareCPU::Inst_IfLsEqu, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-A!=B", &cHardwareCPU::Inst_IfANotEqB, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-B!=C", &cHardwareCPU::Inst_IfBNotEqC, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-A!=C", &cHardwareCPU::Inst_IfANotEqC, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-bit-1", &cHardwareCPU::Inst_IfBit1, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-grt-X", &cHardwareCPU::Inst_IfGrX, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-equ-X", &cHardwareCPU::Inst_IfEquX, INST_CLASS_CONDITIONAL),
		
    // Probabilistic ifs.
    StaticTableInstLib<tMethod>::MethodEntry("if-p-0.125", &cHardwareCPU::Inst_IfP0p125, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-p-0.25", &cHardwareCPU::Inst_IfP0p25, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-p-0.50", &cHardwareCPU::Inst_IfP0p50, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-p-0.75", &cHardwareCPU::Inst_IfP0p75, INST_CLASS_CONDITIONAL),
    
    // The below series of conditionals extend the traditional Avida single-instruction-skip
    // to a block, or series of instructions.
    StaticTableInstLib<tMethod>::MethodEntry("if-less.end", &cHardwareCPU::Inst_IfLessEnd, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if-n-equ.end", &cHardwareCPU::Inst_IfNotEqualEnd, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("if->=.end", &cHardwareCPU::Inst_IfGrtEquEnd, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("else", &cHardwareCPU::Inst_Else, INST_CLASS_CONDITIONAL),
    StaticTableInstLib<tMethod>::MethodEntry("end-if", &cHardwareCPU::Inst_EndIf, INST_CLASS_CONDITIONAL),
    
    StaticTableInstLib<tMethod>::MethodEntry("jump-f", &cHardwareCPU::Inst_JumpF, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("jump-b", &cHardwareCPU::Inst_JumpB, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("call", &cHardwareCPU::Inst_Call, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("return", &cHardwareCPU::Inst_Return, INST_CLASS_FLOW_CONTROL),
    
    StaticTableInstLib<tMethod>::MethodEntry("throw", &cHardwareCPU::Inst_Throw, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("throwif=0", &cHardwareCPU::Inst_ThrowIf0, INST_CLASS_FLOW_CONTROL),    
    StaticTableInstLib<tMethod>::MethodEntry("throwif!=0", &cHardwareCPU::Inst_ThrowIfNot0, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("catch", &cHardwareCPU::Inst_Catch, INST_CLASS_FLOW_CONTROL),
    
    StaticTableInstLib<tMethod>::MethodEntry("goto", &cHardwareCPU::Inst_Goto, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("goto-if=0", &cHardwareCPU::Inst_GotoIf0, INST_CLASS_FLOW_CONTROL),    
    StaticTableInstLib<tMethod>::MethodEntry("goto-if!=0", &cHardwareCPU::Inst_GotoIfNot0, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("label", &cHardwareCPU::Inst_Label, INST_CLASS_FLOW_CONTROL),
    
    StaticTableInstLib<tMethod>::MethodEntry("pop", &cHardwareCPU::Inst_Pop, INST_CLASS_DATA, DEFAULT, "Remove top number from stack and place into ?BX?"),
    StaticTableInstLib<tMethod>::MethodEntry("push", &cHardwareCPU::Inst_Push, INST_CLASS_DATA, DEFAULT, "Copy number from ?BX? and place it into the stack"),
    StaticTableInstLib<tMethod>::MethodEntry("swap-stk", &cHardwareCPU::Inst_SwitchStack, INST_CLASS_DATA, DEFAULT, "Toggle which stack is currently being used"),
    StaticTableInstLib<tMethod>::MethodEntry("swap", &cHardwareCPU::Inst_Swap, INST_CLASS_DATA, DEFAULT, "Swap the contents of ?BX? with ?CX?"),
    StaticTableInstLib<tMethod>::MethodEntry("swap-AB", &cHardwareCPU::Inst_SwapAB, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("swap-BC", &cHardwareCPU::Inst_SwapBC, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("swap-AC", &cHardwareCPU::Inst_SwapAC, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("copy-reg", &cHardwareCPU::Inst_CopyReg, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("set_A=B", &cHardwareCPU::Inst_CopyRegAB, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("set_A=C", &cHardwareCPU::Inst_CopyRegAC, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("set_B=A", &cHardwareCPU::Inst_CopyRegBA, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("set_B=C", &cHardwareCPU::Inst_CopyRegBC, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("set_C=A", &cHardwareCPU::Inst_CopyRegCA, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("set_C=B", &cHardwareCPU::Inst_CopyRegCB, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("reset", &cHardwareCPU::Inst_Reset, INST_CLASS_DATA),
    
    StaticTableInstLib<tMethod>::MethodEntry("pop-A", &cHardwareCPU::Inst_PopA, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("pop-B", &cHardwareCPU::Inst_PopB, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("pop-C", &cHardwareCPU::Inst_PopC, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("push-A", &cHardwareCPU::Inst_PushA, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("push-B", &cHardwareCPU::Inst_PushB, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("push-C", &cHardwareCPU::Inst_PushC, INST_CLASS_DATA),
    
    StaticTableInstLib<tMethod>::MethodEntry("shift-r", &cHardwareCPU::Inst_ShiftR, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Shift bits in ?BX? right by one (divide by two)"),
    StaticTableInstLib<tMethod>::MethodEntry("shift-l", &cHardwareCPU::Inst_ShiftL, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Shift bits in ?BX? left by one (multiply by two)"),
    StaticTableInstLib<tMethod>::MethodEntry("bit-1", &cHardwareCPU::Inst_Bit1, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("set-num", &cHardwareCPU::Inst_SetNum, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("val-grey", &cHardwareCPU::Inst_ValGrey, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("val-dir", &cHardwareCPU::Inst_ValDir, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("val-add-p", &cHardwareCPU::Inst_ValAddP, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("val-fib", &cHardwareCPU::Inst_ValFib, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("val-poly-c", &cHardwareCPU::Inst_ValPolyC, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("inc", &cHardwareCPU::Inst_Inc, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Increment ?BX? by one"),
    StaticTableInstLib<tMethod>::MethodEntry("dec", &cHardwareCPU::Inst_Dec, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Decrement ?BX? by one"),
    StaticTableInstLib<tMethod>::MethodEntry("zero", &cHardwareCPU::Inst_Zero, INST_CLASS_ARITHMETIC_LOGIC, 0, "Set ?BX? to zero"),
    StaticTableInstLib<tMethod>::MethodEntry("one", &cHardwareCPU::Inst_One, INST_CLASS_ARITHMETIC_LOGIC, 0, "Set ?BX? to one"),
    StaticTableInstLib<tMethod>::MethodEntry("all1s", &cHardwareCPU::Inst_All1s, INST_CLASS_ARITHMETIC_LOGIC, 0, "Set ?BX? to all 1s in bitstring"),
    StaticTableInstLib<tMethod>::MethodEntry("neg", &cHardwareCPU::Inst_Neg, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("square", &cHardwareCPU::Inst_Square, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("sqrt", &cHardwareCPU::Inst_Sqrt, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("not", &cHardwareCPU::Inst_Not, INST_CLASS_ARITHMETIC_LOGIC),
    
    StaticTableInstLib<tMethod>::MethodEntry("add", &cHardwareCPU::Inst_Add, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Add BX to CX and place the result in ?BX?"),
    StaticTableInstLib<tMethod>::MethodEntry("sub", &cHardwareCPU::Inst_Sub, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Subtract CX from BX and place the result in ?BX?"),
    StaticTableInstLib<tMethod>::MethodEntry("mult", &cHardwareCPU::Inst_Mult, INST_CLASS_ARITHMETIC_LOGIC, 0, "Multiple BX by CX and place the result in ?BX?"),
    StaticTableInstLib<tMethod>::MethodEntry("div", &cHardwareCPU::Inst_Div, INST_CLASS_ARITHMETIC_LOGIC, 0, "Divide BX by CX and place the result in ?BX?"),
    StaticTableInstLib<tMethod>::MethodEntry("mod", &cHardwareCPU::Inst_Mod, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("nand", &cHardwareCPU::Inst_Nand, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Nand BX by CX and place the result in ?BX?"),
    StaticTableInstLib<tMethod>::MethodEntry("or", &cHardwareCPU::Inst_Or, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("nor", &cHardwareCPU::Inst_Nor, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("and", &cHardwareCPU::Inst_And, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("order", &cHardwareCPU::Inst_Order, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("xor", &cHardwareCPU::Inst_Xor, INST_CLASS_ARITHMETIC_LOGIC),
    
    // Instructions that modify specific bits in the register values
    StaticTableInstLib<tMethod>::MethodEntry("setbit", &cHardwareCPU::Inst_Setbit, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Set the bit in ?BX? specified by ?BX?'s complement"),
    StaticTableInstLib<tMethod>::MethodEntry("clearbit", &cHardwareCPU::Inst_Clearbit, INST_CLASS_ARITHMETIC_LOGIC, DEFAULT, "Clear the bit in ?BX? specified by ?BX?'s complement"),
    
    StaticTableInstLib<tMethod>::MethodEntry("copy", &cHardwareCPU::Inst_Copy, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("read", &cHardwareCPU::Inst_ReadInst, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("write", &cHardwareCPU::Inst_WriteInst, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("stk-read", &cHardwareCPU::Inst_StackReadInst, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("stk-writ", &cHardwareCPU::Inst_StackWriteInst, INST_CLASS_LIFECYCLE),
    
    StaticTableInstLib<tMethod>::MethodEntry("compare", &cHardwareCPU::Inst_Compare),
    StaticTableInstLib<tMethod>::MethodEntry("if-n-cpy", &cHardwareCPU::Inst_IfNCpy),
    StaticTableInstLib<tMethod>::MethodEntry("allocate", &cHardwareCPU::Inst_Allocate, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("divide", &cHardwareCPU::Inst_Divide, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("divideRS", &cHardwareCPU::Inst_DivideRS, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("c-alloc", &cHardwareCPU::Inst_CAlloc, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("c-divide", &cHardwareCPU::Inst_CDivide, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("transposon", &cHardwareCPU::Inst_Transposon, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("search-f", &cHardwareCPU::Inst_SearchF, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("search-b", &cHardwareCPU::Inst_SearchB, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("mem-size", &cHardwareCPU::Inst_MemSize),
    
    StaticTableInstLib<tMethod>::MethodEntry("get", &cHardwareCPU::Inst_TaskGet, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("get-2", &cHardwareCPU::Inst_TaskGet2, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("stk-get", &cHardwareCPU::Inst_TaskStackGet, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("stk-load", &cHardwareCPU::Inst_TaskStackLoad, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("put", &cHardwareCPU::Inst_TaskPut, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("put-reset", &cHardwareCPU::Inst_TaskPutResetInputs, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("IO", &cHardwareCPU::Inst_TaskIO, INST_CLASS_ENVIRONMENT, DEFAULT | STALL, "Output ?BX?, and input new number back into ?BX?"),
    StaticTableInstLib<tMethod>::MethodEntry("IO-Feedback", &cHardwareCPU::Inst_TaskIO_Feedback, INST_CLASS_ENVIRONMENT, STALL, "Output ?BX?, and input new number back into ?BX?,  and push 1,0,  or -1 onto stack1 if merit increased, stayed the same, or decreased"),
    StaticTableInstLib<tMethod>::MethodEntry("IO-bc-0.001", &cHardwareCPU::Inst_TaskIO_BonusCost_0_001, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("match-strings", &cHardwareCPU::Inst_MatchStrings, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("send", &cHardwareCPU::Inst_Send, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("receive", &cHardwareCPU::Inst_Receive, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense", &cHardwareCPU::Inst_SenseLog2, INST_CLASS_ENVIRONMENT, STALL),           // If you add more sense instructions
    StaticTableInstLib<tMethod>::MethodEntry("sense-unit", &cHardwareCPU::Inst_SenseUnit, INST_CLASS_ENVIRONMENT, STALL),      // and want to keep stats, also add
    StaticTableInstLib<tMethod>::MethodEntry("sense-m100", &cHardwareCPU::Inst_SenseMult100, INST_CLASS_ENVIRONMENT, STALL),   // the names to cStats::cStats() @JEB
    StaticTableInstLib<tMethod>::MethodEntry("sense-resource-id", &cHardwareCPU::Inst_SenseResourceID, INST_CLASS_ENVIRONMENT, STALL), 
    StaticTableInstLib<tMethod>::MethodEntry("sense-opinion-resource-quantity", &cHardwareCPU::Inst_SenseOpinionResourceQuantity, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense-next-res-level", &cHardwareCPU::Inst_SenseNextResLevel, INST_CLASS_ENVIRONMENT, STALL), // @JJB
    StaticTableInstLib<tMethod>::MethodEntry("sense-diff-faced", &cHardwareCPU::Inst_SenseDiffFaced, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense-faced-habitat", &cHardwareCPU::Inst_SenseFacedHabitat, INST_CLASS_ENVIRONMENT, STALL),
    
    StaticTableInstLib<tMethod>::MethodEntry("sense-resource0", &cHardwareCPU::Inst_SenseResource0, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense-resource1", &cHardwareCPU::Inst_SenseResource1, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense-resource2", &cHardwareCPU::Inst_SenseResource2, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense-faced-resource0", &cHardwareCPU::Inst_SenseFacedResource0, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense-faced-resource1", &cHardwareCPU::Inst_SenseFacedResource1, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("sense-faced-resource2", &cHardwareCPU::Inst_SenseFacedResource2, INST_CLASS_ENVIRONMENT, STALL),
    
    StaticTableInstLib<tMethod>::MethodEntry("if-resources", &cHardwareCPU::Inst_IfResources, INST_CLASS_CONDITIONAL, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("collect", &cHardwareCPU::Inst_Collect, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("collect-no-env-remove", &cHardwareCPU::Inst_CollectNoEnvRemove, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("destroy", &cHardwareCPU::Inst_Destroy, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("nop-collect", &cHardwareCPU::Inst_NopCollect, INST_CLASS_ENVIRONMENT),
    StaticTableInstLib<tMethod>::MethodEntry("collect-unit-prob", &cHardwareCPU::Inst_CollectUnitProbabilistic, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("collect-specific", &cHardwareCPU::Inst_CollectSpecific, INST_CLASS_ENVIRONMENT, STALL),

    StaticTableInstLib<tMethod>::MethodEntry("donate-rnd", &cHardwareCPU::Inst_DonateRandom),
    StaticTableInstLib<tMethod>::MethodEntry("donate-kin", &cHardwareCPU::Inst_DonateKin),
    StaticTableInstLib<tMethod>::MethodEntry("donate-edt", &cHardwareCPU::Inst_DonateEditDist),
    StaticTableInstLib<tMethod>::MethodEntry("get-faced-edit-dist", &cHardwareCPU::Inst_GetFacedEditDistance),
    StaticTableInstLib<tMethod>::MethodEntry("donate-gbg",  &cHardwareCPU::Inst_DonateGreenBeardGene),
    StaticTableInstLib<tMethod>::MethodEntry("donate-tgb",  &cHardwareCPU::Inst_DonateTrueGreenBeard),
    StaticTableInstLib<tMethod>::MethodEntry("donate-shadedgb",  &cHardwareCPU::Inst_DonateShadedGreenBeard),
    StaticTableInstLib<tMethod>::MethodEntry("donate-threshgb",  &cHardwareCPU::Inst_DonateThreshGreenBeard),
    StaticTableInstLib<tMethod>::MethodEntry("donate-quantagb",  &cHardwareCPU::Inst_DonateQuantaThreshGreenBeard),
    StaticTableInstLib<tMethod>::MethodEntry("donate-gbsl",  &cHardwareCPU::Inst_DonateGreenBeardSameLocus),
    StaticTableInstLib<tMethod>::MethodEntry("donate-NUL", &cHardwareCPU::Inst_DonateNULL),
    StaticTableInstLib<tMethod>::MethodEntry("donate-resource0", &cHardwareCPU::Inst_DonateResource0, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("donate-resource1", &cHardwareCPU::Inst_DonateResource1, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("donate-resource2", &cHardwareCPU::Inst_DonateResource2, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("IObuf-add1", &cHardwareCPU::Inst_IOBufAdd1, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("IObuf-add0", &cHardwareCPU::Inst_IOBufAdd0, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("donate-specific", &cHardwareCPU::Inst_DonateSpecific, INST_CLASS_ENVIRONMENT, STALL),
    
    StaticTableInstLib<tMethod>::MethodEntry("rotate-l", &cHardwareCPU::Inst_RotateL, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-r", &cHardwareCPU::Inst_RotateR, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-left-one", &cHardwareCPU::Inst_RotateLeftOne, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-right-one", &cHardwareCPU::Inst_RotateRightOne, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-label", &cHardwareCPU::Inst_RotateLabel, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-unoccupied-cell", &cHardwareCPU::Inst_RotateUnoccupiedCell, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-next-unoccupied-cell", &cHardwareCPU::Inst_RotateNextUnoccupiedCell, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-occupied-cell", &cHardwareCPU::Inst_RotateOccupiedCell, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-next-occupied-cell", &cHardwareCPU::Inst_RotateNextOccupiedCell, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-event-cell", &cHardwareCPU::Inst_RotateEventCell, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-uphill", &cHardwareCPU::Inst_RotateUphill, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-home", &cHardwareCPU::Inst_RotateHome, INST_CLASS_ENVIRONMENT, STALL),
    
    StaticTableInstLib<tMethod>::MethodEntry("set-cmut", &cHardwareCPU::Inst_SetCopyMut),
    StaticTableInstLib<tMethod>::MethodEntry("mod-cmut", &cHardwareCPU::Inst_ModCopyMut),
    StaticTableInstLib<tMethod>::MethodEntry("get-cell-xy", &cHardwareCPU::Inst_GetCellPosition),
    StaticTableInstLib<tMethod>::MethodEntry("get-cell-x", &cHardwareCPU::Inst_GetCellPositionX),
    StaticTableInstLib<tMethod>::MethodEntry("get-cell-y", &cHardwareCPU::Inst_GetCellPositionY),
    StaticTableInstLib<tMethod>::MethodEntry("dist-from-diag", &cHardwareCPU::Inst_GetDistanceFromDiagonal),
    StaticTableInstLib<tMethod>::MethodEntry("get-north-offset", &cHardwareCPU::Inst_GetDirectionOffNorth),    
    StaticTableInstLib<tMethod>::MethodEntry("get-northerly", &cHardwareCPU::Inst_GetNortherly),    
    StaticTableInstLib<tMethod>::MethodEntry("get-easterly", &cHardwareCPU::Inst_GetEasterly), 
    StaticTableInstLib<tMethod>::MethodEntry("zero-easterly", &cHardwareCPU::Inst_ZeroEasterly),    
    StaticTableInstLib<tMethod>::MethodEntry("zero-northerly", &cHardwareCPU::Inst_ZeroNortherly),    
    
    
    // State Grid instructions
    StaticTableInstLib<tMethod>::MethodEntry("sg-move", &cHardwareCPU::Inst_SGMove),
    StaticTableInstLib<tMethod>::MethodEntry("sg-rotate-l", &cHardwareCPU::Inst_SGRotateL),
    StaticTableInstLib<tMethod>::MethodEntry("sg-rotate-r", &cHardwareCPU::Inst_SGRotateR),
    StaticTableInstLib<tMethod>::MethodEntry("sg-sense", &cHardwareCPU::Inst_SGSense),
    
    
    
    // Movement instructions
    StaticTableInstLib<tMethod>::MethodEntry("tumble", &cHardwareCPU::Inst_Tumble, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("move", &cHardwareCPU::Inst_Move, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("move-to-event", &cHardwareCPU::Inst_MoveToEvent, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("if-event-in-unoccupied-neighbor-cell", &cHardwareCPU::Inst_IfNeighborEventInUnoccupiedCell),
    StaticTableInstLib<tMethod>::MethodEntry("if-event-in-faced-cell", &cHardwareCPU::Inst_IfFacingEventCell),
    StaticTableInstLib<tMethod>::MethodEntry("if-event-in-current-cell", &cHardwareCPU::Inst_IfEventInCell),
    
    // Threading instructions
    StaticTableInstLib<tMethod>::MethodEntry("fork-th", &cHardwareCPU::Inst_ForkThread),
    StaticTableInstLib<tMethod>::MethodEntry("forkl", &cHardwareCPU::Inst_ForkThreadLabel),
    StaticTableInstLib<tMethod>::MethodEntry("forkl!=0", &cHardwareCPU::Inst_ForkThreadLabelIfNot0),
    StaticTableInstLib<tMethod>::MethodEntry("forkl=0", &cHardwareCPU::Inst_ForkThreadLabelIf0),
    StaticTableInstLib<tMethod>::MethodEntry("kill-th", &cHardwareCPU::Inst_KillThread),
    StaticTableInstLib<tMethod>::MethodEntry("id-th", &cHardwareCPU::Inst_ThreadID),
    
    // Head-based instructions
    StaticTableInstLib<tMethod>::MethodEntry("h-alloc", &cHardwareCPU::Inst_MaxAlloc, INST_CLASS_LIFECYCLE, DEFAULT, "Allocate maximum allowed space"),
    StaticTableInstLib<tMethod>::MethodEntry("h-alloc-mw", &cHardwareCPU::Inst_MaxAllocMoveWriteHead),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide", &cHardwareCPU::Inst_HeadDivide, INST_CLASS_LIFECYCLE, DEFAULT | STALL, "Divide code between read and write heads."),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide1RS", &cHardwareCPU::Inst_HeadDivide1RS, INST_CLASS_LIFECYCLE, STALL, "Divide code between read and write heads, at most one mutation on divide, resample if reverted."),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide2RS", &cHardwareCPU::Inst_HeadDivide2RS, INST_CLASS_LIFECYCLE, STALL, "Divide code between read and write heads, at most two mutations on divide, resample if reverted."),
    StaticTableInstLib<tMethod>::MethodEntry("h-divideRS", &cHardwareCPU::Inst_HeadDivideRS, INST_CLASS_LIFECYCLE, STALL, "Divide code between read and write heads, resample if reverted."),
    StaticTableInstLib<tMethod>::MethodEntry("h-read", &cHardwareCPU::Inst_HeadRead, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("h-write", &cHardwareCPU::Inst_HeadWrite, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy", &cHardwareCPU::Inst_HeadCopy, INST_CLASS_LIFECYCLE, DEFAULT, "Copy from read-head to write-head; advance both"),
    StaticTableInstLib<tMethod>::MethodEntry("h-search", &cHardwareCPU::Inst_HeadSearch, INST_CLASS_FLOW_CONTROL, DEFAULT, "Find complement template and make with flow head"),
    StaticTableInstLib<tMethod>::MethodEntry("h-search-direct", &cHardwareCPU::Inst_HeadSearchDirect, INST_CLASS_FLOW_CONTROL, 0, "Find direct template and move the flow head"),
    StaticTableInstLib<tMethod>::MethodEntry("h-push", &cHardwareCPU::Inst_HeadPush, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("h-pop", &cHardwareCPU::Inst_HeadPop, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("set-head", &cHardwareCPU::Inst_SetHead, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("adv-head", &cHardwareCPU::Inst_AdvanceHead, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("mov-head", &cHardwareCPU::Inst_MoveHead, INST_CLASS_FLOW_CONTROL, DEFAULT, "Move head ?IP? to the flow head"),
    StaticTableInstLib<tMethod>::MethodEntry("jmp-head", &cHardwareCPU::Inst_JumpHead, INST_CLASS_FLOW_CONTROL, DEFAULT, "Move head ?IP? by amount in CX register; CX = old pos."),
    StaticTableInstLib<tMethod>::MethodEntry("get-head", &cHardwareCPU::Inst_GetHead, INST_CLASS_FLOW_CONTROL, DEFAULT, "Copy the position of the ?IP? head into CX"),
    StaticTableInstLib<tMethod>::MethodEntry("if-label", &cHardwareCPU::Inst_IfLabel, INST_CLASS_CONDITIONAL, DEFAULT, "Execute next if we copied complement of attached label"),
    StaticTableInstLib<tMethod>::MethodEntry("if-label-direct", &cHardwareCPU::Inst_IfLabelDirect, INST_CLASS_CONDITIONAL, DEFAULT, "Execute next if we copied direct match of the attached label"),
    StaticTableInstLib<tMethod>::MethodEntry("if-label2", &cHardwareCPU::Inst_IfLabel2, INST_CLASS_CONDITIONAL, 0, "If copied label compl., exec next inst; else SKIP W/NOPS"),
    StaticTableInstLib<tMethod>::MethodEntry("set-flow", &cHardwareCPU::Inst_SetFlow, INST_CLASS_FLOW_CONTROL, DEFAULT, "Set flow-head to position in ?CX?"),
    
    StaticTableInstLib<tMethod>::MethodEntry("h-copy2", &cHardwareCPU::Inst_HeadCopy2),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy3", &cHardwareCPU::Inst_HeadCopy3),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy4", &cHardwareCPU::Inst_HeadCopy4),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy5", &cHardwareCPU::Inst_HeadCopy5),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy6", &cHardwareCPU::Inst_HeadCopy6),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy7", &cHardwareCPU::Inst_HeadCopy7),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy8", &cHardwareCPU::Inst_HeadCopy8),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy9", &cHardwareCPU::Inst_HeadCopy9),
    StaticTableInstLib<tMethod>::MethodEntry("h-copy10", &cHardwareCPU::Inst_HeadCopy10),
    
    StaticTableInstLib<tMethod>::MethodEntry("divide-sex", &cHardwareCPU::Inst_HeadDivideSex, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("divide-asex", &cHardwareCPU::Inst_HeadDivideAsex, INST_CLASS_LIFECYCLE, STALL),
    
    StaticTableInstLib<tMethod>::MethodEntry("div-sex", &cHardwareCPU::Inst_HeadDivideSex, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("div-asex", &cHardwareCPU::Inst_HeadDivideAsex, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("div-asex-w", &cHardwareCPU::Inst_HeadDivideAsexWait, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("div-sex-MS", &cHardwareCPU::Inst_HeadDivideMateSelect, INST_CLASS_LIFECYCLE, STALL),
    
    StaticTableInstLib<tMethod>::MethodEntry("h-divide1", &cHardwareCPU::Inst_HeadDivide1, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide2", &cHardwareCPU::Inst_HeadDivide2, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide3", &cHardwareCPU::Inst_HeadDivide3, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide4", &cHardwareCPU::Inst_HeadDivide4, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide5", &cHardwareCPU::Inst_HeadDivide5, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide6", &cHardwareCPU::Inst_HeadDivide6, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide7", &cHardwareCPU::Inst_HeadDivide7, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide8", &cHardwareCPU::Inst_HeadDivide8, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide9", &cHardwareCPU::Inst_HeadDivide9, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide10", &cHardwareCPU::Inst_HeadDivide10, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide16", &cHardwareCPU::Inst_HeadDivide16, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide32", &cHardwareCPU::Inst_HeadDivide32, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide50", &cHardwareCPU::Inst_HeadDivide50, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide100", &cHardwareCPU::Inst_HeadDivide100, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide500", &cHardwareCPU::Inst_HeadDivide500, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide1000", &cHardwareCPU::Inst_HeadDivide1000, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide5000", &cHardwareCPU::Inst_HeadDivide5000, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide10000", &cHardwareCPU::Inst_HeadDivide10000, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide50000", &cHardwareCPU::Inst_HeadDivide50000, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide0.5", &cHardwareCPU::Inst_HeadDivide0_5, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide0.1", &cHardwareCPU::Inst_HeadDivide0_1, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide0.05", &cHardwareCPU::Inst_HeadDivide0_05, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide0.01", &cHardwareCPU::Inst_HeadDivide0_01, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("h-divide0.001", &cHardwareCPU::Inst_HeadDivide0_001, INST_CLASS_LIFECYCLE, STALL),
    
    //@CHC Mating type / mate choice instructions
    StaticTableInstLib<tMethod>::MethodEntry("set-mating-type-male", &cHardwareCPU::Inst_SetMatingTypeMale, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mating-type-female", &cHardwareCPU::Inst_SetMatingTypeFemale, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mating-type-juvenile", &cHardwareCPU::Inst_SetMatingTypeJuvenile, INST_CLASS_LIFECYCLE), 
    StaticTableInstLib<tMethod>::MethodEntry("div-sex-mating-type", &cHardwareCPU::Inst_DivideSexMatingType, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("if-mating-type-male", &cHardwareCPU::Inst_IfMatingTypeMale, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("if-mating-type-female", &cHardwareCPU::Inst_IfMatingTypeFemale, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("if-mating-type-juvenile", &cHardwareCPU::Inst_IfMatingTypeJuvenile, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("increment-mating-display-a", &cHardwareCPU::Inst_IncrementMatingDisplayA, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("increment-mating-display-b", &cHardwareCPU::Inst_IncrementMatingDisplayB, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mating-display-a", &cHardwareCPU::Inst_SetMatingDisplayA, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mating-display-b", &cHardwareCPU::Inst_SetMatingDisplayB, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mate-preference-random", &cHardwareCPU::Inst_SetMatePreferenceRandom, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mate-preference-highest-display-a", &cHardwareCPU::Inst_SetMatePreferenceHighestDisplayA, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mate-preference-highest-display-b", &cHardwareCPU::Inst_SetMatePreferenceHighestDisplayB, INST_CLASS_LIFECYCLE),
    StaticTableInstLib<tMethod>::MethodEntry("set-mate-preference-highest-merit", &cHardwareCPU::Inst_SetMatePreferenceHighestMerit, INST_CLASS_LIFECYCLE),
    
    
    // High-level instructions
    StaticTableInstLib<tMethod>::MethodEntry("repro", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-sex", &cHardwareCPU::Inst_ReproSex, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-A", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-B", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-C", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-D", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-E", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-F", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-G", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-H", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-I", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-J", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-K", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-L", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-M", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-N", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-O", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-P", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-Q", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-R", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-S", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-T", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-U", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-V", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-W", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-X", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-Y", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repro-Z", &cHardwareCPU::Inst_Repro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("cond-repro", &cHardwareCPU::Inst_ConditionalRepro, INST_CLASS_LIFECYCLE, STALL),
    
    StaticTableInstLib<tMethod>::MethodEntry("put-repro", &cHardwareCPU::Inst_TaskPutRepro, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("metabolize", &cHardwareCPU::Inst_TaskPutResetInputsRepro, INST_CLASS_LIFECYCLE, STALL),        
    
    StaticTableInstLib<tMethod>::MethodEntry("sterilize", &cHardwareCPU::Inst_Sterilize, INST_CLASS_LIFECYCLE),
    
    // Suicide
    StaticTableInstLib<tMethod>::MethodEntry("kazi",	&cHardwareCPU::Inst_Kazi, INST_CLASS_OTHER, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("kazi1", &cHardwareCPU::Inst_Kazi1, INST_CLASS_OTHER, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("kazi2", &cHardwareCPU::Inst_Kazi2, INST_CLASS_OTHER, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("kazi3", &cHardwareCPU::Inst_Kazi3, INST_CLASS_OTHER, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("kazi4", &cHardwareCPU::Inst_Kazi4, INST_CLASS_OTHER, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("kazi5", &cHardwareCPU::Inst_Kazi5, INST_CLASS_OTHER, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("die", &cHardwareCPU::Inst_Die, INST_CLASS_OTHER, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("poison", &cHardwareCPU::Inst_Poison),
    StaticTableInstLib<tMethod>::MethodEntry("suicide", &cHardwareCPU::Inst_Suicide, INST_CLASS_OTHER, STALL),		
    
    // Promoter Model
    StaticTableInstLib<tMethod>::MethodEntry("promoter", &cHardwareCPU::Inst_Promoter, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("terminate", &cHardwareCPU::Inst_Terminate, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("regulate", &cHardwareCPU::Inst_Regulate, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("regulate-sp", &cHardwareCPU::Inst_RegulateSpecificPromoters, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("s-regulate", &cHardwareCPU::Inst_SenseRegulate, INST_CLASS_FLOW_CONTROL),
    StaticTableInstLib<tMethod>::MethodEntry("numberate", &cHardwareCPU::Inst_Numberate, INST_CLASS_DATA),
    StaticTableInstLib<tMethod>::MethodEntry("numberate-24", &cHardwareCPU::Inst_Numberate24, INST_CLASS_DATA),
    
    // Bit Consensus
    StaticTableInstLib<tMethod>::MethodEntry("bit-cons", &cHardwareCPU::Inst_BitConsensus, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("bit-cons-24", &cHardwareCPU::Inst_BitConsensus24, INST_CLASS_ARITHMETIC_LOGIC),
    StaticTableInstLib<tMethod>::MethodEntry("if-cons", &cHardwareCPU::Inst_IfConsensus, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if ?BX? in consensus, else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-cons-24", &cHardwareCPU::Inst_IfConsensus24, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if ?BX[0:23]? in consensus , else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-less-cons", &cHardwareCPU::Inst_IfLessConsensus, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if Count(?BX?) < Count(?CX?), else skip it"),
    StaticTableInstLib<tMethod>::MethodEntry("if-less-cons-24", &cHardwareCPU::Inst_IfLessConsensus24, INST_CLASS_CONDITIONAL, 0, "Execute next instruction if Count(?BX[0:23]?) < Count(?CX[0:23]?), else skip it"),
    
    // Bit Masking (higher order bit masking is possible, just add the instructions if needed)
    StaticTableInstLib<tMethod>::MethodEntry("mask-signbit", &cHardwareCPU::Inst_MaskSignBit),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower16bits", &cHardwareCPU::Inst_MaskOffLower16Bits),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower16bits-defaultAX", &cHardwareCPU::Inst_MaskOffLower16Bits_defaultAX),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower15bits", &cHardwareCPU::Inst_MaskOffLower15Bits),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower15bits-defaultAX", &cHardwareCPU::Inst_MaskOffLower15Bits_defaultAX),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower14bits", &cHardwareCPU::Inst_MaskOffLower14Bits),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower14bits-defaultAX", &cHardwareCPU::Inst_MaskOffLower14Bits_defaultAX),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower13bits", &cHardwareCPU::Inst_MaskOffLower13Bits),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower13bits-defaultAX", &cHardwareCPU::Inst_MaskOffLower13Bits_defaultAX),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower12bits", &cHardwareCPU::Inst_MaskOffLower12Bits),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower12bits-defaultAX", &cHardwareCPU::Inst_MaskOffLower12Bits_defaultAX),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower8bits",  &cHardwareCPU::Inst_MaskOffLower8Bits),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower8bits-defaultAX",  &cHardwareCPU::Inst_MaskOffLower8Bits_defaultAX),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower4bits",  &cHardwareCPU::Inst_MaskOffLower4Bits),
    StaticTableInstLib<tMethod>::MethodEntry("maskoff-lower4bits-defaultAX",  &cHardwareCPU::Inst_MaskOffLower4Bits_defaultAX),
		
    
    // Placebo instructions
    StaticTableInstLib<tMethod>::MethodEntry("skip", &cHardwareCPU::Inst_Skip),
    
    
    // Opinion instructions.
    // These are STALLs because opinions are only relevant with respect to time.
    StaticTableInstLib<tMethod>::MethodEntry("set-opinion", &cHardwareCPU::Inst_SetOpinion, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("get-opinion", &cHardwareCPU::Inst_GetOpinion, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("get-opinionOnly", &cHardwareCPU::Inst_GetOpinionOnly_ZeroIfNone, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("clear-opinion", &cHardwareCPU::Inst_ClearOpinion, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("if-opinion-set", &cHardwareCPU::Inst_IfOpinionSet, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("if-opinion-notset", &cHardwareCPU::Inst_IfOpinionNotSet, INST_CLASS_ENVIRONMENT, STALL),
    
    // Data collection
    StaticTableInstLib<tMethod>::MethodEntry("if-cell-data-changed", &cHardwareCPU::Inst_IfCellDataChanged, INST_CLASS_CONDITIONAL, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("collect-cell-data", &cHardwareCPU::Inst_CollectCellData, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("read-cell-data", &cHardwareCPU::Inst_ReadCellData, INST_CLASS_ENVIRONMENT),
    StaticTableInstLib<tMethod>::MethodEntry("read-faced-cell-data", &cHardwareCPU::Inst_ReadFacedCellData, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("read-faced-cell-org-id", &cHardwareCPU::Inst_ReadFacedCellDataOrgID, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("read-faced-cell-freshness", &cHardwareCPU::Inst_ReadFacedCellDataFreshness, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("mark-cell-with-id", &cHardwareCPU::Inst_MarkCellWithID, INST_CLASS_ENVIRONMENT),
    StaticTableInstLib<tMethod>::MethodEntry("mark-cell-with-vitality", &cHardwareCPU::Inst_MarkCellWithVitality, INST_CLASS_ENVIRONMENT),
    StaticTableInstLib<tMethod>::MethodEntry("get-res-stored", &cHardwareCPU::Inst_GetResStored, INST_CLASS_ENVIRONMENT),
    StaticTableInstLib<tMethod>::MethodEntry("get-id", &cHardwareCPU::Inst_GetID, INST_CLASS_ENVIRONMENT),
    StaticTableInstLib<tMethod>::MethodEntry("get-faced-vitality-diff", &cHardwareCPU::Inst_GetFacedVitalityDiff, INST_CLASS_ENVIRONMENT, STALL), 
    StaticTableInstLib<tMethod>::MethodEntry("get-faced-org-id", &cHardwareCPU::Inst_GetFacedOrgID, INST_CLASS_ENVIRONMENT, STALL), 
    StaticTableInstLib<tMethod>::MethodEntry("attack-faced-org", &cHardwareCPU::Inst_AttackFacedOrg, INST_CLASS_ENVIRONMENT, STALL), 
    StaticTableInstLib<tMethod>::MethodEntry("get-attack-odds", &cHardwareCPU::Inst_GetAttackOdds, INST_CLASS_ENVIRONMENT, STALL),     
		
    // Neighborhood-sensing instructions
    StaticTableInstLib<tMethod>::MethodEntry("get-neighborhood", &cHardwareCPU::Inst_GetNeighborhood, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("if-neighborhood-changed", &cHardwareCPU::Inst_IfNeighborhoodChanged, INST_CLASS_CONDITIONAL, STALL),
		
		
    // Reputation instructions
    
    StaticTableInstLib<tMethod>::MethodEntry("donate-frm", &cHardwareCPU::Inst_DonateFacingRawMaterials, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("donate-spec", &cHardwareCPU::Inst_DonateFacingRawMaterialsOtherSpecies, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("donate-if-donor", &cHardwareCPU::Inst_DonateIfDonor, INST_CLASS_ENVIRONMENT, STALL),		
    StaticTableInstLib<tMethod>::MethodEntry("donate-string", &cHardwareCPU::Inst_DonateFacingString, INST_CLASS_ENVIRONMENT, STALL),		
		
    StaticTableInstLib<tMethod>::MethodEntry("get-neighbors-reputation", &cHardwareCPU::Inst_GetNeighborsReputation, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("get-reputation", &cHardwareCPU::Inst_GetReputation, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("get-raw-mat-amount", &cHardwareCPU::Inst_GetAmountOfRawMaterials, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("get-other-raw-mat-amount", &cHardwareCPU::Inst_GetAmountOfOtherRawMaterials, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("pose", &cHardwareCPU::Inst_Pose, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-rep", &cHardwareCPU::Inst_RotateToGreatestReputation, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-rep-and-donate", &cHardwareCPU::Inst_RotateToGreatestReputationAndDonate, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-rep-tag", &cHardwareCPU::Inst_RotateToGreatestReputationWithDifferentTag, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-rep-lineage", &cHardwareCPU::Inst_RotateToGreatestReputationWithDifferentLineage, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("rotate-to-tag", &cHardwareCPU::Inst_RotateToDifferentTag, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("if-donor",  &cHardwareCPU::Inst_IfDonor, INST_CLASS_CONDITIONAL, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("prod-string",  &cHardwareCPU::Inst_ProduceString, INST_CLASS_ENVIRONMENT, STALL),
		
    // Group formation instructions
    StaticTableInstLib<tMethod>::MethodEntry("join-group", &cHardwareCPU::Inst_JoinGroup, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("join-next-group", &cHardwareCPU::Inst_JoinNextGroup, INST_CLASS_ENVIRONMENT, STALL), 
    StaticTableInstLib<tMethod>::MethodEntry("kill-group-member", &cHardwareCPU::Inst_KillGroupMember, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("orgs-in-my-group", &cHardwareCPU::Inst_NumberOrgsInMyGroup, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("orgs-in-group", &cHardwareCPU::Inst_NumberOrgsInGroup, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("number-next-group", &cHardwareCPU::Inst_NumberNextGroup, INST_CLASS_ENVIRONMENT, STALL), 

    StaticTableInstLib<tMethod>::MethodEntry("join-mt-group", &cHardwareCPU::Inst_JoinMTGroup, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("join-next-mt-group", &cHardwareCPU::Inst_JoinMTGroup, INST_CLASS_ENVIRONMENT, STALL), 
    StaticTableInstLib<tMethod>::MethodEntry("num-mt-in-my-group", &cHardwareCPU::Inst_NumberMTInMyGroup, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("num-mt-in-group", &cHardwareCPU::Inst_NumberMTInGroup, INST_CLASS_ENVIRONMENT, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("num-mt-next-group", &cHardwareCPU::Inst_NumberMTNextGroup, INST_CLASS_ENVIRONMENT, STALL), 
    
    StaticTableInstLib<tMethod>::MethodEntry("inc-tolerance", &cHardwareCPU::Inst_IncTolerance, INST_CLASS_ENVIRONMENT, STALL),  
    StaticTableInstLib<tMethod>::MethodEntry("dec-tolerance", &cHardwareCPU::Inst_DecTolerance, INST_CLASS_ENVIRONMENT, STALL), 
    StaticTableInstLib<tMethod>::MethodEntry("get-tolerance", &cHardwareCPU::Inst_GetTolerance, INST_CLASS_ENVIRONMENT, STALL),    
    StaticTableInstLib<tMethod>::MethodEntry("get-group-tolerance", &cHardwareCPU::Inst_GetGroupTolerance, INST_CLASS_ENVIRONMENT, STALL), 

    
    // Division of labor instructions
    StaticTableInstLib<tMethod>::MethodEntry("get-age", &cHardwareCPU::Inst_GetTimeUsed, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("point-mut", &cHardwareCPU::Inst_ApplyPointMutations, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("varying-point-mut", &cHardwareCPU::Inst_ApplyVaryingPointMutations, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repair-on", &cHardwareCPU::Inst_RepairPointMutOn, INST_CLASS_LIFECYCLE, STALL),
    StaticTableInstLib<tMethod>::MethodEntry("repair-off", &cHardwareCPU::Inst_RepairPointMutOff, INST_CLASS_LIFECYCLE, STALL),
    
    // Must always be the last instruction in the array
    StaticTableInstLib<tMethod>::MethodEntry("NULL", &cHardwareCPU::Inst_Nop, INST_CLASS_NOP, 0, "True no-operation instruction: does nothing"),
  };
  
  const int n_size = sizeof(s_n_array)/sizeof(cNOPEntryCPU);
  
  static int nop_mods[n_size];
  for (int i = 0; i < n_size && i < NUM_REGISTERS; i++) {
    nop_mods[i] = s_n_array[i].nop_mod;
  }
  
  const int f_size = sizeof(s_f_array)/sizeof(StaticTableInstLib<tMethod>::MethodEntry);
  static tMethod functions[f_size];
  for (int i = 0; i < f_size; i++) functions[i] = s_f_array[i].GetFunction();
  
  const int def = 0;
  const int null_inst = f_size - 1;
  
  return new StaticTableInstLib<tMethod>(f_size, s_f_array, n_names, nop_mods, functions, def, null_inst);
}

cHardwareCPU::cHardwareCPU(cAvidaContext& ctx, cWorld* world, cOrganism* in_organism, cInstSet* in_inst_set)
: cHardwareBase(world, in_organism, in_inst_set)
, m_last_cell_data(false, 0)
{
  m_functions = s_inst_slib->GetFunctions();
  
  m_spec_die = false;
  m_epigenetic_state = false;
  
  m_thread_slicing_parallel = (m_world->GetConfig().THREAD_SLICING_METHOD.Get() == 1);
  m_no_cpu_cycle_time = m_world->GetConfig().NO_CPU_CYCLE_TIME.Get();
  
  m_promoters_enabled = m_world->GetConfig().PROMOTERS_ENABLED.Get();
  m_constitutive_regulation = m_world->GetConfig().CONSTITUTIVE_REGULATION.Get();
  
  m_slip_read_head = !m_world->GetConfig().SLIP_COPY_MODE.Get();
  
  // Initialize memory...
  const Genome& in_genome = in_organism->GetGenome();
  ConstInstructionSequencePtr in_seq_p;
  in_seq_p.DynamicCastFrom(in_genome.Representation());
  m_memory = *in_seq_p;
  
  Reset(ctx);                            // Setup the rest of the hardware...
  internalReset();
}

bool cHardwareCPU::checkNoMutList(cHeadCPU to)
{
    //Anya's code for head to head experiments
    //Tests to see if the given cHeadCPU has an instruction that is on the no mutation list, returns false if it is not and true if it is
    bool in_list = false;
    char test_inst = to.GetInst().GetSymbol()[0];
    cString no_mut_list = m_world->GetConfig().NO_MUT_INSTS.Get();
    for (int i=0; i<(int)strlen(no_mut_list); i++) {
        if ((char) no_mut_list[i] == test_inst) {
            in_list = true;
        }
    }
    return in_list;
}


void cHardwareCPU::internalReset()
{
  m_global_stack.Clear();
  
  // We want to reset to have a single thread.
  m_threads.Resize(1);
  
  // Reset that single thread.
  m_threads[0].Reset(this, 0);
  m_thread_id_chart = 1; // Mark only the first thread as taken...
  m_cur_thread = 0;
  
  // But then reset thread to have any epigenetic information we have saved
  if (m_epigenetic_state) {
    for (int i=0; i<NUM_REGISTERS; i++) {
      m_threads[0].reg[i] = m_epigenetic_saved_reg[i];
    }
    m_threads[0].stack = m_epigenetic_saved_stack;
  }
  
  m_mal_active = false;
  m_executedmatchstrings = false;
  
  
  // Promoter model
  if (m_world->GetConfig().PROMOTERS_ENABLED.Get()) {
    // Ideally, this shouldn't be hard-coded
    Instruction promoter_inst = m_inst_set->GetInst("promoter");
    
    m_promoter_index = -1; // Meaning the last promoter was nothing
    m_promoter_offset = 0;
    m_promoters.Resize(0);
    for (int i=0; i< m_memory.GetSize(); i++)
    {
      if (m_memory[i] == promoter_inst)
      {
        int code = Numberate(i-1, -1, m_world->GetConfig().PROMOTER_CODE_SIZE.Get());
        m_promoters.Push( cPromoter(i,code) );
      }
    }
  }
  
  m_last_cell_data = std::make_pair(false, 0);
}

void cHardwareCPU::internalResetOnFailedDivide()
{
  internalReset();
  m_mal_active = true;
  m_advance_ip = false;
}



void cHardwareCPU::cLocalThread::operator=(const cLocalThread& in_thread)
{
  m_id = in_thread.m_id;
  for (int i = 0; i < NUM_REGISTERS; i++) reg[i] = in_thread.reg[i];
  for (int i = 0; i < NUM_HEADS; i++) heads[i] = in_thread.heads[i];
  stack = in_thread.stack;
}

void cHardwareCPU::cLocalThread::Reset(cHardwareBase* in_hardware, int in_id)
{
  m_id = in_id;
  
  for (int i = 0; i < NUM_REGISTERS; i++) reg[i] = 0;
  for (int i = 0; i < NUM_HEADS; i++) heads[i].Reset(in_hardware);
  
  stack.Clear();
  cur_stack = 0;
  cur_head = nHardware::HEAD_IP;
  read_label.Clear();
  next_label.Clear();
  
  // Promoter model
  m_promoter_inst_executed = 0;
}

void cHardwareCPU::SetupMiniTraceFileHeader(Avida::Output::File& df, const int gen_id, const Apto::String& genotype) { (void)df, (void)gen_id, (void)genotype; }


// This function processes the very next command in the genome, and is made
// to be as optimized as possible.  This is the heart of avida.

bool cHardwareCPU::SingleProcess(cAvidaContext& ctx, bool speculative)
{
  assert(!speculative || (speculative && !m_thread_slicing_parallel));
  
  int last_IP_pos = getIP().GetPosition();
  
  // Mark this organism as running...
  m_organism->SetRunning(true);
  
  if (!speculative && m_spec_die) {
    m_organism->Die(ctx);
    m_organism->SetRunning(false);
    return false;
  }
  
  cPhenotype& phenotype = m_organism->GetPhenotype();
  
  // First instruction - check whether we should be starting at a promoter, when enabled.
  if (phenotype.GetCPUCyclesUsed() == 0 && m_promoters_enabled) Inst_Terminate(ctx);
  
  // Count the cpu cycles used
  phenotype.IncCPUCyclesUsed();
  if (!m_world->GetConfig().NO_CPU_CYCLE_TIME.Get()) phenotype.IncTimeUsed();
  
  int num_threads = m_threads.GetSize();
  
  // If we have threads turned on and we executed each thread in a single
  // timestep, adjust the number of instructions executed accordingly.
  int num_inst_exec = m_thread_slicing_parallel ? num_threads : 1;
  
  for (int i = 0; i < num_inst_exec; i++) {
    // Setup the hardware for the next instruction to be executed.
    int last_thread = m_cur_thread;
    
    m_cur_thread++;
    
    if (m_cur_thread >= num_threads) m_cur_thread = 0;
    
    m_advance_ip = true;
    cHeadCPU& ip = m_threads[m_cur_thread].heads[nHardware::HEAD_IP];
    ip.Adjust();
    
    
    // Print the status of this CPU at each step...
    if (m_tracer) m_tracer->TraceHardware(ctx, *this);
    
    // Find the instruction to be executed
    const Instruction cur_inst = ip.GetInst();
    
    if (speculative && (m_spec_die || m_inst_set->ShouldStall(cur_inst))) {
      // Speculative instruction reject, flush and return
      m_cur_thread = last_thread;
      phenotype.DecCPUCyclesUsed();
      if (!m_no_cpu_cycle_time) phenotype.IncTimeUsed(-1);
      m_organism->SetRunning(false);
      return false;
    }
    
    // Test if costs have been paid and it is okay to execute this now...
    bool exec = true;
    if (m_has_any_costs) exec = SingleProcess_PayPreCosts(ctx, cur_inst, m_cur_thread);
    
    // Constitutive regulation applied here
    if (m_constitutive_regulation) Inst_SenseRegulate(ctx); 
    
    // If there are no active promoters and a certain mode is set, then don't execute any further instructions
    if (m_promoters_enabled && m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 2 && m_promoter_index == -1) exec = false;
    
    // Now execute the instruction...
    if (exec == true) {
      // NOTE: This call based on the cur_inst must occur prior to instruction
      //       execution, because this instruction reference may be invalid after
      //       certain classes of instructions (namely divide instructions) @DMB
      const int time_cost = m_inst_set->GetAddlTimeCost(cur_inst);
      
      // Prob of exec (moved from SingleProcess_PayCosts so that we advance IP after a fail)
      if (m_inst_set->GetProbFail(cur_inst) > 0.0) {
        exec = !( ctx.GetRandom().P(m_inst_set->GetProbFail(cur_inst)) );
      }
      
      // Flag instruction as executed even if it failed (moved from SingleProcess_ExecuteInst)
      // this allows division conditions to be met even if most instruction executions failed. @JEB
      
      // Mark the instruction as executed
      getIP().SetFlagExecuted();
      
      // Add to the promoter inst executed count before executing the inst (in case it is a terminator)
      if (m_promoters_enabled) m_threads[m_cur_thread].IncPromoterInstExecuted();
      
      if (exec == true) {
        if (SingleProcess_ExecuteInst(ctx, cur_inst)) { 
          SingleProcess_PayPostResCosts(ctx, cur_inst); 
          SingleProcess_SetPostCPUCosts(ctx, cur_inst, m_cur_thread); 
        }
      }
      
      // Check if the instruction just executed caused premature death, break out of execution if so
      if (phenotype.GetToDelete()) break;
      
      // Some instruction (such as jump) may turn m_advance_ip off.  Usually
      // we now want to move to the next instruction in the memory.
      if (m_advance_ip == true) ip.Advance();
      
      // Pay the time cost of the instruction now
      phenotype.IncTimeUsed(time_cost);
      
      // In the promoter model, we may force termination after a certain number of inst have been executed
      if (m_promoters_enabled) {
        const double processivity = m_world->GetConfig().PROMOTER_PROCESSIVITY.Get();
        if (ctx.GetRandom().P(1 - processivity)) Inst_Terminate(ctx);
        if (m_world->GetConfig().PROMOTER_INST_MAX.Get() && (m_threads[m_cur_thread].GetPromoterInstExecuted() >= m_world->GetConfig().PROMOTER_INST_MAX.Get())) 
          Inst_Terminate(ctx);
      }
      
      // check for difference in thread count caused by KillThread or ForkThread
      if (num_threads == m_threads.GetSize()+1){
        --num_threads;
        --num_inst_exec;
      } else if (num_threads > m_threads.GetSize() && m_threads.GetSize() == 1) {
        // divide probably occured, I think divide insts. are the only ones that can reduce the thread count by more than one.
        num_threads = 1;
        num_inst_exec=0;
      } else if (num_threads > m_threads.GetSize()) {
        cerr<<cur_inst.GetOp()<<" "<<cur_inst.GetSymbol()<<" "<< num_threads << " " << m_threads.GetSize() <<endl;
        m_organism->Fault(FAULT_LOC_DEFAULT, FAULT_TYPE_ERROR);
        cerr<<"Error in thread handling\n";
        exit(-1);
      }      
    } // if exec
    
  } // Previous was executed once for each thread...
  
  // Kill creatures who have reached their max num of instructions executed
  const int max_executed = m_organism->GetMaxExecuted();
  if ((max_executed > 0 && phenotype.GetTimeUsed() >= max_executed) || phenotype.GetToDie() == true) {
    if (speculative) m_spec_die = true;
    else m_organism->Die(ctx);
  }
  if (!speculative && phenotype.GetToDelete()) m_spec_die = true;
  
  // Note: if organism just died, this will NOT let it repro.
  CheckImplicitRepro(ctx, last_IP_pos > m_threads[m_cur_thread].heads[nHardware::HEAD_IP].GetPosition());
  
  m_organism->SetRunning(false);
  
  return !m_spec_die;
}

// This method will handle the actual execution of an instruction
// within a single process, once that function has been finalized.
bool cHardwareCPU::SingleProcess_ExecuteInst(cAvidaContext& ctx, const Instruction& cur_inst) 
{
  // Copy Instruction locally to handle stochastic effects
  Instruction actual_inst = cur_inst;
  
  // Get a pointer to the corresponding method...
  int inst_idx = m_inst_set->GetLibFunctionIndex(actual_inst);
  
  // instruction execution count incremented
  m_organism->GetPhenotype().IncCurInstCount(actual_inst.GetOp());
	
  // And execute it.
  const bool exec_success = (this->*(m_functions[inst_idx]))(ctx);
  
  // NOTE: Organism may be dead now if instruction executed killed it (such as some divides, "die", or "kazi")
	
  // Decrement if the instruction was not executed successfully.
  if (exec_success == false) {
    m_organism->GetPhenotype().DecCurInstCount(actual_inst.GetOp());
  }
  
  return exec_success;
}


void cHardwareCPU::ProcessBonusInst(cAvidaContext& ctx, const Instruction& inst)
{
  // Mark this organism as running...
  bool prev_run_state = m_organism->IsRunning();
  m_organism->SetRunning(true);
  
  if (m_tracer) m_tracer->TraceHardware(ctx, *this, true);
  
  SingleProcess_ExecuteInst(ctx, inst);
  
  m_organism->SetRunning(prev_run_state);
}


void cHardwareCPU::PrintStatus(ostream& fp)
{
  fp << m_organism->GetPhenotype().GetCPUCyclesUsed() << " ";
  fp << "IP:" << getIP().GetPosition() << " (" << GetInstSet().GetName(IP().GetInst()) << ")" << endl;
  
  for (int i = 0; i < NUM_REGISTERS; i++) {
    fp << static_cast<char>('A' + i) << "X:" << GetRegister(i) << " ";
    fp << setbase(16) << "[0x" << GetRegister(i) << "]  " << setbase(10);
  }
  
  // Add some extra information if additional time costs are used for instructions,
  // leave this out if there are no differences to keep it cleaner
  if (m_organism->GetPhenotype().GetTimeUsed() != m_organism->GetPhenotype().GetCPUCyclesUsed()) {
    fp << "  EnergyUsed:" << m_organism->GetPhenotype().GetTimeUsed(); // this is not energy that is used by the energy model
  }
  fp << endl;
  
  fp << "  R-Head:" << getHead(nHardware::HEAD_READ).GetPosition() << " "
  << "W-Head:" << getHead(nHardware::HEAD_WRITE).GetPosition()  << " "
  << "F-Head:" << getHead(nHardware::HEAD_FLOW).GetPosition()   << "  "
  << "RL:" << GetReadLabel().AsString() << "   "
  << endl;
  
  int number_of_stacks = GetNumStacks();
  for (int stack_id = 0; stack_id < number_of_stacks; stack_id++) {
    fp << ((m_threads[m_cur_thread].cur_stack == stack_id) ? '*' : ' ') << " Stack " << stack_id << ":" << setbase(16) << setfill('0');
    for (int i = 0; i < nHardware::STACK_SIZE; i++) fp << " Ox" << setw(8) << GetStack(i, stack_id, 0);
    fp << setfill(' ') << setbase(10) << endl;
  }
  
  fp << "  Mem (" << m_memory.GetSize() << "):"
  << "  " << m_memory.AsString()
  << endl;
  
  if (m_ext_mem.GetSize()) {
    fp << "  Ext Mem: " << m_ext_mem[0];
    for (int i = 1; i < m_ext_mem.GetSize(); i++) fp << ", " << m_ext_mem[i];
    fp << endl;
  }
  
  if (m_world->GetConfig().PROMOTERS_ENABLED.Get())
  {
    fp << "  Promoters: index=" << m_promoter_index << " offset=" << m_promoter_offset;
    fp << " exe_inst=" << m_threads[m_cur_thread].GetPromoterInstExecuted();
    for (int i=0; i<m_promoters.GetSize(); i++) {
      fp << setfill(' ') << setbase(10) << " " << m_promoters[i].m_pos << ":";
      fp << "Ox" << setbase(16) << setfill('0') << setw(8) << (m_promoters[i].GetRegulatedBitCode()) << " "; 
    }
    fp << setfill(' ') << setbase(10) << endl;
  }    
  fp.flush();
}

/////////////////////////////////////////////////////////////////////////
// Method: cHardwareCPU::FindLabel(direction)
//
// Search in 'direction' (+ or - 1) from the instruction pointer for the
// compliment of the label in 'next_label' and return a pointer to the
// results.  If direction is 0, search from the beginning of the genome.
//
/////////////////////////////////////////////////////////////////////////

cHeadCPU cHardwareCPU::FindLabel(int direction)
{
  cHeadCPU & inst_ptr = getIP();
  
  // Start up a search head at the position of the instruction pointer.
  cHeadCPU search_head(inst_ptr);
  cCodeLabel & search_label = GetLabel();
  
  // Make sure the label is of size > 0.
  
  if (search_label.GetSize() == 0) {
    return inst_ptr;
  }
  
  // Call special functions depending on if jump is forwards or backwards.
  int found_pos = 0;
  if ( direction < 0 ) {
    found_pos = FindLabel_Backward(search_label, m_memory, inst_ptr.GetPosition() - search_label.GetSize());
  }
  
  // Jump forward.
  else if (direction > 0) {
    found_pos = FindLabel_Forward(search_label, m_memory, inst_ptr.GetPosition());
  }
  
  // Jump forward from the very beginning.
  else {
    found_pos = FindLabel_Forward(search_label, m_memory, 0);
  }
  
  // Return the last line of the found label, if it was found.
  if (found_pos >= 0) search_head.Set(found_pos - 1);
  
  // Return the found position (still at start point if not found).
  return search_head;
}


// Search forwards for search_label from _after_ position pos in the
// memory.  Return the first line _after_ the the found label.  It is okay
// to find search label's match inside another label.

int cHardwareCPU::FindLabel_Forward(const cCodeLabel & search_label,
                                    const InstructionSequence & search_genome, int pos)
{
  assert (pos < search_genome.GetSize() && pos >= 0);
  
  int search_start = pos;
  int label_size = search_label.GetSize();
  bool found_label = false;
  
  // Move off the template we are on.
  pos += label_size;
  
  // Search until we find the complement or exit the memory.
  while (pos < search_genome.GetSize()) {
    
    // If we are within a label, rewind to the beginning of it and see if
    // it has the proper sub-label that we're looking for.
    
    if (m_inst_set->IsNop(search_genome[pos])) {
      // Find the start and end of the label we're in the middle of.
      
      int start_pos = pos;
      int end_pos = pos + 1;
      while (start_pos > search_start &&
             m_inst_set->IsNop( search_genome[start_pos - 1] )) {
        start_pos--;
      }
      while (end_pos < search_genome.GetSize() &&
             m_inst_set->IsNop( search_genome[end_pos] )) {
        end_pos++;
      }
      int test_size = end_pos - start_pos;
      
      // See if this label has the proper sub-label within it.
      int max_offset = test_size - label_size + 1;
      int offset = start_pos;
      for (offset = start_pos; offset < start_pos + max_offset; offset++) {
        
        // Test the number of matches for this offset.
        int matches;
        for (matches = 0; matches < label_size; matches++) {
          if (search_label[matches] !=
              m_inst_set->GetNopMod( search_genome[offset + matches] )) {
            break;
          }
        }
        
        // If we have found it, break out of this loop!
        if (matches == label_size) {
          found_label = true;
          break;
        }
      }
      
      // If we've found the complement label, set the position to the end of
      // the label we found it in, and break out.
      
      if (found_label == true) {
        // pos = end_pos;
        pos = label_size + offset;
        break;
      }
      
      // We haven't found it; jump pos to just after the current label being
      // checked.
      pos = end_pos;
    }
    
    // Jump up a block to the next possible point to find a label,
    pos += label_size;
  }
  
  // If the label was not found return a -1.
  if (found_label == false) pos = -1;
  
  return pos;
}

// Search backwards for search_label from _before_ position pos in the
// memory.  Return the first line _after_ the the found label.  It is okay
// to find search label's match inside another label.

int cHardwareCPU::FindLabel_Backward(const cCodeLabel & search_label,
                                     const InstructionSequence & search_genome, int pos)
{
  assert (pos < search_genome.GetSize());
  
  int search_start = pos;
  int label_size = search_label.GetSize();
  bool found_label = false;
  
  // Move off the template we are on.
  pos -= label_size;
  
  // Search until we find the complement or exit the memory.
  while (pos >= 0) {
    // If we are within a label, rewind to the beginning of it and see if
    // it has the proper sub-label that we're looking for.
    
    if (m_inst_set->IsNop( search_genome[pos] )) {
      // Find the start and end of the label we're in the middle of.
      
      int start_pos = pos;
      int end_pos = pos + 1;
      while (start_pos > 0 && m_inst_set->IsNop(search_genome[start_pos - 1])) {
        start_pos--;
      }
      while (end_pos < search_start &&
             m_inst_set->IsNop(search_genome[end_pos])) {
        end_pos++;
      }
      int test_size = end_pos - start_pos;
      
      // See if this label has the proper sub-label within it.
      int max_offset = test_size - label_size + 1;
      for (int offset = start_pos; offset < start_pos + max_offset; offset++) {
        
        // Test the number of matches for this offset.
        int matches;
        for (matches = 0; matches < label_size; matches++) {
          if (search_label[matches] !=
              m_inst_set->GetNopMod(search_genome[offset + matches])) {
            break;
          }
        }
        
        // If we have found it, break out of this loop!
        if (matches == label_size) {
          found_label = true;
          break;
        }
      }
      
      // If we've found the complement label, set the position to the end of
      // the label we found it in, and break out.
      
      if (found_label == true) {
        pos = end_pos;
        break;
      }
      
      // We haven't found it; jump pos to just before the current label
      // being checked.
      pos = start_pos - 1;
    }
    
    // Jump up a block to the next possible point to find a label,
    pos -= label_size;
  }
  
  // If the label was not found return a -1.
  if (found_label == false) pos = -1;
  
  return pos;
}

// Search for 'in_label' anywhere in the hardware.
cHeadCPU cHardwareCPU::FindLabel(const cCodeLabel & in_label, int direction)
{
  assert (in_label.GetSize() > 0);
  
  // IDEALY:
  // Keep making jumps (in the proper direction) equal to the label
  // length.  If we are inside of a label, check its size, and see if
  // any of the sub-labels match properly.
  // FOR NOW:
  // Get something which works, no matter how inefficient!!!
  
  cHeadCPU temp_head(this);
  
  while (temp_head.InMemory()) {
    // IDEALY: Analyze the label we are in; see if the one we are looking
    // for could be a sub-label of it.  Skip past it if not.
    
    int i;
    for (i = 0; i < in_label.GetSize(); i++) {
      if (!m_inst_set->IsNop(temp_head.GetInst()) ||
          in_label[i] != m_inst_set->GetNopMod(temp_head.GetInst())) {
        break;
      }
    }
    if (i == GetLabel().GetSize()) {
      temp_head.AbsJump(i - 1);
      return temp_head;
    }
    
    temp_head.AbsJump(direction);     // IDEALY: MAKE LARGER JUMPS
  }
  
  temp_head.AbsSet(-1);
  return temp_head;
}

void cHardwareCPU::FindLabelInMemory(const cCodeLabel& label, cHeadCPU& search_head)
{
  assert(label.GetSize() > 0); // Trying to find label of 0 size!
  
  
  while (search_head.InMemory()) {
    // If we are not in a label, jump to the next checkpoint...
    if (!m_inst_set->IsNop(search_head.GetInst())) {
      search_head.AbsJump(label.GetSize());
      continue;
    }
    
    // Otherwise, rewind to the begining of this label...
    
    while (!(search_head.AtFront()) && m_inst_set->IsNop(search_head.GetInst(-1)))
      search_head.AbsJump(-1);
    
    // Calculate the size of the label being checked, and make sure they
    // are equal.
    
    int size = 0;
    bool label_match = true;
    do {
      // Check if the nop matches
      if (size < label.GetSize() && label[size] != m_inst_set->GetNopMod(search_head.GetInst()))
        label_match = false;
      
      // Increment the current position and length calculation
      search_head.AbsJump(1);
      size++;
      
      // While still within memory and the instruction is a nop
    } while (search_head.InMemory() && m_inst_set->IsNop(search_head.GetInst()));
    
    if (size != label.GetSize()) continue;
    
    // temp_head will point to the first non-nop instruction after the label, or the end of the memory space
    //   if this is a match, return this position
    if (label_match) return;
  }
  
  // The label does not exist in this creature.
  
  search_head.AbsSet(-1);
}


void cHardwareCPU::ReadInst(const int in_inst)
{
  if (m_inst_set->IsNop( Instruction(in_inst) )) {
    GetReadLabel().AddNop(in_inst);
  } else {
    GetReadLabel().Clear();
  }
}


void cHardwareCPU::AdjustHeads()
{
  for (int i = 0; i < m_threads.GetSize(); i++) {
    for (int j = 0; j < NUM_HEADS; j++) {
      m_threads[i].heads[j].Adjust();
    }
  }
}



// This function looks at the current position in the info of a creature,
// and sets the next_label to be the sequence of nops which follows.  The
// instruction pointer is left on the last line of the label found.

void cHardwareCPU::ReadLabel(int max_size)
{
  int count = 0;
  cHeadCPU * inst_ptr = &( getIP() );
  
  GetLabel().Clear();
  
  while (m_inst_set->IsNop(inst_ptr->GetNextInst()) &&
         (count < max_size)) {
    count++;
    inst_ptr->Advance();
    GetLabel().AddNop(m_inst_set->GetNopMod(inst_ptr->GetInst()));
    
    // If this is the first line of the template, mark it executed.
    if (GetLabel().GetSize() <=	m_world->GetConfig().MAX_LABEL_EXE_SIZE.Get()) {
      inst_ptr->SetFlagExecuted();
    }
  }
}


bool cHardwareCPU::ForkThread()
{
  const int num_threads = m_threads.GetSize();
  if (num_threads == m_world->GetConfig().MAX_CPU_THREADS.Get()) return false;
  
  // Make room for the new thread.
  m_threads.Resize(num_threads + 1);
  
  // Initialize the new thread to the same values as the current one.
  m_threads[num_threads] = m_threads[m_cur_thread];
  
  // Find the first free bit in m_thread_id_chart to determine the new
  // thread id.
  int new_id = 0;
  while ( (m_thread_id_chart >> new_id) & 1) new_id++;
  m_threads[num_threads].SetID(new_id);
  m_thread_id_chart |= (1 << new_id);
  
  return true;
}


bool cHardwareCPU::KillThread()
{
  // Make sure that there is always at least one thread...
  if (m_threads.GetSize() == 1) return false;
  
  // Note the current thread and set the current back one.
  const int kill_thread = m_cur_thread;
  ThreadPrev();
  
  // Turn off this bit in the m_thread_id_chart...
  m_thread_id_chart ^= 1 << m_threads[kill_thread].GetID();
  
  // Copy the last thread into the kill position
  const int last_thread = m_threads.GetSize() - 1;
  if (last_thread != kill_thread) {
    m_threads[kill_thread] = m_threads[last_thread];
  }
  
  // Kill the thread!
  m_threads.Resize(m_threads.GetSize() - 1);
  
  if (m_cur_thread > kill_thread) m_cur_thread--;
	
  return true;
}

////////////////////////////
//  Instruction Helpers...
////////////////////////////

inline int cHardwareCPU::FindModifiedRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(getIP().GetNextInst())) {
    getIP().Advance();
    default_register = m_inst_set->GetNopMod(getIP().GetInst());
    getIP().SetFlagExecuted();
  }
  return default_register;
}

inline int cHardwareCPU::FindModifiedNextRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(getIP().GetNextInst())) {
    getIP().Advance();
    default_register = m_inst_set->GetNopMod(getIP().GetInst());
    getIP().SetFlagExecuted();
  } else {
    default_register = (default_register + 1) % NUM_REGISTERS;
  }
  return default_register;
}

inline int cHardwareCPU::FindModifiedPreviousRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(getIP().GetNextInst())) {
    getIP().Advance();
    default_register = m_inst_set->GetNopMod(getIP().GetInst());
    getIP().SetFlagExecuted();
  } else {
    default_register = (default_register + NUM_REGISTERS - 1) % NUM_REGISTERS;
  }
  return default_register;
}


inline int cHardwareCPU::FindModifiedHead(int default_head)
{
  assert(default_head < NUM_HEADS); // Head ID too high.
  
  if (m_inst_set->IsNop(getIP().GetNextInst())) {
    getIP().Advance();
    default_head = m_inst_set->GetNopMod(getIP().GetInst());
    getIP().SetFlagExecuted();
  }
  return default_head;
}


inline int cHardwareCPU::FindNextRegister(int base_reg)
{
  return (base_reg + 1) % NUM_REGISTERS;
}


bool cHardwareCPU::Allocate_Necro(const int new_size)
{
  m_memory.ResizeOld(new_size);
  return true;
}

bool cHardwareCPU::Allocate_Random(cAvidaContext& ctx, const int old_size, const int new_size)
{
  m_memory.Resize(new_size);
  
  for (int i = old_size; i < new_size; i++) {
    m_memory[i] = m_inst_set->GetRandomInst(ctx);
  }
  return true;
}

bool cHardwareCPU::Allocate_Default(const int new_size)
{
  m_memory.Resize(new_size);
  
  // New space already defaults to default instruction...
  
  return true;
}

bool cHardwareCPU::Allocate_Main(cAvidaContext& ctx, const int allocated_size)
{
  // must do divide before second allocate & must allocate positive amount...
  if (m_world->GetConfig().REQUIRE_ALLOCATE.Get() && m_mal_active == true) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR, "Allocate already active");
    return false;
  }
  if (allocated_size < 1) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
                      cStringUtil::Stringf("Allocate of %d too small", allocated_size));
    return false;
  }
  
  const int old_size = m_memory.GetSize();
  const int new_size = old_size + allocated_size;
  
  // Make sure that the new size is in range.
  if (new_size > MAX_GENOME_LENGTH  ||  new_size < MIN_GENOME_LENGTH) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
                      cStringUtil::Stringf("Invalid post-allocate size (%d)",
                                           new_size));
    return false;
  }
  
  const int max_alloc_size = (int) (old_size * m_world->GetConfig().OFFSPRING_SIZE_RANGE.Get());
  if (allocated_size > max_alloc_size) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
                      cStringUtil::Stringf("Allocate too large (%d > %d)",
                                           allocated_size, max_alloc_size));
    return false;
  }
  
  const int max_old_size =
  (int) (allocated_size * m_world->GetConfig().OFFSPRING_SIZE_RANGE.Get());
  if (old_size > max_old_size) {
    m_organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
                      cStringUtil::Stringf("Allocate too small (%d > %d)",
                                           old_size, max_old_size));
    return false;
  }
  
  switch (m_world->GetConfig().ALLOC_METHOD.Get()) {
    case ALLOC_METHOD_NECRO:
      // Only break if this succeeds -- otherwise just do random.
      if (Allocate_Necro(new_size) == true) break;
    case ALLOC_METHOD_RANDOM:
      Allocate_Random(ctx, old_size, new_size);
      break;
    case ALLOC_METHOD_DEFAULT:
      Allocate_Default(new_size);
      break;
  }
  
  m_mal_active = true;
  
  return true;
}

int cHardwareCPU::calcCopiedSize(const int parent_size, const int child_size)
{
  int copied_size = 0;
  for (int i = parent_size; i < parent_size + child_size; i++) {
    if (m_memory.FlagCopied(i)) copied_size++;
  }
  return copied_size;
}  


bool cHardwareCPU::Divide_Main(cAvidaContext& ctx, const int div_point,
                               const int extra_lines, double mut_multiplier)
{
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
	
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  InstructionSequencePtr offspring_seq(new InstructionSequence(m_memory.Crop(div_point, div_point + child_size)));
  HashPropertyMap props;
  cHardwareManager::SetupPropertyMap(props, (const char*)m_inst_set->GetInstSetName());
  Genome offspring(GetType(), props, offspring_seq);
    
  // Make sure it is an exact copy at this point (before divide mutations) if required
  const Genome& base_genome = m_organism->GetGenome();
  ConstInstructionSequencePtr seq_p;
  seq_p.DynamicCastFrom(base_genome.Representation());
  const InstructionSequence& seq = *seq_p;
  if (m_world->GetConfig().REQUIRE_EXACT_COPY.Get() && (seq != *offspring_seq) ) {
    return false;
  }
  
  m_organism->OffspringGenome() = offspring;
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  // Handle Divide Mutations...
  Divide_DoMutations(ctx, mut_multiplier);
  
  // Many tests will require us to run the offspring through a test CPU;
  // this is, for example, to see if mutations need to be reverted or if
  // lineages need to be updated.
  Divide_TestFitnessMeasures1(ctx); 
  
  if (m_world->GetConfig().DIVIDE_METHOD.Get() != DIVIDE_METHOD_OFFSPRING) {
    // reset first time instruction costs
    for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
      m_inst_ft_cost[i] = m_inst_set->GetFTCost(Instruction(i));
    }
  }
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child
  bool parent_alive = m_organism->ActivateDivide(ctx);
  
  // Do more work if the parent lives through the birth of the offspring
  if (parent_alive) {
    
    if ( (m_world->GetConfig().EPIGENETIC_METHOD.Get() == EPIGENETIC_METHOD_PARENT) 
        || (m_world->GetConfig().EPIGENETIC_METHOD.Get() == EPIGENETIC_METHOD_BOTH) ) {
      InheritState(*this);  
    }
    
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

/*
 Almost the same as Divide_Main, but resamples reverted offspring.
 
 RESAMPLING ONLY WORKS CORRECTLY WHEN ALL MUTIONS OCCUR ON DIVIDE!!
 
 AWC - 06/29/06
 */
bool cHardwareCPU::Divide_MainRS(cAvidaContext& ctx, const int div_point,
                                 const int extra_lines, double mut_multiplier)
{
  
  //cStats stats = m_world->GetStats();
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  InstructionSequencePtr offspring_seq(new InstructionSequence(m_memory.Crop(div_point, div_point + child_size)));
  HashPropertyMap props;
  cHardwareManager::SetupPropertyMap(props, (const char*)m_inst_set->GetInstSetName());
  Genome offspring(GetType(), props, offspring_seq);

  m_organism->OffspringGenome() = offspring;
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  unsigned 
  totalMutations = 0,
  mutations = 0;
  //RScount = 0;
  
  
  bool
  fitTest = false;
  
  // Handle Divide Mutations...
  /*
   Do mutations until one of these conditions are satisified:
   we have resampled X times
   we have an offspring with the same number of muations as the first offspring
   that is not reverted
   the parent is steralized (usually means an implicit mutation)
   */
  for (unsigned i = 0; i <= 100; i++) {
    if (i == 0) {
      mutations = totalMutations = Divide_DoMutations(ctx, mut_multiplier);
    }
    else{
      mutations = Divide_DoMutations(ctx, mut_multiplier);
      m_world->GetStats().IncResamplings();
    }
    
    fitTest = Divide_TestFitnessMeasures1(ctx);
    
    if (!fitTest && mutations >= totalMutations) break;
    
  } 
  // think about making this mutations == totalMuations - though this may be too hard...
  /*
   if (RScount > 2)
   cerr << "Resampled " << RScount << endl;
   */
  //org could not be resampled beneath the hard cap -- it is then steraalized
  if (fitTest/*RScount == 11*/) {
    m_organism->GetPhenotype().ChildFertile() = false;
    m_world->GetStats().IncFailedResamplings();
  }
  
  if (m_world->GetConfig().DIVIDE_METHOD.Get() != DIVIDE_METHOD_OFFSPRING) {
    
    // reset first time instruction costs
    for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
      m_inst_ft_cost[i] = m_inst_set->GetFTCost(Instruction(i));
    }
  }
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child, and do more work if the parent lives through the
  // birth.
  bool parent_alive = m_organism->ActivateDivide(ctx);
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

/*
 Almost the same as Divide_Main, but only allows for one mutation 
 on divde and resamples reverted offspring.
 
 RESAMPLING ONLY WORKS CORRECTLY WHEN ALL MUTIONS OCCUR ON DIVIDE!!
 
 AWC - 07/28/06
 */
bool cHardwareCPU::Divide_Main1RS(cAvidaContext& ctx, const int div_point,
                                  const int extra_lines, double mut_multiplier)
{
  
  //cStats stats = m_world->GetStats();
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  InstructionSequencePtr offspring_seq(new InstructionSequence(m_memory.Crop(div_point, div_point + child_size)));
  HashPropertyMap props;
  cHardwareManager::SetupPropertyMap(props, (const char*)m_inst_set->GetInstSetName());
  Genome offspring(GetType(), props, offspring_seq);

  m_organism->OffspringGenome() = offspring;
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  int totalMutations = 0;
  int mutations = 0;
  //    RScount = 0;
  
  bool fitTest = false;
  
  // Handle Divide Mutations...
  /*
   Do mutations until one of these conditions are satisified:
   we have resampled X times
   we have an offspring with the same number of muations as the first offspring
   that is not reverted
   the parent is steralized (usually means an implicit mutation)
   */
  
  mutations = totalMutations = Divide_DoMutations(ctx, mut_multiplier,1);
  for (int i = 0; i < 100; i++) {
    if (i > 0) {
      mutations = Divide_DoExactMutations(ctx, mut_multiplier,1);
      m_world->GetStats().IncResamplings();
    }
    
    fitTest = Divide_TestFitnessMeasures1(ctx);
    //if (mutations > 1 ) cerr << "Too Many mutations!!!!!!!!!!!!!!!" << endl;
    if (fitTest == false && mutations >= totalMutations) break;
    
  } 
  // think about making this mutations == totalMuations - though this may be too hard...
  /*
   if (RScount > 2)
   cerr << "Resampled " << RScount << endl;
   */
  //org could not be resampled beneath the hard cap -- it is then steraalized
  if (fitTest/*RScount == 11*/) {
    m_organism->GetPhenotype().ChildFertile() = false;
    m_world->GetStats().IncFailedResamplings();
  }
  
  if (m_world->GetConfig().DIVIDE_METHOD.Get() != DIVIDE_METHOD_OFFSPRING) {
    // reset first time instruction costs
    for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
      m_inst_ft_cost[i] = m_inst_set->GetFTCost(Instruction(i));
    }
  }
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child, and do more work if the parent lives through the
  // birth.
  bool parent_alive = m_organism->ActivateDivide(ctx);
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

/*
 Almost the same as Divide_Main, but only allows for one mutation 
 on divde and resamples reverted offspring.
 
 RESAMPLING ONLY WORKS CORRECTLY WHEN ALL MUTIONS OCCUR ON DIVIDE!!
 
 AWC - 07/28/06
 */
bool cHardwareCPU::Divide_Main2RS(cAvidaContext& ctx, const int div_point,
                                  const int extra_lines, double mut_multiplier)
{
  
  //cStats stats = m_world->GetStats();
  const int child_size = m_memory.GetSize() - div_point - extra_lines;
  
  // Make sure this divide will produce a viable offspring.
  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
  if (viable == false) return false;
  
  // Since the divide will now succeed, set up the information to be sent
  // to the new organism
  InstructionSequencePtr offspring_seq(new InstructionSequence(m_memory.Crop(div_point, div_point + child_size)));
  HashPropertyMap props;
  cHardwareManager::SetupPropertyMap(props, (const char*)m_inst_set->GetInstSetName());
  Genome offspring(GetType(), props, offspring_seq);

  m_organism->OffspringGenome() = offspring;
  
  // Cut off everything in this memory past the divide point.
  m_memory.Resize(div_point);
  
  int totalMutations = 0;
  int mutations = 0;
  //    RScount = 0;
  
  bool fitTest = false;
  
  
  // Handle Divide Mutations...
  /*
   Do mutations until one of these conditions are satisified:
   we have resampled X times
   we have an offspring with the same number of muations as the first offspring
   that is not reverted
   the parent is steralized (usually means an implicit mutation)
   */
  for (int i = 0; i < 100; i++){
    if (i == 0){
      mutations = totalMutations = Divide_DoMutations(ctx, mut_multiplier,2);
    }
    else{
      Divide_DoExactMutations(ctx, mut_multiplier,mutations);
      m_world->GetStats().IncResamplings();
    }
    
    fitTest = Divide_TestFitnessMeasures(ctx);
    //if (mutations > 1 ) cerr << "Too Many mutations!!!!!!!!!!!!!!!" << endl;
    if (!fitTest && mutations >= totalMutations) break;
    
  } 
  // think about making this mutations == totalMuations - though this may be too hard...
  /*
   if (RScount > 2)
   cerr << "Resampled " << RScount << endl;
   */
  //org could not be resampled beneath the hard cap -- it is then steraalized
  if (fitTest/*RScount == 11*/) {
    m_organism->GetPhenotype().ChildFertile() = false;
    m_world->GetStats().IncFailedResamplings();
  }
  
  if (m_world->GetConfig().DIVIDE_METHOD.Get() != DIVIDE_METHOD_OFFSPRING) {
    // reset first time instruction costs
    for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
      m_inst_ft_cost[i] = m_inst_set->GetFTCost(Instruction(i));
    }
  }
  
  m_mal_active = false;
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
    m_advance_ip = false;
  }
  
  // Activate the child, and do more work if the parent lives through the
  // birth.
  bool parent_alive = m_organism->ActivateDivide(ctx);
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  
  return true;
}

// Sets the current state of the hardware and also saves this state so
//  that future Reset() calls will reset to that epigenetic state
void cHardwareCPU::InheritState(cHardwareBase& in_hardware)
{ 
  m_epigenetic_state = true;
  cHardwareCPU& in_h = (cHardwareCPU&)in_hardware; 
  const cLocalThread& thread = in_h.GetThread(in_h.GetCurThread());
  for (int i=0; i<NUM_REGISTERS; i++) {
    m_epigenetic_saved_reg[i] = thread.reg[i];
    m_threads[m_cur_thread].reg[i] = m_epigenetic_saved_reg[i];
  }
  m_epigenetic_saved_stack = thread.stack;
  m_threads[m_cur_thread].stack = m_epigenetic_saved_stack;
}

//////////////////////////
// And the instructions...
//////////////////////////

bool cHardwareCPU::Inst_If0(cAvidaContext&)          // Execute next if ?bx? ==0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) != 0)  getIP().Advance();
  return true; 
}

bool cHardwareCPU::Inst_IfNot0(cAvidaContext&)       // Execute next if ?bx? != 0.
{ 
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) == 0)  getIP().Advance();
  return true;
}

// Same as Inst_If0, except AX is used by default, not BX
bool cHardwareCPU::Inst_If0_defaultAX(cAvidaContext&)          // Execute next if ?ax? ==0.
{
  const int reg_used = FindModifiedRegister(REG_AX);
  if (GetRegister(reg_used) != 0)  getIP().Advance();
  return true; 
}

// Same as Inst_IfNot0, except AX is used by default, not BX
bool cHardwareCPU::Inst_IfNot0_defaultAX(cAvidaContext&)       // Execute next if ?ax? != 0.
{ 
  const int reg_used = FindModifiedRegister(REG_AX);
  if (GetRegister(reg_used) == 0)  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfEqu(cAvidaContext&)      // Execute next if bx == ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) != GetRegister(op2))  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfNEqu(cAvidaContext&)     // Execute next if bx != ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) == GetRegister(op2))  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGr0(cAvidaContext&)       // Execute next if ?bx? ! < 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) <= 0)  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGr(cAvidaContext&)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) <= GetRegister(op2))  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGrEqu0(cAvidaContext&)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) < 0)  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGrEqu(cAvidaContext&)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) < GetRegister(op2)) getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLess0(cAvidaContext&)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) >= 0)  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLess(cAvidaContext&)       // Execute next if ?bx? < ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) >=  GetRegister(op2))  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLsEqu0(cAvidaContext&)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) > 0) getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLsEqu(cAvidaContext&)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) >  GetRegister(op2))  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfBit1(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if ((GetRegister(reg_used) & 1) == 0)  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfANotEqB(cAvidaContext&)     // Execute next if AX != BX
{
  if (GetRegister(REG_AX) == GetRegister(REG_BX) )  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfBNotEqC(cAvidaContext&)     // Execute next if BX != CX
{
  if (GetRegister(REG_BX) == GetRegister(REG_CX) )  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfANotEqC(cAvidaContext&)     // Execute next if AX != BX
{
  if (GetRegister(REG_AX) == GetRegister(REG_CX) )  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfGrX(cAvidaContext&)       // Execute next if BX > X; X value set according to NOP label
{
  // Compares value in BX to a specific value.  The value to compare to is determined by the nop label as follows:
  //    no nop label (default): valueToCompare = 1; nop-A: valueToCompare = -1
  //                     nop-B: valueToCompare = 2; nop-C: valueToCompare =  4
  // @LMG 2/13/2009
  
  int valueToCompare = 1;
  
  if (m_inst_set->IsNop(getIP().GetNextInst())) {
    getIP().Advance();    
    switch (m_inst_set->GetNopMod(getIP().GetInst())) {
        
      case REG_AX:
        valueToCompare = -1; break;
      case REG_BX:
        valueToCompare =  2; break;
      case REG_CX:
        valueToCompare =  4; break;
      default:
        valueToCompare =  1; break;
    }
    getIP().SetFlagExecuted();
    
  }
  
  if (GetRegister(REG_BX) <= valueToCompare)  getIP().Advance();
  
  return true;
}

bool cHardwareCPU::Inst_IfEquX(cAvidaContext&)       // Execute next if BX == X; X value set according to NOP label
{
  // Compares value in BX to a specific value.  The value to compare to is determined by the nop label as follows:
  //    no nop label (default): valueToCompare = 1; nop-A: valueToCompare = -1
  //                     nop-B: valueToCompare = 2; nop-C: valueToCompare =  4
  // @LMG 2/13/2009
  
  int valueToCompare = 1;
  
  if (m_inst_set->IsNop(getIP().GetNextInst())) {
    getIP().Advance();
    switch (m_inst_set->GetNopMod(getIP().GetInst())) {
        
      case REG_AX: valueToCompare = -1; break;
      case REG_BX: valueToCompare =  2; break;
      case REG_CX: valueToCompare =  4; break;
      default:     valueToCompare =  1; break;
    }
    getIP().SetFlagExecuted();
    
  }
  
  if (GetRegister(REG_BX) != valueToCompare)  getIP().Advance();
  
  return true;
}



bool cHardwareCPU::Inst_IfP0p125(cAvidaContext& ctx)
{
  if (ctx.GetRandom().P(0.875)) {
    getIP().Advance();
  }
  
  return true;
}


bool cHardwareCPU::Inst_IfP0p25(cAvidaContext& ctx)
{
  if (ctx.GetRandom().P(0.75)) {
    getIP().Advance();
  }
  
  return true;
}


bool cHardwareCPU::Inst_IfP0p50(cAvidaContext& ctx)
{
  if (ctx.GetRandom().P(0.5)) {
    getIP().Advance();
  }
  
  return true;
}


bool cHardwareCPU::Inst_IfP0p75(cAvidaContext& ctx)
{
  if (ctx.GetRandom().P(0.25)) {
    getIP().Advance();
  }
  
  return true;
}


bool cHardwareCPU::Inst_JumpF(cAvidaContext&)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  // If there is no label, jump BX steps.
  if (GetLabel().GetSize() == 0) {
    GetActiveHead().Jump(GetRegister(REG_BX));
    return true;
  }
  
  // Otherwise, try to jump to the complement label.
  const cHeadCPU jump_location(FindLabel(1));
  if ( jump_location.GetPosition() != -1 ) {
    GetActiveHead().Set(jump_location);
    return true;
  }
  
  // If complement label was not found; record an error.
  m_organism->Fault(FAULT_LOC_JUMP, FAULT_TYPE_ERROR,
                    "jump-f: No complement label");
  return false;
}


bool cHardwareCPU::Inst_JumpB(cAvidaContext&)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  // If there is no label, jump BX steps.
  if (GetLabel().GetSize() == 0) {
    GetActiveHead().Jump(GetRegister(REG_BX));
    return true;
  }
  
  // otherwise jump to the complement label.
  const cHeadCPU jump_location(FindLabel(-1));
  if ( jump_location.GetPosition() != -1 ) {
    GetActiveHead().Set(jump_location);
    return true;
  }
  
  // If complement label was not found; record an error.
  m_organism->Fault(FAULT_LOC_JUMP, FAULT_TYPE_ERROR,
                    "jump-b: No complement label");
  return false;
}

bool cHardwareCPU::Inst_Call(cAvidaContext&)
{
  // Put the starting location onto the stack
  const int location = getIP().GetPosition();
  StackPush(location);
  
  // Jump to the compliment label (or by the ammount in the bx register)
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  if (GetLabel().GetSize() == 0) {
    getIP().Jump(GetRegister(REG_BX));
    return true;
  }
  
  const cHeadCPU jump_location(FindLabel(1));
  if (jump_location.GetPosition() != -1) {
    getIP().Set(jump_location);
    return true;
  }
  
  // If complement label was not found; record an error.
  m_organism->Fault(FAULT_LOC_JUMP, FAULT_TYPE_ERROR,
                    "call: no complement label");
  return false;
}

bool cHardwareCPU::Inst_Return(cAvidaContext&)
{
  getIP().Set(StackPop());
  return true;
}

bool cHardwareCPU::Inst_Throw(cAvidaContext&)
{
  // Only initialize this once to save some time...
  static Instruction catch_inst = GetInstSet().GetInst(cStringUtil::Stringf("catch"));
  
  //Look for the label directly (no complement)
  ReadLabel();
  
  cHeadCPU search_head(getIP());
  int start_pos = search_head.GetPosition();
  search_head++;
  
  while (start_pos != search_head.GetPosition()) {
    // If we find a catch instruction, compare the NOPs following it
    if (search_head.GetInst() == catch_inst) {
      int catch_pos = search_head.GetPosition();
      search_head++;
      
      // Continue to examine the label after the catch
      //  (1) It ends (=> use the catch!)
      //  (2) It becomes longer than the throw label (=> use the catch!)
      //  (3) We find a NOP that doesnt match the throw (=> DON'T use the catch...)
      
      bool match = true;
      int size_matched = 0;      
      while ( match && m_inst_set->IsNop(search_head.GetInst()) && (size_matched < GetLabel().GetSize()) ) {
        if ( GetLabel()[size_matched] != m_inst_set->GetNopMod( search_head.GetInst()) ) match = false;
        search_head++;
        size_matched++;
      }
      
      // We found a matching catch instruction
      if (match) {
        getIP().Set(catch_pos);
        m_advance_ip = false; // Don't automatically move the IP
        // so we mark the catch as executed.
        return true;
      }
      
      //If we advanced past NOPs during testing, retreat
      if ( !m_inst_set->IsNop(search_head.GetInst()) ) search_head--;
    }
    search_head.Advance();
  }
  
  return false;
}


bool cHardwareCPU::Inst_ThrowIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) return false;
  return Inst_Throw(ctx);
}

bool cHardwareCPU::Inst_ThrowIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0) return false;
  return Inst_Throw(ctx);
}

bool cHardwareCPU::Inst_Goto(cAvidaContext&)
{
  // Only initialize this once to save some time...
  static Instruction label_inst = GetInstSet().GetInst(cStringUtil::Stringf("label"));
  
  //Look for an EXACT label match after a 'label' instruction
  ReadLabel();
  
  cHeadCPU search_head(getIP());
  int start_pos = search_head.GetPosition();
  search_head++;
  
  while (start_pos != search_head.GetPosition()) {
    if (search_head.GetInst() == label_inst) {
      int label_pos = search_head.GetPosition();
      search_head++;
      int size_matched = 0;
      while ( size_matched < GetLabel().GetSize() ) {
        if ( !m_inst_set->IsNop(search_head.GetInst()) ) break;
        if ( GetLabel()[size_matched] != m_inst_set->GetNopMod( search_head.GetInst()) ) break;
        if ( !m_inst_set->IsNop(search_head.GetInst()) ) break;
        
        size_matched++;
        search_head++;
      }
      
      // We found a matching 'label' instruction only if the next 
      // instruction (at the search head now) is also not a NOP
      if ( (size_matched == GetLabel().GetSize()) && !m_inst_set->IsNop(search_head.GetInst()) ) {
        getIP().Set(label_pos);
        m_advance_ip = false; // Don't automatically move the IP
        // so we mark the catch as executed.
        return true;
      }
      
      //If we advanced past NOPs during testing, retreat
      if ( !m_inst_set->IsNop(search_head.GetInst()) ) search_head--;
    }
    search_head++;
  }
  
  return false;
}


bool cHardwareCPU::Inst_GotoIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) return false;
  return Inst_Goto(ctx);
}

bool cHardwareCPU::Inst_GotoIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0) return false;
  return Inst_Goto(ctx);
}


bool cHardwareCPU::Inst_Pop(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = StackPop();
  return true;
}

bool cHardwareCPU::Inst_Push(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  StackPush(GetRegister(reg_used));
  return true;
}

bool cHardwareCPU::Inst_HeadPop(cAvidaContext&)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  getHead(head_used).Set(StackPop());
  return true;
}

bool cHardwareCPU::Inst_HeadPush(cAvidaContext&)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  StackPush(getHead(head_used).GetPosition());
  if (head_used == nHardware::HEAD_IP) {
    getHead(head_used).Set(getHead(nHardware::HEAD_FLOW));
    m_advance_ip = false;
  }
  return true;
}


bool cHardwareCPU::Inst_PopA(cAvidaContext&) { GetRegister(REG_AX) = StackPop(); return true;}
bool cHardwareCPU::Inst_PopB(cAvidaContext&) { GetRegister(REG_BX) = StackPop(); return true;}
bool cHardwareCPU::Inst_PopC(cAvidaContext&) { GetRegister(REG_CX) = StackPop(); return true;}

bool cHardwareCPU::Inst_PushA(cAvidaContext&) { StackPush(GetRegister(REG_AX)); return true;}
bool cHardwareCPU::Inst_PushB(cAvidaContext&) { StackPush(GetRegister(REG_BX)); return true;}
bool cHardwareCPU::Inst_PushC(cAvidaContext&) { StackPush(GetRegister(REG_CX)); return true;}

bool cHardwareCPU::Inst_SwitchStack(cAvidaContext&) { SwitchStack(); return true; }

bool cHardwareCPU::Inst_Swap(cAvidaContext&)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  Swap(GetRegister(op1), GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_SwapAB(cAvidaContext&)\
{
  Swap(GetRegister(REG_AX), GetRegister(REG_BX)); return true;
}
bool cHardwareCPU::Inst_SwapBC(cAvidaContext&)
{
  Swap(GetRegister(REG_BX), GetRegister(REG_CX)); return true;
}
bool cHardwareCPU::Inst_SwapAC(cAvidaContext&)
{
  Swap(GetRegister(REG_AX), GetRegister(REG_CX)); return true;
}

bool cHardwareCPU::Inst_CopyReg(cAvidaContext&)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = FindNextRegister(src);
  GetRegister(dst) = GetRegister(src);
  return true;
}

bool cHardwareCPU::Inst_CopyRegAB(cAvidaContext&)
{
  GetRegister(REG_AX) = GetRegister(REG_BX);   return true;
}
bool cHardwareCPU::Inst_CopyRegAC(cAvidaContext&)
{
  GetRegister(REG_AX) = GetRegister(REG_CX);   return true;
}
bool cHardwareCPU::Inst_CopyRegBA(cAvidaContext&)
{
  GetRegister(REG_BX) = GetRegister(REG_AX);   return true;
}
bool cHardwareCPU::Inst_CopyRegBC(cAvidaContext&)
{
  GetRegister(REG_BX) = GetRegister(REG_CX);   return true;
}
bool cHardwareCPU::Inst_CopyRegCA(cAvidaContext&)
{
  GetRegister(REG_CX) = GetRegister(REG_AX);   return true;
}
bool cHardwareCPU::Inst_CopyRegCB(cAvidaContext&)
{
  GetRegister(REG_CX) = GetRegister(REG_BX);   return true;
}

bool cHardwareCPU::Inst_Reset(cAvidaContext&)
{
  GetRegister(REG_AX) = 0;
  GetRegister(REG_BX) = 0;
  GetRegister(REG_CX) = 0;
  StackClear();
  m_last_cell_data = std::make_pair(false, 0);
  return true;
}

bool cHardwareCPU::Inst_ShiftR(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) >>= 1;
  return true;
}

bool cHardwareCPU::Inst_ShiftL(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) <<= 1;
  return true;
}

bool cHardwareCPU::Inst_Bit1(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) |=  1;
  return true;
}

bool cHardwareCPU::Inst_SetNum(cAvidaContext&)
{
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsInt(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValGrey(cAvidaContext&) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntGreyCode(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValDir(cAvidaContext&) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntDirect(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValAddP(cAvidaContext&) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntAdditivePolynomial(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValFib(cAvidaContext&) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntFib(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_ValPolyC(cAvidaContext&) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntPolynomialCoefficent(NUM_NOPS);
  return true;
}

bool cHardwareCPU::Inst_Inc(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) += 1;
  return true;
}

bool cHardwareCPU::Inst_Dec(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) -= 1;
  return true;
}

bool cHardwareCPU::Inst_Zero(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_One(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = 1;
  return true;
}

bool cHardwareCPU::Inst_All1s(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = 0;
  
  for (int i=0; i< ((int) sizeof(int) * 8); i++) {
    GetRegister(reg_used) |= 1 << i;
  }
  
  return true;
}

bool cHardwareCPU::Inst_Neg(cAvidaContext&)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = -GetRegister(src);
  return true;
}

bool cHardwareCPU::Inst_Square(cAvidaContext&)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = GetRegister(src) * GetRegister(src);
  return true;
}

bool cHardwareCPU::Inst_Sqrt(cAvidaContext&)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value > 1) GetRegister(dst) = static_cast<int>(sqrt(static_cast<double>(value)));
  else if (value < 0) {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "sqrt: value is negative");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Log(cAvidaContext&)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value >= 1) GetRegister(dst) = static_cast<int>(log(static_cast<double>(value)));
  else if (value < 0) {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "log: value is negative");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Log10(cAvidaContext&)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value >= 1) GetRegister(dst) = static_cast<int>(log10(static_cast<double>(value)));
  else if (value < 0) {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "log10: value is negative");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Add(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) + GetRegister(op2);
  return true;
}

bool cHardwareCPU::Inst_Sub(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) - GetRegister(op2);
  return true;
}

bool cHardwareCPU::Inst_Mult(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) * GetRegister(op2);
  return true;
}

bool cHardwareCPU::Inst_Div(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op2) != 0) {
    if (0-INT_MAX > GetRegister(op1) && GetRegister(op2) == -1)
      m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: Float exception");
    else
      GetRegister(dst) = GetRegister(op1) / GetRegister(op2);
  } else {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: dividing by 0");
    return false;
  }
  return true;
}

bool cHardwareCPU::Inst_Mod(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op2) != 0) {
    GetRegister(dst) = GetRegister(op1) % GetRegister(op2);
  } else {
    m_organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "mod: modding by 0");
    return false;
  }
  return true;
}


bool cHardwareCPU::Inst_Nand(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = ~(GetRegister(op1) & GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_Nor(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = ~(GetRegister(op1) | GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_Or(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = (GetRegister(op1) | GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_And(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = (GetRegister(op1) & GetRegister(op2));
  return true;
}

bool cHardwareCPU::Inst_Not(cAvidaContext&)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = ~(GetRegister(src));
  return true;
}

bool cHardwareCPU::Inst_Order(cAvidaContext&)
{
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op1) > GetRegister(op2)) {
    Swap(GetRegister(op1), GetRegister(op2));
  }
  return true;
}

bool cHardwareCPU::Inst_Xor(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) ^ GetRegister(op2);
  return true;
}

// Set the bit in ?BX? specified in its complement register
bool cHardwareCPU::Inst_Setbit(cAvidaContext&)
{
  const int to_set = FindModifiedRegister(REG_BX);
  const int bit_reg = FindNextRegister(to_set);
  
  const int bit_to_set = max(0, GetRegister(bit_reg)) % (sizeof(int) * 8);
  
  GetRegister(to_set) |= 1 << bit_to_set;
  
  return true;
}

// Clear the bit in ?BX? specified in its complement register
bool cHardwareCPU::Inst_Clearbit(cAvidaContext&)
{
  const int to_clear = FindModifiedRegister(REG_BX);
  const int bit_reg = FindNextRegister(to_clear);
  
  const int bit_to_clear = max(0, GetRegister(bit_reg)) % (sizeof(int) * 8);
  
  GetRegister(to_clear) &= ~(1 << bit_to_clear);
  
  return true;
}


bool cHardwareCPU::Inst_Copy(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_AX;
  
  const cHeadCPU from(this, GetRegister(op1));
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  //checkNoMutList is for head to head kaboom experiments
  if (m_organism->TestCopyMut(ctx) && !(checkNoMutList(from))) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();  // Mark this instruction as mutated...
    to.SetFlagCopyMut();  // Mark this instruction as copy mut...
  } else {
    to.SetInst(from.GetInst());
    to.ClearFlagMutated();  // UnMark
    to.ClearFlagCopyMut();  // UnMark
  }
  
  to.SetFlagCopied();  // Set the copied flag.
  //  cpu_stats.mut_stats.copies_exec++;
  return true;
}

bool cHardwareCPU::Inst_ReadInst(cAvidaContext&)
{
  const int dst = FindModifiedRegister(REG_CX);
  const int src = REG_BX;
  
  const cHeadCPU from(this, GetRegister(src));
  
  // Dis-allowing mutations on read, for the moment (write only...)
  // @CAO This allows perfect error-correction...
  GetRegister(dst) = from.GetInst().GetOp();
  return true;
}

bool cHardwareCPU::Inst_WriteInst(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_CX);
  const int op1 = REG_BX;
  const int op2 = REG_AX;
  
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  const int value = Mod(GetRegister(src), m_inst_set->GetSize());
  
  // Change value on a mutation... checkNoMutList is for head to head
    //kaboom experiments
  if (m_organism->TestCopyMut(ctx) && !(checkNoMutList(to))) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
  } else {
    to.SetInst(Instruction(value));
    to.ClearFlagMutated();     // UnMark
    to.ClearFlagCopyMut();     // UnMark
  }
  
  to.SetFlagCopied();  // Set the copied flag.
  return true;
}

bool cHardwareCPU::Inst_StackReadInst(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  cHeadCPU from(this, GetRegister(reg_used));
  StackPush(from.GetInst().GetOp());
  return true;
}

bool cHardwareCPU::Inst_StackWriteInst(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_AX;
  cHeadCPU to(this, GetRegister(op1) + GetRegister(dst));
  const int value = Mod(StackPop(), m_inst_set->GetSize());
  
  // Change value on a mutation... checkNoMutList is for head to head kaboom experiments
  if (m_organism->TestCopyMut(ctx) && !(checkNoMutList(to))) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
  } else {
    to.SetInst(Instruction(value));
    to.ClearFlagMutated();     // UnMark
    to.ClearFlagCopyMut();     // UnMark
  }
  
  to.SetFlagCopied();  // Set the copied flag.
  return true;
}

bool cHardwareCPU::Inst_Compare(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_CX);
  const int op1 = REG_BX;
  const int op2 = REG_AX;
  
  cHeadCPU from(this, GetRegister(op1));
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  // Compare is dangerous -- it can cause mutations!
    //checkNoMutList is for head to head kaboom experiments
  if (m_organism->TestCopyMut(ctx) && !(checkNoMutList(from))) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
  }
  
  GetRegister(dst) = from.GetInst().GetOp() - to.GetInst().GetOp();
  
  return true;
}

bool cHardwareCPU::Inst_IfNCpy(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_AX;
  
  const cHeadCPU from(this, GetRegister(op1));
  const cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  // Allow for errors in this test...
  if (m_organism->TestCopyMut(ctx)) {
    if (from.GetInst() != to.GetInst()) getIP().Advance();
  } else {
    if (from.GetInst() == to.GetInst()) getIP().Advance();
  }
  return true;
}

bool cHardwareCPU::Inst_Allocate(cAvidaContext& ctx)   // Allocate bx more space...
{
  const int src = REG_BX;
  const int dst = REG_AX;
  const int size = m_memory.GetSize();
  if (Allocate_Main(ctx, GetRegister(src))) {
    GetRegister(dst) = size;
    return true;
  } else return false;
}

bool cHardwareCPU::Inst_Divide(cAvidaContext& ctx)  
{ 
  const int src = REG_AX;
  return Divide_Main(ctx, GetRegister(src));    
}

/*
 Divide with resampling -- Same as regular divide but on reversions will be 
 resampled after they are reverted.
 
 AWC 06/29/06
 
 */

bool cHardwareCPU::Inst_DivideRS(cAvidaContext& ctx)  
{ 
  const int src = REG_AX;
  return Divide_MainRS(ctx, GetRegister(src));    
}


bool cHardwareCPU::Inst_CDivide(cAvidaContext& ctx) 
{ 
  return Divide_Main(ctx, m_memory.GetSize() / 2);   
}

bool cHardwareCPU::Inst_CAlloc(cAvidaContext& ctx)  
{ 
  return Allocate_Main(ctx, m_memory.GetSize());   
}

bool cHardwareCPU::Inst_MaxAlloc(cAvidaContext& ctx)   // Allocate maximal more
{
  const int dst = REG_AX;
  const int cur_size = m_memory.GetSize();
  const int alloc_size = Apto::Min((int)(m_world->GetConfig().OFFSPRING_SIZE_RANGE.Get() * cur_size), MAX_GENOME_LENGTH - cur_size);
  if (Allocate_Main(ctx, alloc_size)) {
    GetRegister(dst) = cur_size;
    return true;
  } else return false;
}

// Alloc and move write head if we're successful
bool cHardwareCPU::Inst_MaxAllocMoveWriteHead(cAvidaContext& ctx)   // Allocate maximal more
{
  const int dst = REG_AX;
  const int cur_size = m_memory.GetSize();
  const int alloc_size = Apto::Min((int)(m_world->GetConfig().OFFSPRING_SIZE_RANGE.Get() * cur_size), MAX_GENOME_LENGTH - cur_size);
  if (Allocate_Main(ctx, alloc_size)) {
    GetRegister(dst) = cur_size;
    getHead(nHardware::HEAD_WRITE).Set(cur_size);
    return true;
  } else return false;
}

bool cHardwareCPU::Inst_Transposon(cAvidaContext&)
{
  ReadLabel();
  return true;
}

void cHardwareCPU::Divide_DoTransposons(cAvidaContext& ctx)
{
  // This only works if 'transposon' is in the current instruction set
  static bool transposon_in_use = GetInstSet().InstInSet(cStringUtil::Stringf("transposon"));
  if (!transposon_in_use) return;
  
  static Instruction transposon_inst = GetInstSet().GetInst(cStringUtil::Stringf("transposon"));
  Genome& child = m_organism->OffspringGenome();
  InstructionSequencePtr child_seq_p;
  child_seq_p.DynamicCastFrom(child.Representation());
  InstructionSequence& child_genome = *child_seq_p;
  
  // Count the number of transposons that are marked as executed
  int tr_count = 0;
  for (int i = 0; i < m_memory.GetSize(); i++) {
    if (m_memory.FlagExecuted(i) && (m_memory[i] == transposon_inst)) tr_count++;
  }
  
  for (int i = 0; i < tr_count; i++) {
    if (ctx.GetRandom().P(0.01)) {
      const unsigned int mut_line = ctx.GetRandom().GetUInt(child_genome.GetSize() + 1);
      child_genome.Insert(mut_line, transposon_inst);
    }
  }
}


bool cHardwareCPU::Inst_Repro(cAvidaContext& ctx)
{ 
  // check if repro can replace an existing organism
  if (m_world->GetConfig().REPRO_METHOD.Get() == 0 && m_organism->IsNeighborCellOccupied()) {
    return false;
  }
  
  if (m_organism->GetPhenotype().GetCurBonus() < m_world->GetConfig().REQUIRED_BONUS.Get()) {
    return false;
  }
  
  // Setup child
  m_organism->OffspringGenome() = m_organism->GetGenome();
  InstructionSequencePtr offspring_seq;
  offspring_seq.DynamicCastFrom(m_organism->OffspringGenome().Representation());

  ConstInstructionSequencePtr org_seq;
  org_seq.DynamicCastFrom(m_organism->GetGenome().Representation());
  
  // Do transposon movement and copying before other mutations
  Divide_DoTransposons(ctx);
  
  // Perform Copy Mutations...
  if (m_organism->GetCopyMutProb() > 0) { // Skip this if no mutations....
    for (int i = 0; i < offspring_seq->GetSize(); i++) {
      //Need to check no_mut_insts for head to head kaboom experiments
      bool in_list = false;
      char test_inst = (*offspring_seq)[i].GetSymbol()[0];
      cString no_mut_list = m_world->GetConfig().NO_MUT_INSTS.Get();
      for (int j = 0; j < (int)strlen(no_mut_list); j++) {
        if ((char) no_mut_list[j] == test_inst) in_list = true;
      }
      if (m_organism->TestCopyMut(ctx) && !(in_list)) {
        (*offspring_seq)[i] = m_inst_set->GetRandomInst(ctx);
      }
    }
  }
  
  //Perform divide mutations...
  Divide_DoMutations(ctx);
  
  // Check viability
  bool viable = Divide_CheckViable(ctx, org_seq->GetSize(), offspring_seq->GetSize(), 1);
  if (!viable) { return false; }
  
  // Many tests will require us to run the offspring through a test CPU;
  // this is, for example, to see if mutations need to be reverted or if
  // lineages need to be updated.
  Divide_TestFitnessMeasures(ctx);
  
  if (m_world->GetConfig().DIVIDE_METHOD.Get() != DIVIDE_METHOD_OFFSPRING) {
    // reset first time instruction costs
    for (int i = 0; i < m_inst_ft_cost.GetSize(); i++) {
      m_inst_ft_cost[i] = m_inst_set->GetFTCost(Instruction(i));
    }
  }
  
  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) m_advance_ip = false;
  
  const bool parent_alive = m_organism->ActivateDivide(ctx);
  
  //Reset the parent
  if (parent_alive) {
    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset(ctx);
  }
  return true;
}

bool cHardwareCPU::Inst_ReproSex(cAvidaContext& ctx)
{
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(1);
  return Inst_Repro(ctx);
}



bool cHardwareCPU::Inst_TaskPutRepro(cAvidaContext& ctx)
{
  // Do normal IO, but don't zero register
  //Inst_TaskPut(ctx);
  
  const int reg_used = FindModifiedRegister(REG_BX);
  const int value = GetRegister(reg_used);
  // GetRegister(reg_used) = 0;
  m_organism->DoOutput(ctx, value);
  
  // Immediately attempt a repro
  return Inst_Repro(ctx);
}

bool cHardwareCPU::Inst_TaskPutResetInputsRepro(cAvidaContext& ctx)
{
  // Do normal IO
  bool return_value = Inst_TaskPutResetInputs(ctx);
  
  // Immediately attempt a repro
  Inst_Repro(ctx);
  
  // return value of put since successful repro would wipe state anyway
  return return_value; 
}


/* The organism can only replicate if the last task it performed is the task with ID 0 */
bool cHardwareCPU::Inst_ConditionalRepro(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetLastTaskID() == 0) { 
    return Inst_Repro(ctx);
  }
  return false;
}

bool cHardwareCPU::Inst_Kazi(cAvidaContext& ctx)
{
  assert(m_world->GetConfig().KABOOM_PROB.Get() != -1 || m_world->GetConfig().KABOOM_HAMMING.Get() != -1);
    //You can not have both kaboom_prob and kaboom_hamming set to adjustable because both must pull from the same register to be backwards compatible
  // Code changed to allow for AdjustableHD
  const int reg_used = FindModifiedRegister(REG_AX);
  m_organism->GetPhenotype().SetKaboomExecuted(true);
  //we're outputting just to trigger reaction checks
  m_organism->DoOutput(ctx, 0);
    
  double percent_prob = 1.0;
  int distance = -1;
  if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM_HAMMING.Get() == -1) {
    //Case where Probability is static and hamming distance is adjustable
    int get_reg_value = GetRegister(reg_used);
    //MAX_GENOME_SIZE and MIN_GENOME_SIZE should be set for these experiments, otherwise hamming distance doesn't make sense
    int genome_size = m_world->GetConfig().MAX_GENOME_SIZE.Get();
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (get_reg_value % genome_size);
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM_HAMMING.Get() != -1) {
    //Case where both Probability and Hamming Distance are static
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (int) m_world->GetConfig().KABOOM_HAMMING.Get();
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() == -1 && (int) m_world->GetConfig().KABOOM_HAMMING.Get() != -1) {
    // Case where Probability is adjustable and Hamming distance isn't
    percent_prob = ((double) (GetRegister(reg_used) % 100)) / 100.0;
    distance = (int) m_world->GetConfig().KABOOM_HAMMING.Get();
    }
    
  if (ctx.GetRandom().P(percent_prob)) m_organism->Kaboom(distance, ctx);
  return true;
}

bool cHardwareCPU::Inst_Kazi1(cAvidaContext& ctx)
{
  assert(m_world->GetConfig().KABOOM_PROB.Get() != -1 || m_world->GetConfig().KABOOM1_HAMMING.Get() != -1);
  const int reg_used = FindModifiedRegister(REG_AX);
  
  m_organism->GetPhenotype().SetKaboomExecuted(true);
  //we're outputting just to trigger reaction checks
  m_organism->DoOutput(ctx, 0);
  
  //These must always be set in the if, they can't both be adjustable, so don't do it
  int distance = -1;
  double percent_prob = 1.0;
  if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM1_HAMMING.Get() == -1) {
    //Case where Probability is static and hamming distance is adjustable
    int get_reg_value = GetRegister(reg_used);
    //MAX_GENOME_SIZE and MIN_GENOME_SIZE should be set for these experiments, otherwise hamming distance doesn't make sense
    int genome_size = m_world->GetConfig().MAX_GENOME_SIZE.Get();
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (get_reg_value % genome_size);
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM1_HAMMING.Get() != -1) {
    //Case where both Probability and Hamming Distance are static
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (int) m_world->GetConfig().KABOOM1_HAMMING.Get();
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() == -1 && (int) m_world->GetConfig().KABOOM1_HAMMING.Get() != -1) {
    //Case where Probability is adjustable and Hamming distance isn't
    percent_prob = ((double) (GetRegister(reg_used) % 100)) / 100.0;
    distance = (int) m_world->GetConfig().KABOOM1_HAMMING.Get();
  }
  
  if ( ctx.GetRandom().P(percent_prob) ) m_organism->Kaboom(distance, ctx);
  return true;
}

bool cHardwareCPU::Inst_Kazi2(cAvidaContext& ctx)
{
  assert(m_world->GetConfig().KABOOM_PROB.Get() != -1 || m_world->GetConfig().KABOOM2_HAMMING.Get() != -1);
  const int reg_used = FindModifiedRegister(REG_AX);
  m_organism->GetPhenotype().SetKaboomExecuted(true);
  //we're outputting just to trigger reaction checks
  m_organism->DoOutput(ctx, 0);
  
  //These must always be set in the if, they can't both be adjustable, so don't do it
  int distance = -1;
  double percent_prob = 1.0;
  if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM2_HAMMING.Get() == -1) {
    //Case where Probability is static and hamming distance is adjustable
    int get_reg_value = GetRegister(reg_used);
    //MAX_GENOME_SIZE and MIN_GENOME_SIZE should be set for these experiments, otherwise hamming distance doesn't make sense
    int genome_size = m_world->GetConfig().MAX_GENOME_SIZE.Get();
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (get_reg_value % genome_size);
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM2_HAMMING.Get() != -1) {
    //Case where both Probability and Hamming Distance are static
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (int) m_world->GetConfig().KABOOM2_HAMMING.Get();
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() == -1 && (int) m_world->GetConfig().KABOOM2_HAMMING.Get() != -1) {
    //Case where Probability is adjustable and Hamming distance isn't
    percent_prob = ((double) (GetRegister(reg_used) % 100)) / 100.0;
    distance = (int) m_world->GetConfig().KABOOM2_HAMMING.Get();
  }
  
  if ( ctx.GetRandom().P(percent_prob) ) m_organism->Kaboom(distance, ctx);
  return true;
}

bool cHardwareCPU::Inst_Kazi3(cAvidaContext& ctx)
{
  assert(m_world->GetConfig().KABOOM_PROB.Get() != -1 || m_world->GetConfig().KABOOM3_HAMMING.Get() != -1);
  const int reg_used = FindModifiedRegister(REG_AX);
  
  m_organism->GetPhenotype().SetKaboomExecuted(true);
  //we're outputting just to trigger reaction checks
  m_organism->DoOutput(ctx, 0);
  
  //These must always be set in the if, they can't both be adjustable, so don't do it
  int distance = -1;
  double percent_prob = 1.0;
  if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM3_HAMMING.Get() == -1) {
    //Case where Probability is static and hamming distance is adjustable
    int get_reg_value = GetRegister(reg_used);
    //MAX_GENOME_SIZE and MIN_GENOME_SIZE should be set for these experiments, otherwise hamming distance doesn't make sense
    int genome_size = m_world->GetConfig().MAX_GENOME_SIZE.Get();
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (get_reg_value % genome_size);
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM3_HAMMING.Get() != -1) {
    //Case where both Probability and Hamming Distance are static
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (int) m_world->GetConfig().KABOOM3_HAMMING.Get();
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() == -1 && (int) m_world->GetConfig().KABOOM3_HAMMING.Get() != -1) {
    //Case where Probability is adjustable and Hamming distance isn't
    percent_prob = ((double) (GetRegister(reg_used) % 100)) / 100.0;
    distance = (int) m_world->GetConfig().KABOOM3_HAMMING.Get();
  }
  
  if ( ctx.GetRandom().P(percent_prob) ) m_organism->Kaboom(distance, ctx);
  return true;
}

bool cHardwareCPU::Inst_Kazi4(cAvidaContext& ctx)
{
  assert(m_world->GetConfig().KABOOM_PROB.Get() != -1 || m_world->GetConfig().KABOOM4_HAMMING.Get() != -1);
  const int reg_used = FindModifiedRegister(REG_AX);
  
  m_organism->GetPhenotype().SetKaboomExecuted(true);
  //we're outputting just to trigger reaction checks
  m_organism->DoOutput(ctx, 0);
  
  //These must always be set in the if, they can't both be adjustable, so don't do it
  int distance = -1;
  double percent_prob = 1.0;
  if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM4_HAMMING.Get() == -1) {
    //Case where Probability is static and hamming distance is adjustable
    int get_reg_value = GetRegister(reg_used);
    //MAX_GENOME_SIZE and MIN_GENOME_SIZE should be set for these experiments, otherwise hamming distance doesn't make sense
    int genome_size = m_world->GetConfig().MAX_GENOME_SIZE.Get();
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (get_reg_value % genome_size);
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM4_HAMMING.Get() != -1) {
    //Case where both Probability and Hamming Distance are static
    percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
    distance = (int) m_world->GetConfig().KABOOM4_HAMMING.Get();
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() == -1 && (int) m_world->GetConfig().KABOOM4_HAMMING.Get() != -1) {
    //Case where Probability is adjustable and Hamming distance isn't
    percent_prob = ((double) (GetRegister(reg_used) % 100)) / 100.0;
    distance = (int) m_world->GetConfig().KABOOM4_HAMMING.Get();
  }
  
  if ( ctx.GetRandom().P(percent_prob) ) m_organism->Kaboom(distance, ctx);
  return true;
}

bool cHardwareCPU::Inst_Kazi5(cAvidaContext& ctx)
{
  assert(m_world->GetConfig().KABOOM_PROB.Get() != -1 || m_world->GetConfig().KABOOM5_HAMMING.Get() != -1);
  const int reg_used = FindModifiedRegister(REG_AX);
  
  m_organism->GetPhenotype().SetKaboomExecuted(true);
  //we're outputting just to trigger reaction checks
  m_organism->DoOutput(ctx, 0);
  
  //These must always be set in the if, they can't both be adjustable, so don't do it
  int distance = -1;
  double percent_prob = 1.0;
  if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM5_HAMMING.Get() == -1) {
      //Case where Probability is static and hamming distance is adjustable
      int get_reg_value = GetRegister(reg_used);
      //MAX_GENOME_SIZE and MIN_GENOME_SIZE should be set for these experiments, otherwise hamming distance doesn't make sense
      int genome_size = m_world->GetConfig().MAX_GENOME_SIZE.Get();
      percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
      distance = (get_reg_value % genome_size);
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() != -1 && (int) m_world->GetConfig().KABOOM5_HAMMING.Get() != -1) {
      //Case where both Probability and Hamming Distance are static
      percent_prob = (double) m_world->GetConfig().KABOOM_PROB.Get();
      distance = (int) m_world->GetConfig().KABOOM5_HAMMING.Get();
  } else if ((int) m_world->GetConfig().KABOOM_PROB.Get() == -1 && (int) m_world->GetConfig().KABOOM5_HAMMING.Get() != -1) {
      //Case where Probability is adjustable and Hamming distance isn't
      percent_prob = ((double) (GetRegister(reg_used) % 100)) / 100.0;
      distance = (int) m_world->GetConfig().KABOOM5_HAMMING.Get();
  }
    
  if ( ctx.GetRandom().P(percent_prob) ) m_organism->Kaboom(distance, ctx);
  return true;
}


bool cHardwareCPU::Inst_Sterilize(cAvidaContext&)
{
  m_organism->GetPhenotype().IsFertile() = false;
  return true;
}



bool cHardwareCPU::Inst_Die(cAvidaContext& ctx)
{
  m_organism->Die(ctx);
  return true;
}

bool cHardwareCPU::Inst_Poison(cAvidaContext&)
{
  double poison_multiplier = 1.0 - m_world->GetConfig().POISON_PENALTY.Get();
  m_organism->GetPhenotype().SetCurBonus(m_organism->GetPhenotype().GetCurBonus() * poison_multiplier);
  return true;
}

/* Similar to Kazi, this instructon probabilistically causes
 the organism to die. However, in this case it does so in 
 order to win points for its group and it does not take out
 any other organims. */
bool  cHardwareCPU::Inst_Suicide(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_AX);
  double percentProb = ((double) (GetRegister(reg_used) % 100)) / 100.0;
  if ( ctx.GetRandom().P(percentProb) ) {
    m_organism->Die(ctx);
  }
  
  return true;
}


bool cHardwareCPU::Inst_TaskGet(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  const int value = m_organism->GetNextInput();
  GetRegister(reg_used) = value;
  m_organism->DoInput(value);
  return true;
}


// @JEB - this instruction does more than two "gets" together, it also resets the inputs
bool cHardwareCPU::Inst_TaskGet2(cAvidaContext& ctx)
{
  // Randomize the inputs so they can't save numbers
  m_organism->GetOrgInterface().ResetInputs(ctx);   // Now re-randomize the inputs this organism sees
  m_organism->ClearInput();                         // Also clear their input buffers, or they can still claim
  // rewards for numbers no longer in their environment!
  
  const int reg_used_1 = FindModifiedRegister(REG_BX);
  const int reg_used_2 = FindNextRegister(reg_used_1);
  
  const int value1 = m_organism->GetNextInput();
  GetRegister(reg_used_1) = value1;
  m_organism->DoInput(value1);
  
  const int value2 = m_organism->GetNextInput();
  GetRegister(reg_used_2) = value2;
  m_organism->DoInput(value2);
  
  return true;
}

bool cHardwareCPU::Inst_TaskStackGet(cAvidaContext&)
{
  const int value = m_organism->GetNextInput();
  StackPush(value);
  m_organism->DoInput(value);
  return true;
}

bool cHardwareCPU::Inst_TaskStackLoad(cAvidaContext&)
{
  // @DMB - TODO: this should look at the input_size...
  for (int i = 0; i < 3; i++) StackPush( m_organism->GetNextInput() );
  return true;
}

bool cHardwareCPU::Inst_TaskPut(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int value = GetRegister(reg_used);
  GetRegister(reg_used) = 0;
  m_organism->DoOutput(ctx, value);
  return true;
}

bool cHardwareCPU::Inst_TaskPutResetInputs(cAvidaContext& ctx)
{
  bool return_value = Inst_TaskPut(ctx);          // Do a normal put
  m_organism->GetOrgInterface().ResetInputs(ctx);   // Now re-randomize the inputs this organism sees
  m_organism->ClearInput();                         // Also clear their input buffers, or they can still claim
  // rewards for numbers no longer in their environment!
  return return_value;
}

bool cHardwareCPU::Inst_TaskIO(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  // Do the "put" component
  const int value_out = GetRegister(reg_used);
  m_organism->DoOutput(ctx, value_out);  // Check for tasks completed.
  
  // Do the "get" component
  const int value_in = m_organism->GetNextInput();
  GetRegister(reg_used) = value_in;
  m_organism->DoInput(value_in);
  return true;
}

bool cHardwareCPU::Inst_TaskIO_BonusCost(cAvidaContext& ctx, double bonus_cost)
{
  // Levy the cost
  double new_bonus = m_organism->GetPhenotype().GetCurBonus() * (1 - bonus_cost);
  if (new_bonus < 0) new_bonus = 0;
  //keep the bonus positive or zero
  m_organism->GetPhenotype().SetCurBonus(new_bonus);
  
  return Inst_TaskIO(ctx);
}

bool cHardwareCPU::Inst_TaskIO_Feedback(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  //check cur_bonus before the output
  double preOutputBonus = m_organism->GetPhenotype().GetCurBonus();
  
  // Do the "put" component
  const int value_out = GetRegister(reg_used);
  m_organism->DoOutput(ctx, value_out);  // Check for tasks completed.
  
  //check cur_merit after the output
  double postOutputBonus = m_organism->GetPhenotype().GetCurBonus(); 
  
  
  //push the effect of the IO on merit (+,0,-) to the active stack
  
  if (preOutputBonus > postOutputBonus){
    StackPush(-1);
  }
  else if (preOutputBonus == postOutputBonus){
    StackPush(0);
  }
  else if (preOutputBonus < postOutputBonus){
    StackPush(1);
  }
  else {
    assert(0);
    //Bollocks. There was an error.
  }
  
  
  // Do the "get" component
  const int value_in = m_organism->GetNextInput();
  GetRegister(reg_used) = value_in;
  m_organism->DoInput(value_in);
  return true;
}

bool cHardwareCPU::Inst_MatchStrings(cAvidaContext& ctx)
{
  if (m_executedmatchstrings) return false;
  m_organism->DoOutput(ctx, 357913941);
  m_executedmatchstrings = true;
  return true;
}

bool cHardwareCPU::Inst_Send(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  m_organism->SendValue(GetRegister(reg_used));
  GetRegister(reg_used) = 0;
  
  return true;
}

bool cHardwareCPU::Inst_Receive(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = m_organism->ReceiveValue();
  
  return true;
}

bool cHardwareCPU::Inst_SenseLog2(cAvidaContext& ctx)
{
  return DoSense(ctx, 0, 2);
}

bool cHardwareCPU::Inst_SenseUnit(cAvidaContext& ctx)
{
  return DoSense(ctx, 1, 1);
}

bool cHardwareCPU::Inst_SenseMult100(cAvidaContext& ctx)
{
  return DoSense(ctx, 1, 100);
}

bool cHardwareCPU::DoSense(cAvidaContext& ctx, int conversion_method, double base)
{
  // Returns the amount of a resource or resources 
  // specified by modifying NOPs into register BX
  const Apto::Array<double> res_count = m_organism->GetOrgInterface().GetResources(ctx);
  
  // Arbitrarily set to BX since the conditional instructions use this directly.
  int reg_to_set = REG_BX;
  
  // There are no resources, return
  if (res_count.GetSize() == 0) return false;
  
  // Only recalculate logs if these values have changed
  static int last_num_resources = 0;
  static int max_label_length = 0;
  int num_nops = GetInstSet().GetNumNops();
  
  if ((last_num_resources != res_count.GetSize()))
  {
    max_label_length = (int) ceil(log((double)res_count.GetSize())/log((double)num_nops));
    last_num_resources = res_count.GetSize();
  }
  
  // Convert modifying NOPs to the index of the resource.
  // If there are fewer than the number of NOPs required
  // to uniquely specify a resource, then add together
  // a subset of resources (motivation: regulation can evolve
  // to be more specific if there is an advantage)
  
  // Find the maximum number of NOPs needed to specify this number of resources
  // Note: It's a bit wasteful to recalculate this every time and organisms will
  // definitely be confused if the number of resources changes during a run
  // because their mapping to resources will be disrupted
  
  // Attempt to read a label with this maximum length
  ReadLabel(max_label_length);
  
  // Find the length of the label that we actually obtained (max is max_reg_needed)
  int real_label_length = GetLabel().GetSize();
  
  // Start and end labels to define the start and end indices of  
  // resources that we need to add together
  cCodeLabel start_label = cCodeLabel(GetLabel());
  cCodeLabel   end_label = cCodeLabel(GetLabel());
  
  for (int i = 0; i < max_label_length - real_label_length; i++) {
    start_label.AddNop(0);
    end_label.AddNop(num_nops-1);
  }
  
  int start_index = start_label.AsInt(num_nops);
  int   end_index =   end_label.AsInt(num_nops);
  
  // If the label refers to ONLY resources that 
  // do not exist, then the operation fails
  if (start_index >= res_count.GetSize()) return false;
  
  // Otherwise sum all valid resources that it might refer to
  // (this will only be ONE if the label was of the maximum length).
  int resource_result = 0;
  double dresource_result = 0;
  for (int i = start_index; i <= end_index; i++) {
    // if it's a valid resource
    if (i < res_count.GetSize()) {
      if (conversion_method == 0) { // Log
        // for log, add together and then take log
        dresource_result += (double) res_count[i];
      }
      else if (conversion_method == 1) { // Addition of multiplied resource amount
        int add_amount = (int) (res_count[i] * base);
        // Do some range checking to make sure we don't overflow
        resource_result = (INT_MAX - resource_result <= add_amount) ? INT_MAX : resource_result + add_amount;
      }
    } 
  }
  
  // Take the log after adding resource amounts together! This way a zero can be assigned to INT_MIN
  if (conversion_method == 0) { // Log2
    // You really shouldn't be using the log method if you can get to zero resources
    if (dresource_result == 0.0) {
      resource_result = INT_MIN;
    }
    else {
      resource_result = (int)(log(dresource_result)/log(base));
    }
  }
  
  //Dump this value into an arbitrary register: BX
  GetRegister(reg_to_set) = resource_result;
  
  //We have to convert this to a different index that includes all degenerate labels possible: shortest to longest
  int sensed_index = 0;
  int on = 1;
  for (int i = 0; i < real_label_length; i++) {
    sensed_index += on;
    on *= num_nops;
  }
  sensed_index+= GetLabel().AsInt(num_nops);
  m_organism->GetPhenotype().IncSenseCount(sensed_index);
  
  return true; 
  
  // Note that we are converting <double> resources to <int> register values
}


bool cHardwareCPU::Inst_SenseResource0(cAvidaContext& ctx)
{
  return DoSenseResourceX(REG_BX, m_organism->GetCellID(), 0, ctx);
}

bool cHardwareCPU::Inst_SenseResource1(cAvidaContext& ctx)
{
  return DoSenseResourceX(REG_BX, m_organism->GetCellID(), 1, ctx);
}

bool cHardwareCPU::Inst_SenseResource2(cAvidaContext& ctx)
{
  return DoSenseResourceX(REG_BX, m_organism->GetCellID(), 2, ctx);
}

bool cHardwareCPU::Inst_SenseFacedResource0(cAvidaContext& ctx)
{
  return DoSenseResourceX(REG_BX, m_world->GetPopulation().GetCell(m_organism->GetCellID()).GetCellFaced().GetID(), 0, ctx);
}

bool cHardwareCPU::Inst_SenseFacedResource1(cAvidaContext& ctx)
{
  return DoSenseResourceX(REG_BX, m_world->GetPopulation().GetCell(m_organism->GetCellID()).GetCellFaced().GetID(), 1, ctx);
}

bool cHardwareCPU::Inst_SenseFacedResource2(cAvidaContext& ctx)
{
  return DoSenseResourceX(REG_BX, m_world->GetPopulation().GetCell(m_organism->GetCellID()).GetCellFaced().GetID(), 2, ctx);
}


bool cHardwareCPU::DoSenseResourceX(int reg_to_set, int cell_id, int resid, cAvidaContext& ctx) 
{
  assert(resid >= 0);
  
  cPopulation& pop = m_world->GetPopulation();
  
  const Apto::Array<double> & res_count = pop.GetResources().GetCellResources(cell_id, ctx);
  
  // Make sure we have the resource requested
  if (resid >= res_count.GetSize()) return false;
  
  GetRegister(reg_to_set) = (int) res_count[resid];
  
  return true; 
  
}

bool cHardwareCPU::Inst_SenseResourceID(cAvidaContext& ctx)
{
  const Apto::Array<double> res_count = m_organism->GetOrgInterface().GetResources(ctx);
  int reg_to_set = FindModifiedRegister(REG_BX);  
  double max_resource = 0.0;    
  // if more than one resource is available, return the resource ID with the most available in this spot (note that, with global resources, the GLOBAL total will evaluated)
  for (int i = 0; i < res_count.GetSize(); i++) {
    if (res_count[i] > max_resource) {
      max_resource = res_count[i];
      GetRegister(reg_to_set) = i;
    }
  }    
  return true;
}

bool cHardwareCPU::Inst_SenseOpinionResourceQuantity(cAvidaContext& ctx)
{
  const Apto::Array<double> res_count = m_organism->GetOrgInterface().GetResources(ctx);
  // check if this is a valid group
  if(m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    int opinion = m_organism->GetOpinion().first;
    int res_opinion = (int) (res_count[opinion] * 100 + 0.5);
    int reg_to_set = FindModifiedRegister(REG_BX);
    GetRegister(reg_to_set) = res_opinion;
  }
  return true;
}

/* Must be nop-modified.
 * Places the number of resources in the group either +1 or -1 in register BX,
 * wrapping from the top group back to group 1 (skipping 0),
 * +1 group if the nop register has a positive number
 * -1 group if the nop register has a negative number. 
 */
bool cHardwareCPU::Inst_SenseNextResLevel(cAvidaContext& ctx)
{
  // Check for an opinion.
  if (!m_organism->GetOrgInterface().HasOpinion(m_organism)) return false;
  if (m_world->GetConfig().USE_FORM_GROUPS.Get() != 2) return false;
  int opinion = m_organism->GetOpinion().first;
  
  const int num_groups = m_organism->GetOrgInterface().GetResources(ctx).GetSize();
  if (num_groups <= 2) return false;
  
  // If not nop-modified, fails to execute.
  if (!(m_inst_set->IsNop(getIP().GetNextInst()))) return false;
  // Retreives the value from the nop-modifying register.
  const int nop_register = FindModifiedRegister(REG_BX);
  int register_value = GetRegister(nop_register);
  if (register_value == 0) return false;
  
  const Apto::Array<double> res_count = m_organism->GetOrgInterface().GetResources(ctx);
  if (opinion == (num_groups - 1)) {
    if (register_value > 0) GetRegister(REG_BX) = (int) (res_count[1] * 100 + 0.5);
    else if (register_value < 0) GetRegister(REG_BX) = (int) (res_count[opinion - 1] * 100 + 0.5);
  }
  else if ((opinion == 1) || (opinion == 0)) {
    if (register_value > 0) GetRegister(REG_BX) = (int) (res_count[opinion + 1] * 100 + 0.5);
    else if (register_value < 0) GetRegister(REG_BX) = (int) (res_count[num_groups - 1] * 100 + 0.5);
  }
  else if ((opinion != (num_groups - 1)) && (opinion != 1) && (opinion != 0)) {
    if (register_value > 0) GetRegister(REG_BX) = (int) (res_count[opinion + 1] * 100 + 0.5);
    else if (register_value < 0) GetRegister(REG_BX) = (int) (res_count[opinion - 1] * 100 + 0.5);
  }
  return true;
}

bool cHardwareCPU::Inst_SenseDiffFaced(cAvidaContext& ctx) 
{
  const Apto::Array<double> res_count = m_organism->GetOrgInterface().GetResources(ctx);
  if(m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    int opinion = m_organism->GetOpinion().first;
    int reg_to_set = FindModifiedRegister(REG_BX);
    double faced_res = m_organism->GetOrgInterface().GetFacedCellResources(ctx)[opinion];  
    // return % change
    int res_diff = 0;
    if (res_count[opinion] == 0) res_diff = (int) faced_res;
    else res_diff = (int) (((faced_res - res_count[opinion])/res_count[opinion]) * 100 + 0.5);
    GetRegister(reg_to_set) = res_diff;
  }
  return true;
}


bool cHardwareCPU::Inst_SenseFacedHabitat(cAvidaContext& ctx) 
{
  int reg_to_set = FindModifiedRegister(REG_BX);
  
  // get the resource library
  const cResourceDefLib& resource_lib = m_world->GetEnvironment().GetResDefLib();
  
  // get the destination cell resource levels
  Apto::Array<double> cell_resource_levels = m_organism->GetOrgInterface().GetFacedCellResources(ctx);
  
  // check for any habitats ahead that affect movement, returning the most 'severe' habitat type
  // are there any barrier resources in the faced cell    
  for (int i = 0; i < cell_resource_levels.GetSize(); i++) {
    if (resource_lib.GetResDef(i)->GetHabitat() == 2 && cell_resource_levels[i] > 0) {
      GetRegister(reg_to_set) = 2;
      return true;
    }    
  }
  // if no barriers, are there any hills in the faced cell    
  for (int i = 0; i < cell_resource_levels.GetSize(); i++) {
    if (resource_lib.GetResDef(i)->GetHabitat() == 1 && cell_resource_levels[i] > 0) {
      GetRegister(reg_to_set) = 1;
      return true;
    }
  }
  // if no barriers or hills, we return a 0 to indicate clear sailing
  GetRegister(reg_to_set) = 0;
  return true;
}

/* Convert modifying NOPs to the index of a resource.  
 *
 * When the specification does not map to exactly one resource (either because the 
 * specification does not have enough nops to fully specify, or because the number of 
 * resources is not a power of the number of nops), choose randomly among the resources 
 * covered by the specification.  The random choice is weighted by how much of the 
 * resource is covered by the specification.
 *
 * For example, in a 3-nop 4-resource system:
 * A -> 75% chance resouce 0, 25% chance resource 1
 * AA and AB -> 100% chance resource 0
 * AC -> 75% chance resource 0, 25% chance resource 1
 *
 * Originally inspired by Jeff B.'s DoSense(); meant to be a helper function for
 * the various collect instructions, and anything else that wants to use this type 
 * of resource NOP-specification.
 */
int cHardwareCPU::FindModifiedResource(cAvidaContext& ctx, int& spec_id)
{
  int num_resources = m_organism->GetOrgInterface().GetResources(ctx).GetSize(); 
  
  //if there are no resources, translation cannot be successful; return false
  if (num_resources <= 0)
  {return -1;}
  
  //calculate the maximum number of NOPs necessary to completely specify a resource
  int num_nops = GetInstSet().GetNumNops();
  int max_label_length = (int)(ceil(log((double)num_resources) / log((double)num_nops)));
  
  //attempt to read a label of the maximum length
  ReadLabel(max_label_length);
  
  //find the length of the label that was actually read
  int real_label_length = GetLabel().GetSize();
  
  // save the specification id
  spec_id = GetLabel().AsIntUnique(num_nops);
  
  /* Find the resource specified by the label.
   * If the specification is not complete, pick a resource from the range specified.
   * If the range covers resources unequally, this is taken into account.
   */
  
  // translate the specification into a number
  int label_int = GetLabel().AsInt(num_nops);
  
  // find the chunk of a unit range covered by the specification
  double chunk_size = 1.0 / pow(double(num_nops), real_label_length);
  
  
  // choose a point in the range
  double resource_approx = label_int * chunk_size + ctx.GetRandom().GetDouble(chunk_size);
  
  // translate it into a resource bin
  int bin_used = floor(resource_approx * num_resources);
  
  return bin_used;
}

/* Helper function to reduce code redundancy in the Inst_Collect variations,
 * including Inst_Destroy.
 * Calls FindModifiedResource() to decide which resource to collect, logs which 
 * specification was used, then calls DoActualCollect() to do the environmental 
 * resource removal and/or internal resource addition.
 *
 * env_remove    - specifies whether the collected resources should be removed from
 *                 the environment
 * internal_add  - specifies whether the collected resources should be added to 
 *                 the organism's internal resources.
 * probabilistic - specifies whether the chance of collection success should be based on 
 *                 the amount of resource in the environment.
 * unit          - specifies whether collection uses the ABSORB_RESOURCE_FRACTION
 *                 configuration or always collects 1 unit of resource.
 */
bool cHardwareCPU::DoCollect(cAvidaContext& ctx, bool env_remove, bool internal_add, bool probabilistic, bool unit)
{
  int spec_id;
  
  int bin_used = FindModifiedResource(ctx, spec_id);
  if (bin_used < 0) { return false; }  // collection failed, there's nothing to collect
  
  // Add this specification
  m_organism->IncCollectSpecCount(spec_id);
  
  return DoActualCollect(ctx, bin_used, env_remove, internal_add, probabilistic, unit);
}

bool cHardwareCPU::DoActualCollect(cAvidaContext& ctx, int bin_used, bool env_remove, bool internal_add, bool probabilistic, bool unit)
{
  // Set up res_change and max total
  const Apto::Array<double> res_count = m_organism->GetOrgInterface().GetResources(ctx);
  Apto::Array<double> res_change(res_count.GetSize());
  res_change.SetAll(0.0);
  double total = m_organism->GetRBinsTotal();
  double max = m_world->GetConfig().MAX_TOTAL_STORED.Get();
  
	/* First, if collection is probabilistic, check to see if it succeeds.
   *
   * If so, remove resource(s) from environment if env_remove is set;
   * add resource(s) to internal resource bins if internal_add is set
   * (and this would not fill the bin beyond max).
   */
  if (probabilistic) {
    double success_chance = res_count[bin_used] / double(m_world->GetConfig().COLLECT_PROB_DIVISOR.Get());
    if (success_chance < ctx.GetRandom().GetDouble()) { 
      return false; 
    }  // we define not collecting as failure
  }
  
  // Collect a unit (if possible) or some ABSORB_RESOURCE_FRACTION
  if (unit) {
    if (res_count[bin_used] >= 1.0) {
      res_change[bin_used] = -1.0;
    }
    else {
      return false;
    }  // failure: not enough to collect
  }
  else {
    res_change[bin_used] = -1 * (res_count[bin_used] * m_world->GetConfig().ABSORB_RESOURCE_FRACTION.Get());
  }

  if (internal_add && (max < 0 || (total + -1 * res_change[bin_used]) <= max)) { 
    m_organism->AddToRBin(bin_used, -1 * res_change[bin_used]); 
  }
  
  if (!env_remove || (max >= 0 && (total + -1 * res_change[bin_used]) > max)) {
    res_change[bin_used] = 0.0;
  }

  // Update resource counts to reflect res_change
  m_organism->GetOrgInterface().UpdateResources(ctx, res_change);
  return true;
}

/* Takes resource(s) from the environment and adds them to the internal resource
 * bins of the organism.
 */
bool cHardwareCPU::Inst_Collect(cAvidaContext& ctx)
{
  return DoCollect(ctx, true, true, false, false);
}

/* Like Inst_Collect, but the collected resources are not removed from the
 * environment.
 */
bool cHardwareCPU::Inst_CollectNoEnvRemove(cAvidaContext& ctx)
{
  return DoCollect(ctx, false, true, false, false);
}

/* Collects resource from the environment but does not add it to the organism,
 * effectively destroying it.
 */
bool cHardwareCPU::Inst_Destroy(cAvidaContext& ctx)
{
  return DoCollect(ctx, true, false, false, false);
}

/* A no-op, nop-modified in the same way as the "collect" instructions:
 * Does not remove resource from environment, does not add resource to organism */
bool cHardwareCPU::Inst_NopCollect(cAvidaContext& ctx)
{
  return DoCollect(ctx, false, false, false, false);
}

/* Collects one unit of resource from the environment and adds it to the internal 
 * resource bins of the organism.  The probability of the instruction succeeding 
 * is given by the level of that resource divided by the COLLECT_PROB_DIVISOR 
 * config option.
 */
bool cHardwareCPU::Inst_CollectUnitProbabilistic(cAvidaContext& ctx)
{
  return DoCollect(ctx, true, true, true, true);
}

/* Takes the resource specified by the COLLECT_RESOURCE_SPECIFIC config option
 * from the environment and adds it to the internal resource bins of the organism.
 */
bool cHardwareCPU::Inst_CollectSpecific(cAvidaContext& ctx)
{
  const int resource = m_world->GetConfig().COLLECT_SPECIFIC_RESOURCE.Get();
  double res_before = m_organism->GetRBin(resource);
  bool success = DoActualCollect(ctx, resource, true, true, false, false);
  double res_after = m_organism->GetRBin(resource);
  GetRegister(FindModifiedRegister(REG_BX)) = (int)(res_after - res_before);
  return success;
}

/*! Sense the level of resources in this organism's cell, and if all of the 
 resources present are above the min level for that resource, execute the following
 intruction.  Otherwise, skip the following instruction.
 */
bool cHardwareCPU::Inst_IfResources(cAvidaContext& ctx)
{
  // These are the current levels of resources at this cell:
  const Apto::Array<double> resources = m_organism->GetOrgInterface().GetResources(ctx);
  
  // Now we loop through the different reactions, checking to see if their
  // required resources are below what's available.  If so, we skip ahead an
  // instruction and return.
  const cReactionLib& rxlib = m_world->GetEnvironment().GetReactionLib();
  for (int i=0; i<rxlib.GetSize(); ++i) {
    cReaction* rx = rxlib.GetReaction(i);
    tLWConstListIterator<cReactionProcess> processes(rx->GetProcesses());
    while (!processes.AtEnd()) {
      const cReactionProcess* proc = processes.Next();
      cResourceDef* res = proc->GetResDef(); // Infinite resource == 0.
      if ((res != 0) && (resources[res->GetID()] < proc->GetMinNumber())) {
        getIP().Advance();
        return true;
      }
    }
  }
  return true;
}

void cHardwareCPU::DoDonate(cOrganism* to_org)
{
  assert(to_org != NULL);
  
  const double merit_given = m_world->GetConfig().MERIT_GIVEN.Get();
  const double merit_received = m_world->GetConfig().MERIT_RECEIVED.Get();
  
  double cur_merit = m_organism->GetPhenotype().GetMerit().GetDouble();
  cur_merit -= merit_given;
  if (cur_merit < 0) cur_merit=0; 
  
  // Plug the current merit back into this organism and notify the scheduler.
  m_organism->UpdateMerit(cur_merit);
  
  // Update the merit of the organism being donated to...
  double other_merit = to_org->GetPhenotype().GetMerit().GetDouble();
  other_merit += merit_received;
  to_org->UpdateMerit(other_merit);
}





bool cHardwareCPU::Inst_DonateRandom(cAvidaContext& ctx)
{
  
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorRand();
	
  // Turn to a random neighbor, get it, and turn back...
  int neighbor_id = ctx.GetRandom().GetInt(m_organism->GetNeighborhoodSize());
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism* neighbor = m_organism->GetNeighbor();
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
  
  // Donate only if we have found a neighbor.
  if (neighbor != NULL) {
    DoDonate(neighbor);
    
    neighbor->GetPhenotype().SetIsReceiverRand();
  }
	
  return true;
	
}


bool cHardwareCPU::Inst_DonateKin(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
  
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorKin();
	
	
  // Find the target as the first Kin found in the neighborhood.
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  // If there is no max distance, just take the random neighbor we're facing.
  const int max_dist = m_world->GetConfig().MAX_DONATE_KIN_DIST.Get();
  if (max_dist != -1) {
    int max_id = neighbor_id + num_neighbors;
    bool found = false;
    Systematics::GroupPtr bg = m_organism->SystematicsGroup("genotype");
    if (!bg) return false;
    Systematics::SexualAncestryPtr sa = bg->GetData<Systematics::SexualAncestry>();
    if (!sa) {
      sa = Systematics::SexualAncestryPtr(new Systematics::SexualAncestry(bg));
      bg->AttachData(sa);
    }
    
    while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();
      if (neighbor != NULL) {
        Systematics::GroupPtr nbg = neighbor->SystematicsGroup("genotype");
        assert(nbg);
        if (sa->GetPhyloDistance(nbg) <= max_dist) {
          found = true;
          break;
        }
      }
      m_organism->Rotate(ctx, 1);
      neighbor_id++;
    }
    if (found == false) neighbor = NULL;
  }
  
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
  
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL){
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverKin();
  }
  return true;
}

bool cHardwareCPU::Inst_DonateEditDist(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorEdit();
  
  // Find the target as the first Kin found in the neighborhood.
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism* neighbor = m_organism->GetNeighbor();
  
  // If there is no max edit distance, take the random neighbor we're facing.
  const int max_dist = m_world->GetConfig().MAX_DONATE_EDIT_DIST.Get();
  if (max_dist != -1) {
    int max_id = neighbor_id + num_neighbors;
    bool found = false;
    while (neighbor_id < max_id) {
      neighbor = m_organism->GetNeighbor();
      int edit_dist = max_dist + 1;
      if (neighbor != NULL) {
        const Genome& org_genome = m_organism->GetGenome();
        ConstInstructionSequencePtr org_seq_p;
        org_seq_p.DynamicCastFrom(org_genome.Representation());
        const InstructionSequence& org_seq = *org_seq_p;
        
        const Genome& neighbor_genome = neighbor->GetGenome();
        ConstInstructionSequencePtr neighbor_seq_p;
        neighbor_seq_p.DynamicCastFrom(neighbor_genome.Representation());
        const InstructionSequence& neighbor_seq = *neighbor_seq_p;
        
        edit_dist = InstructionSequence::FindEditDistance(org_seq, neighbor_seq);
      }
      if (edit_dist <= max_dist) {
        found = true;
				
        // Code to track the edit distance between edt donors and recipients
        const Genome& org_genome = m_organism->GetGenome();
        ConstInstructionSequencePtr org_seq_p;
        org_seq_p.DynamicCastFrom(org_genome.Representation());
        const InstructionSequence& org_seq = *org_seq_p;
        
        const Genome& neighbor_genome = neighbor->GetGenome();
        ConstInstructionSequencePtr neighbor_seq_p;
        neighbor_seq_p.DynamicCastFrom(neighbor_genome.Representation());
        const InstructionSequence& neighbor_seq = *neighbor_seq_p;
        
        const int edit_dist = InstructionSequence::FindEditDistance(org_seq, neighbor_seq);
				
        /*static ofstream edit_file("edit_dists.dat");*/
        static int num_edit_donates = 0;
        static int num_edit_donates_15_dist = 0;
        static int tot_dist_edit_donate = 0;
				
        num_edit_donates++;
        if (edit_dist > 15) num_edit_donates_15_dist++;
        tot_dist_edit_donate += edit_dist;
        
        if (num_edit_donates == 1000) {
          /*
           edit_file << num_edit_donates << " "
           << (double) num_edit_donates_15_dist / (double) num_edit_donates << " "
           << (double) tot_dist_edit_donate / (double) num_edit_donates << endl;
           */
					
          num_edit_donates = 0;
          num_edit_donates_15_dist = 0;
          tot_dist_edit_donate = 0;
        }
        
        break;
      }
      m_organism->Rotate(ctx, 1);
      neighbor_id++;
    }
    if (found == false) neighbor = NULL;
  }
  
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
  
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL){
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverEdit();
  }
  return true;
	
}

bool cHardwareCPU::Inst_GetFacedEditDistance(cAvidaContext& ctx)
{
  if (!m_organism->IsNeighborCellOccupied()) return false;
  
  cOrganism* target = NULL;
  target = m_organism->GetOrgInterface().GetNeighbor();

  const Genome& org_genome = m_organism->GetGenome();
  ConstInstructionSequencePtr org_seq_p;
  org_seq_p.DynamicCastFrom(org_genome.Representation());
  const InstructionSequence& org_seq = *org_seq_p;
  
  const Genome& target_genome = target->GetGenome();
  ConstInstructionSequencePtr target_seq_p;
  target_seq_p.DynamicCastFrom(target_genome.Representation());
  const InstructionSequence& target_seq = *target_seq_p;
  
  GetRegister(FindModifiedRegister(REG_BX)) = InstructionSequence::FindEditDistance(org_seq, target_seq);
  
  return true;
}

bool cHardwareCPU::Inst_DonateGreenBeardGene(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome (see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  phenotype.IncDonates();
  phenotype.SetIsDonorGbg();
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
    neighbor = m_organism->GetNeighbor();
		
    //if neighbor exists, do they have the green beard gene?
    if (neighbor != NULL) {
      const Genome& neighbor_gen = neighbor->GetGenome();
      ConstInstructionSequencePtr neighbor_seq_p;
      neighbor_seq_p.DynamicCastFrom(neighbor_gen.Representation());
      const InstructionSequence& neighbor_genome = *neighbor_seq_p;
      
      // for each instruction in the genome...
      for (int i = 0; i < neighbor_genome.GetSize(); i++){
        
        // ...see if it is donate-gbg
        if (neighbor_genome[i] == getIP().GetInst()) {
          found = true;
          break;
        }
        
      }
    }
		
    // stop searching through the neighbors if we already found one
    if (found == true){
      break;
    }
    
    m_organism->Rotate(ctx, 1);
    neighbor_id++;
  }
	
  if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverGbg();
  }
  
  return true;
  
}

/* This instruction donates to other organisms that have at least
 as many donate-shaded-greenbeard instructions in their organism 
 as this organism does. */
bool cHardwareCPU::Inst_DonateShadedGreenBeard(cAvidaContext& ctx)
{
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  // Determine if this m_organism is below the threshold and thus eligible to donate.
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  // Identify how many green beard donations the parent of this organism made
  
  // Identify how many shaded green beard donations this organisms made
  // First figure out what number instruction donate-shadedgb is
  const int num_inst = m_inst_set->GetSize();
  int shade_of_gb = 0;
  int neighbor_shade_of_gb = 0;
  int inst_number = 0;
  for (int i = 0; i < num_inst; i++) { 
    if ((m_inst_set->GetName(i) == "donate-shadedgb") && (phenotype.GetTestCPUInstCount().GetSize() > 0)) {
      shade_of_gb = phenotype.GetTestCPUInstCount()[i];
      inst_number = i;
    }
  }
	
	
  // Update stats.
  phenotype.IncDonates();
  phenotype.SetIsDonorShadedGb();
  phenotype.IncNumShadedGbDonations();
	
  // Find the target as the first match found in the neighborhood.
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  // Part of the reason the donates fail so frequently is that this code
  // although it randomizes the neighbor, does not take into account whether
  // a neigbhor is there or not. 
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
    neighbor = m_organism->GetNeighbor();
    //if neighbor exists, AND if their parent attempted to donate >= shaded of green beard,
    if (neighbor != NULL) {
      
      // Get the neighbor's shade
      neighbor_shade_of_gb = 0; 
      if (neighbor->GetPhenotype().GetTestCPUInstCount().GetSize() > 0) { 
        neighbor_shade_of_gb = neighbor->GetPhenotype().GetTestCPUInstCount()[inst_number];
      }
      
      // Changing this line makes shaded gb ONLY donate to organisms with the exact same 
      // shade (color/number of donations)
      //			if (neighbor_shade_of_gb >=  shade_of_gb) {
      if (neighbor_shade_of_gb ==  shade_of_gb) {	
        // Code to track the edit distance between shaded donors and recipients
        const Genome& org_genome = m_organism->GetGenome();
        ConstInstructionSequencePtr org_seq_p;
        org_seq_p.DynamicCastFrom(org_genome.Representation());
        const InstructionSequence& org_seq = *org_seq_p;
        
        const Genome& neighbor_genome = neighbor->GetGenome();
        ConstInstructionSequencePtr neighbor_seq_p;
        neighbor_seq_p.DynamicCastFrom(neighbor_genome.Representation());
        const InstructionSequence& neighbor_seq = *neighbor_seq_p;
        
        const int edit_dist = InstructionSequence::FindEditDistance(org_seq, neighbor_seq);
				
        /*static ofstream gb_file("shaded_gb_dists.dat");*/
        static int num_gb_donates = 0;
        static int num_gb_donates_15_dist = 0;
        static int tot_dist_gb_donate = 0;
				
        num_gb_donates++;
        if (edit_dist > 15) num_gb_donates_15_dist++;
        tot_dist_gb_donate += edit_dist;
				
        if (num_gb_donates == 1000) {
          /*
           gb_file << num_gb_donates << " "
           << (double) num_gb_donates_15_dist / (double) num_gb_donates << " "
           << (double) tot_dist_gb_donate / (double) num_gb_donates << endl;
           */
					
          num_gb_donates = 0;
          num_gb_donates_15_dist = 0;
          tot_dist_gb_donate = 0;
        }
				
        found = true;
      }
    }
		
    // stop searching through the neighbors if we already found one
    if (found == true){
      break;
    }
		
    m_organism->Rotate(ctx, 1);
    neighbor_id++;
  }
	
  if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverShadedGb();
  }
	
  return true;
	
}



bool cHardwareCPU::Inst_DonateTrueGreenBeard(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome AND their parents excuted it
  //(see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  phenotype.IncDonates();
  phenotype.SetIsDonorTrueGb();
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Get greenbeard instruction number
  const int num_inst = m_inst_set->GetSize();
  int inst_number = 0;
  for (int i = 0; i < num_inst; i++) { 
    if (m_inst_set->GetName(i) == "donate-tgb") {
      inst_number = i;
    }
  }
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
    neighbor = m_organism->GetNeighbor();
    //if neighbor is a green beard
    if (neighbor->GetPhenotype().GetTestCPUInstCount()[inst_number]) {
      found = true;
    }
		
    // stop searching through the neighbors if we already found one
    if (found == true){
      break;
    }
		
    m_organism->Rotate(ctx, 1);
    neighbor_id++;
  }
	
  if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverTrueGb();
  }
	
  
  return true;	
  
}

bool cHardwareCPU::Inst_DonateThreshGreenBeard(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome AND their parents excuted it >=THRESHOLD number of times
  //(see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
	
  phenotype.IncDonates();
  phenotype.SetIsDonorThreshGb();
  phenotype.IncNumThreshGbDonations();
	
	
  // Identify how many thresh green beard donations this organisms made
  // First figure out what number instruction donate-threshgb is	
  const int num_inst = m_inst_set->GetSize();
  int neighbor_thresh_of_gb = 0;
  int inst_number = 0;
  for (int i = 0; i < num_inst; i++) { 
    if ((m_inst_set->GetName(i) == "donate-threshgb") && (phenotype.GetTestCPUInstCount().GetSize() > 0)) {
      inst_number = i;
    }
  }
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
    neighbor = m_organism->GetNeighbor();
    //if neighbor exists, AND if their parent attempted to donate >= threshhold,
    if (neighbor != NULL) {
			
      // Get neighbor threshold
      neighbor_thresh_of_gb = 0; 
      if (neighbor->GetPhenotype().GetTestCPUInstCount().GetSize() > 0) { 
        neighbor_thresh_of_gb = neighbor->GetPhenotype().GetTestCPUInstCount()[inst_number];
      }
			
      if (neighbor_thresh_of_gb >= m_world->GetConfig().MIN_GB_DONATE_THRESHOLD.Get() ) {
        const Genome& org_gen = m_organism->GetGenome();
        ConstInstructionSequencePtr org_seq_p;
        org_seq_p.DynamicCastFrom(org_gen.Representation());
        const InstructionSequence& org_seq = *org_seq_p;

        const Genome& neighbor_gen = neighbor->GetGenome();
        ConstInstructionSequencePtr neighbor_seq_p;
        neighbor_seq_p.DynamicCastFrom(neighbor_gen.Representation());
        const InstructionSequence& neighbor_seq = *neighbor_seq_p;
        
        // Code to track the edit distance between tgb donors and recipients
        const int edit_dist = InstructionSequence::FindEditDistance(org_seq, neighbor_seq);
				
        /*static ofstream tgb_file("thresh_gb_dists.dat");*/
        static int num_tgb_donates = 0;
        static int num_tgb_donates_15_dist = 0;
        static int tot_dist_tgb_donate = 0;
        
        num_tgb_donates++;
        if (edit_dist > 15) num_tgb_donates_15_dist++;
        tot_dist_tgb_donate += edit_dist;
				
        if (num_tgb_donates == 1000) {
          /*
           tgb_file << num_tgb_donates << " "
           << (double) num_tgb_donates_15_dist / (double) num_tgb_donates << " "
           << (double) tot_dist_tgb_donate / (double) num_tgb_donates << endl;
           */
          
          num_tgb_donates = 0;
          num_tgb_donates_15_dist = 0;
          tot_dist_tgb_donate = 0;
        }
        
        // for each instruction in the genome...
        for (int i=0;i<neighbor_seq.GetSize();i++){
					
          // ...see if it is donate-threshgb, if so, we found a target
          if (neighbor_seq[i] == getIP().GetInst()) {
            found = true;
            break;
          }
					
        }
      }
    }
    
    // stop searching through the neighbors if we already found one
    if (found == true){
      break;
    }
		
    m_organism->Rotate(ctx, 1);
    neighbor_id++;
  }
	
  if (found == false) neighbor = NULL;
  
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverThreshGb();
    // cout << "************ neighbor->GetPhenotype().GetNumThreshGbDonationsLast() is " << neighbor->GetPhenotype().GetNumThreshGbDonationsLast() << endl;
    
  }
	
  return true;
}


bool cHardwareCPU::Inst_DonateQuantaThreshGreenBeard(cAvidaContext& ctx)
{
  // this donates to organisms that have this instruction anywhere
  // in their genome AND their parents excuted it more than a
  // THRESHOLD number of times where that threshold depend on the
  // number of times the individual's parents attempted to donate
  // using this instruction.  The threshold levels are multiples of
  // the quanta value set in avida.cfg, and the highest level that
  // the donor qualifies for is the one used.
	
  // (see Dawkins 1976, The Selfish Gene, for 
  // the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (phenotype.GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  phenotype.IncDonates();
  phenotype.SetIsDonorQuantaThreshGb();
  phenotype.IncNumQuantaThreshGbDonations();
  //cout << endl << "quanta_threshgb attempt.. " ;
	
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // Get the quanta (step size) between threshold levels.
  const int donate_quanta = m_world->GetConfig().DONATE_THRESH_QUANTA.Get();
  
  // Calculate what quanta level we should be at for this individual.  We do a
  // math trick to make sure its the next lowest event multiple of donate_quanta.
  const int quanta_donate_thresh =
	(phenotype.GetNumQuantaThreshGbDonationsLast() / donate_quanta) * donate_quanta;
  //cout << " phenotype.GetNumQuantaThreshGbDonationsLast() is " << phenotype.GetNumQuantaThreshGbDonationsLast();
  //cout << " quanta thresh=  " << quanta_donate_thresh;
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
    neighbor = m_organism->GetNeighbor();
    //if neighbor exists, AND if their parent attempted to donate >= threshhold,
    if (neighbor != NULL &&
        neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast() >= quanta_donate_thresh) {
      
      const Genome& neighbor_gen = neighbor->GetGenome();
      ConstInstructionSequencePtr neighbor_seq_p;
      neighbor_seq_p.DynamicCastFrom(neighbor_gen.Representation());
      const InstructionSequence& neighbor_genome = *neighbor_seq_p;
      
      // for each instruction in the genome...
      for (int i=0;i<neighbor_genome.GetSize();i++){
        
        // ...see if it is donate-quantagb, if so, we found a target
        if (neighbor_genome[i] == getIP().GetInst()) {
          found = true;
          break;
        }
				
      }
    }
    
    // stop searching through the neighbors if we already found one
    if (found == true) {
      break;
    }
    
    m_organism->Rotate(ctx, 1);
    neighbor_id++;
  }
	
  if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
	
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverQuantaThreshGb();
    //cout << " ************ neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast() is " << neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast();
    
  }
	
  return true;
  
}


bool cHardwareCPU::Inst_DonateGreenBeardSameLocus(cAvidaContext& ctx)
{
  // This instruction donates to organisms that have a matching instruction
  // at the same position in their genome AND their parents excuted it.
  
  cPhenotype & phenotype = m_organism->GetPhenotype();
	
  if (phenotype.GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
	
  int donate_locus = getIP().GetPosition();
  
  phenotype.IncDonates();
  phenotype.SetIsDonorPosition(donate_locus);
  phenotype.IncNumGreenBeardSameLocus();
	
  // Find the target as the first match found in the neighborhood.
	
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  // We have not found a match yet
  bool found = false;
	
  // Rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
    neighbor = m_organism->GetNeighbor();
    // If neighbor exists, AND if their parent attempted to donate at this position.
    if (neighbor != NULL && neighbor->GetPhenotype().IsDonorPositionLast(donate_locus)) {
      const Genome& neighbor_gen = neighbor->GetGenome();
      ConstInstructionSequencePtr neighbor_seq_p;
      neighbor_seq_p.DynamicCastFrom(neighbor_gen.Representation());
      const InstructionSequence& neighbor_seq = *neighbor_seq_p;
      // See if this organism has a donate at the correct position.
      if (neighbor_seq.GetSize() > donate_locus && neighbor_seq[donate_locus] == getIP().GetInst()) {
        found = true;
        break;
      }
    }
		
    m_organism->Rotate(ctx, 1);
    neighbor_id++;
  }
	
  if (found == false) neighbor = NULL;
	
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, -1);
	
  // Donate only if we have found a valid receiver
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverGBSameLocus();
  }
	
  return true;
  
}


bool cHardwareCPU::Inst_DonateNULL(cAvidaContext&)
{
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
  
  m_organism->GetPhenotype().IncDonates();
  m_organism->GetPhenotype().SetIsDonorNull();
  
  // This is a fake donate command that causes the organism to lose merit,
  // but no one else to gain any.
  
  const double merit_given = m_world->GetConfig().MERIT_GIVEN.Get();
  double cur_merit = m_organism->GetPhenotype().GetMerit().GetDouble();
  cur_merit -= merit_given;
  
  // Plug the current merit back into this organism and notify the scheduler.
  m_organism->UpdateMerit(cur_merit);
  
  return true;
}








// Move a fraction of the given resource present at the current cell to the specified cell.
void cHardwareCPU::DoResourceDonatePercent(cAvidaContext& ctx, const int to_cell, const int resource_id, const double frac_resource_given)
{
  assert(to_cell >= 0);
  assert(resource_id >= 0);
  assert(frac_resource_given >= 0);
  assert(frac_resource_given <= 1);
  
  const Apto::Array<double> &resources = m_organism->GetOrgInterface().GetResources(ctx);
  if (resource_id >= resources.GetSize()) return;
  
  const double amount = max(0.0, frac_resource_given * resources[resource_id]);
  
  DoResourceDonateAmount(ctx, to_cell, resource_id, amount); 
  
} //End DoResourceDonatePercent()


// Donate a portion of the given resource present at the current cell to the specified cell.
void cHardwareCPU::DoResourceDonateAmount(cAvidaContext& ctx, const int to_cell, const int resource_id, const double amount)
{  
  assert(to_cell >= 0);
  assert(amount >= 0);
  assert(resource_id >= 0);
  
  const Apto::Array<double> &src_resources = m_organism->GetOrgInterface().GetResources(ctx);
  const Apto::Array<double> &dest_resources = m_world->GetPopulation().GetResources().GetCellResources(to_cell, ctx);
  
  assert(resource_id < src_resources.GetSize());
  assert(resource_id < dest_resources.GetSize());
  
  const double donation = min(amount, src_resources[resource_id]);
  const double decay = 0.0; //m_world->GetConfig().RESOURCE_SHARING_LOSS.Get();
  
  assert(decay >= 0);
  assert(decay <= 1);
  
  Apto::Array<double> src_change;
  Apto::Array<double> dest_change;
  
  src_change.Resize(src_resources.GetSize(), 0);
  dest_change.Resize(dest_resources.GetSize(), 0);
  
  src_change[resource_id] = -1 * donation;
  dest_change[resource_id] = (1 - decay) * donation;
  
  m_organism->GetOrgInterface().UpdateResources(ctx, src_change);
  m_world->GetPopulation().GetResources().UpdateCellResources(ctx, dest_change, to_cell);
  
} //End DoResourceDonateAmount()


//Donate a fraction of nop-specified resource at organism's location to cell faced
bool cHardwareCPU::DonateResourceX(cAvidaContext& ctx, const int res_id)
{
  assert(m_organism != 0);
  assert(res_id >= 0);
  
  const double pct = 0.1;
  
  int current_cell, faced_cell;
  
  current_cell = m_organism->GetCellID();
	
  if (current_cell == -1) {
    return false;
  }
  
  cPopulation& pop = m_world->GetPopulation();
  faced_cell = pop.GetCell(current_cell).GetCellFaced().GetID();
  
  if (faced_cell == -1) {
    return false;
  }
  
  DoResourceDonatePercent(ctx, faced_cell, res_id, pct); 
  
  return true;
  
} //End DonateResourceX()


//Donate a fraction of nop-specified resource at organism's location to cell faced
bool cHardwareCPU::Inst_DonateResource0(cAvidaContext& ctx)
{
  return DonateResourceX(ctx ,0);  
} //End Inst_DonateResource0()


//Donate a fraction of nop-specified resource at organism's location to cell faced
bool cHardwareCPU::Inst_DonateResource1(cAvidaContext& ctx)
{
  return DonateResourceX(ctx, 1);
} //End Inst_DonateResource1()

//Donate a fraction of nop-specified resource at organism's location to cell faced
bool cHardwareCPU::Inst_DonateResource2(cAvidaContext& ctx)
{
  return DonateResourceX(ctx, 2);
} //End Inst_DonateResource2()


/*Donates resources to the a neighboring cell */
bool cHardwareCPU::Inst_DonateSpecific(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get() ||
      (m_world->GetConfig().MAX_DONATE_EDIT_DIST.Get() > 0 && m_organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATE_EDIT_DIST.Get())) {
    return false;
  }
  if (!m_organism->IsNeighborCellOccupied()) return false;
  
  cOrganism* target = NULL;
  target = m_organism->GetOrgInterface().GetNeighbor();
  const int resource = m_world->GetConfig().COLLECT_SPECIFIC_RESOURCE.Get();
  if (m_world->GetConfig().USE_RESOURCE_BINS.Get()){
    double res_before = m_organism->GetRBin(resource);
    if (res_before >= 1) {
      target->AddToRBin (resource, 1);
      m_organism->GetPhenotype().IncDonates();
      m_organism->GetPhenotype().SetIsDonorEdit();
      target->GetPhenotype().SetIsReceiverEdit();
      
      const Genome& org_genome = m_organism->GetGenome();
      ConstInstructionSequencePtr org_seq_p;
      org_seq_p.DynamicCastFrom(org_genome.Representation());
      const InstructionSequence& org_seq = *org_seq_p;
      
      const Genome& target_genome = target->GetGenome();
      ConstInstructionSequencePtr target_seq_p;
      target_seq_p.DynamicCastFrom(target_genome.Representation());
      const InstructionSequence& target_seq = *target_seq_p;
      
      InstructionSequence::FindEditDistance(org_seq, target_seq);
      return true;
    }
  }
  return false;
}

bool cHardwareCPU::Inst_SearchF(cAvidaContext&)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  const int search_size = FindLabel(1).GetPosition() - getIP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  return true;
}

bool cHardwareCPU::Inst_SearchB(cAvidaContext&)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  const int search_size = getIP().GetPosition() - FindLabel(-1).GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  return true;
}

bool cHardwareCPU::Inst_MemSize(cAvidaContext&)
{
  GetRegister(FindModifiedRegister(REG_BX)) = m_memory.GetSize();
  return true;
}

bool cHardwareCPU::Inst_IOBufAdd1(cAvidaContext&)
{ 
  m_organism->AddOutput(1);
  return true;
}
bool cHardwareCPU::Inst_IOBufAdd0(cAvidaContext&)
{ 
  m_organism->AddOutput(0);
  return true; 
}

bool cHardwareCPU::Inst_RotateL(cAvidaContext& ctx)
{
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // If this organism has no neighbors, ignore rotate.
  if (num_neighbors == 0) return false;
  
  ReadLabel();
  
  // Always rotate at least once.
  m_organism->Rotate(ctx, -1);
  
  // If there is no label, then the one rotation was all we want.
  if (!GetLabel().GetSize()) return true;
  
  // Rotate until a complement label is found (or all have been checked).
  GetLabel().Rotate(1, NUM_NOPS);
  for (int i = 1; i < num_neighbors; i++) {
    cOrganism* neighbor = m_organism->GetNeighbor();
    if (neighbor != NULL) {
      cHeadCPU search_head(&neighbor->GetHardware());
      FindLabelInMemory(GetLabel(), search_head);
      if (search_head.InMemory()) return true;
    }
    
    // Otherwise keep rotating...
    m_organism->Rotate(ctx, 1);
  }
  return true;
}

bool cHardwareCPU::Inst_RotateR(cAvidaContext& ctx)
{
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  
  // If this organism has no neighbors, ignore rotate.
  if (num_neighbors == 0) return false;
  
  ReadLabel();
  
  // Always rotate at least once.
  m_organism->Rotate(ctx, 1);
  
  // If there is no label, then the one rotation was all we want.
  if (!GetLabel().GetSize()) return true;
  
  // Rotate until a complement label is found (or all have been checked).
  GetLabel().Rotate(1, NUM_NOPS);
  for (int i = 1; i < num_neighbors; i++) {
    cOrganism* neighbor = m_organism->GetNeighbor();
    if (neighbor != NULL) {
      cHeadCPU search_head(&neighbor->GetHardware());
      FindLabelInMemory(GetLabel(), search_head);
      if (search_head.InMemory()) return true;
    }
    
    // Otherwise keep rotating...
    m_organism->Rotate(ctx, -1);
  }
  return true;
}

bool cHardwareCPU::Inst_RotateLeftOne(cAvidaContext& ctx)
{
  m_organism->Rotate(ctx, 1);
  return true;
}

bool cHardwareCPU::Inst_RotateRightOne(cAvidaContext& ctx)
{
  m_organism->Rotate(ctx, -1);
  return true;
}

/**
 Rotate to facing specified by following label
 */
bool cHardwareCPU::Inst_RotateLabel(cAvidaContext& ctx)
{
  int standardNeighborhoodSize, actualNeighborhoodSize, newFacing, currentFacing;
  actualNeighborhoodSize = m_organism->GetNeighborhoodSize();
  
  ReadLabel();
  if (m_world->GetConfig().WORLD_GEOMETRY.Get() == nGeometry::TORUS ||
      m_world->GetConfig().WORLD_GEOMETRY.Get() == nGeometry::GRID) {
    standardNeighborhoodSize = 8;
  } else {
    exit(-1);
  }
  newFacing = GetLabel().AsIntGreyCode(NUM_NOPS) % standardNeighborhoodSize;
  
  for (int i = 0; i < actualNeighborhoodSize; i++) {
    currentFacing = m_organism->GetFacing();
    if (newFacing == currentFacing)
      break;
    m_organism->Rotate(ctx, 1);
  }
  return true;
}

bool cHardwareCPU::Inst_RotateUnoccupiedCell(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  for (int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if (!m_organism->IsNeighborCellOccupied()) { // faced cell is unoccupied
      GetRegister(reg_used) = 1;      
      return true;
    }
    m_organism->Rotate(ctx, 1); // continue to rotate
  }  
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_RotateOccupiedCell(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  for (int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if (m_organism->IsNeighborCellOccupied()) { // faced cell is occupied
      GetRegister(reg_used) = 1;      
      return true;
    }
    m_organism->Rotate(ctx, 1); // continue to rotate
  }  
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_RotateNextOccupiedCell(cAvidaContext& ctx)
{
  m_organism->Rotate(ctx, 1);
  return Inst_RotateOccupiedCell(ctx);
}

bool cHardwareCPU::Inst_RotateNextUnoccupiedCell(cAvidaContext& ctx)
{
  m_organism->Rotate(ctx, 1); // continue to rotate
  return Inst_RotateUnoccupiedCell(ctx);
}

bool cHardwareCPU::Inst_RotateEventCell(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  for (int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if (m_organism->GetCellData() > 0) { // event in faced cell
      GetRegister(reg_used) = 1;      
      return true;
    }
    m_organism->Rotate(ctx, 1); // continue to rotate
  }  
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_RotateUphill(cAvidaContext& ctx)
{
  int actualNeighborhoodSize = m_organism->GetNeighborhoodSize();  
  int opinion = 0;
  
  if(m_organism->GetOrgInterface().HasOpinion(m_organism)) opinion = m_organism->GetOpinion().first; 
  
  const Apto::Array<double> current_res = m_organism->GetOrgInterface().GetResources(ctx);
  double max_res = 0;
  for(int i = 0; i < actualNeighborhoodSize; i++) {
    m_organism->Rotate(ctx, 1);
    Apto::Array<double> faced_res = m_organism->GetOrgInterface().GetFacedCellResources(ctx);
    if (faced_res[opinion] > max_res) max_res = faced_res[opinion];
  } 
  
  if (max_res > current_res[opinion]) {
    for(int i = 0; i < actualNeighborhoodSize; i++) {
      Apto::Array<double> faced_res = m_organism->GetOrgInterface().GetFacedCellResources(ctx);
      if (faced_res[opinion] != max_res) m_organism->Rotate(ctx, 1);
    }
  }
  // return % change
  int res_diff = 0;
  if (current_res[opinion] == 0) res_diff = (int) max_res;
  else res_diff = (int) (((max_res - current_res[opinion])/current_res[opinion]) * 100 + 0.5);
  int reg_to_set = FindModifiedRegister(REG_BX);
  GetRegister(reg_to_set) = res_diff;
  return true;
}

bool cHardwareCPU::Inst_RotateHome(cAvidaContext& ctx)
{
  // Will rotate organism to face birth cell if org never used zero-easterly or zero-northerly. Otherwise will rotate org
  // to face the 'marked' spot where those instructions were executed.
  int easterly = m_organism->GetEasterly();
  int northerly = m_organism->GetNortherly();
  int correct_facing = 0;
  if (northerly > 0 && easterly == 0) correct_facing = 0; // rotate N    
  else if (northerly > 0 && easterly < 0) correct_facing = 1; // rotate NE
  else if (northerly == 0 && easterly < 0) correct_facing = 2; // rotate E
  else if (northerly < 0 && easterly < 0) correct_facing = 3; // rotate SE
  else if (northerly < 0 && easterly == 0) correct_facing = 4; // rotate S
  else if (northerly < 0 && easterly > 0) correct_facing = 5; // rotate SW
  else if (northerly == 0 && easterly > 0) correct_facing = 6; // rotate W
  else if (northerly > 0 && easterly > 0) correct_facing = 7; // rotate NW  
  for (int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    m_organism->Rotate(ctx, 1);
    if (m_organism->GetFacedDir() == correct_facing) break;
  }
  return true;
}

bool cHardwareCPU::Inst_SetCopyMut(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int new_mut_rate = Apto::Max(GetRegister(reg_used), 1 );
  m_organism->SetCopyMutProb(static_cast<double>(new_mut_rate) / 10000.0);
  return true;
}

bool cHardwareCPU::Inst_ModCopyMut(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const double new_mut_rate = m_organism->GetCopyMutProb() + static_cast<double>(GetRegister(reg_used)) / 10000.0;
  if (new_mut_rate > 0.0) m_organism->SetCopyMutProb(new_mut_rate);
  return true;
}

// @WRE addition for movement
// Tumble sets the organism and cell to a new random facing
// 
bool cHardwareCPU::Inst_Tumble(cAvidaContext& ctx)
{
  // Get number of neighbor cells that the organism can move to.
  const int num_neighbors = m_organism->GetNeighborhoodSize();
  // Exclude extreme case of the completely disconnected cell
  if (0 < num_neighbors) {
    // Choose a base 0 random number of turns to make in facing, [0 .. num_neighbors-2].
    int irot = ctx.GetRandom().GetUInt(num_neighbors-1);
    // Treat as base 0 number of turns to make
    for (int i = 0; i <= irot; i++) {
      m_organism->Rotate(ctx, 1);
    }
  }
  // Logging
  // ofstream tumblelog;
  // tumblelog.open("data/tumblelog.txt",ios::app);
  // tumblelog << organism->GetID() << "," << irot << endl;
  // tumblelog.close();
  
  return true;
}


bool cHardwareCPU::Inst_SGMove(cAvidaContext&)
{
  assert(m_ext_mem.GetSize() > 3);
  
  const cStateGrid& sg = m_organism->GetStateGrid();
  
  int& x = m_ext_mem[0];
  int& y = m_ext_mem[1];
  
  const int facing = m_ext_mem[2];
  
  // State grid is treated as a 2-dimensional toroidal grid with size [0, width) and [0, height)
  // State grid is treated as a 2-dimensional toroidal grid with size [0, width) and [0, height)
  switch (facing) {
    case 0: // N
      if (++y == sg.GetHeight()) y = 0;
      break;
      
    case 1: // NE
      if (++x == sg.GetWidth()) x = 0;
      if (++y == sg.GetHeight()) y = 0;
      break;
      
    case 2: // E
      if (++x == sg.GetWidth()) x = 0;
      break;
      
    case 3: // SE
      if (++x == sg.GetWidth()) x = 0;
      if (--y == -1) y = sg.GetHeight() - 1;
      break;
      
    case 4: // S
      if (--y == -1) y = sg.GetHeight() - 1;
      break;
      
    case 5: // SW
      if (--x == -1) x = sg.GetWidth() - 1;
      if (--y == -1) y = sg.GetHeight() - 1;
      break;
      
    case 6: // W
      if (--x == -1) x = sg.GetWidth() - 1;
      break;
      
    case 7: // NW
      if (--x == -1) x = sg.GetWidth() - 1;
      if (++y == sg.GetHeight()) y = 0;
      break;
      
    default:
      assert(facing >= 0 && facing <= 7);
  }
  
  // Increment state observed count
  m_ext_mem[3 + sg.GetStateAt(x, y)]++;
  
  // Save this location in the movement history
  m_ext_mem.Push(sg.GetIDFor(x, y));
  return true;
}

bool cHardwareCPU::Inst_SGRotateL(cAvidaContext&)
{
  assert(m_ext_mem.GetSize() > 3);
  if (--m_ext_mem[2] < 0) m_ext_mem[2] = 7;
  return true;
}

bool cHardwareCPU::Inst_SGRotateR(cAvidaContext&)
{
  assert(m_ext_mem.GetSize() > 3);
  if (++m_ext_mem[2] > 7) m_ext_mem[2] = 0;
  return true;
}

bool cHardwareCPU::Inst_SGSense(cAvidaContext&)
{
  const cStateGrid& sg = m_organism->GetStateGrid();
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = sg.SenseStateAt(m_ext_mem[0], m_ext_mem[1]);
  return true; 
}

// @WRE addition for movement
// Move uses the cPopulation::SwapCells method to move an m_organism to a different cell
// and the cPopulation::MoveOrganisms helper function to clean up after a move
// The cell selected as a destination is the one faced
bool cHardwareCPU::Inst_Move(cAvidaContext& ctx)
{
  // In TestCPU, movement fails...
  if (m_organism->GetCellID() == -1) return false;
  
  bool move_success = m_organism->Move(ctx);  
  const int out_reg = FindModifiedRegister(REG_BX);   
  GetRegister(out_reg) = move_success;   
  return true;
}

bool cHardwareCPU::Inst_MoveToEvent(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  for (int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if (m_organism->GetNeighborCellContents() > 0) { 
      Inst_Move(ctx);
      GetRegister(reg_used) = 1;
      return true;
    }
    m_organism->Rotate(ctx, 1);
  }
  Inst_Move(ctx);
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareCPU::Inst_IfNeighborEventInUnoccupiedCell(cAvidaContext& ctx)
{
  for (int i = 0; i < m_organism->GetNeighborhoodSize(); i++) {
    if (m_organism->GetNeighborCellContents() > 0 && !m_organism->IsNeighborCellOccupied()) { 
      return true;
    }
    m_organism->Rotate(ctx, 1);
  }
  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfFacingEventCell(cAvidaContext&)
{
  if (m_organism->GetNeighborCellContents() > 0) { 
    return true;
  }
  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfEventInCell(cAvidaContext&)
{
  if (m_organism->GetCellData() > 0) { 
    return true;
  }
  getIP().Advance();
  return true;
}

// Multi-threading.
bool cHardwareCPU::Inst_ForkThread(cAvidaContext&)
{
  getIP().Advance();
  if (!ForkThread()) m_organism->Fault(FAULT_LOC_THREAD_FORK, FAULT_TYPE_FORK_TH);
  return true;
}

bool cHardwareCPU::Inst_ForkThreadLabel(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  // If there is no label, then do normal fork behavior
  if (GetLabel().GetSize() == 0) {
    return Inst_ForkThread(ctx);
  }
  
  cHeadCPU searchHead = FindLabel(+1);
  if ( searchHead.GetPosition() != getIP().GetPosition() ) {
    int save_pos = getIP().GetPosition();
    getIP().Set(searchHead.GetPosition() + 1);
    if (!ForkThread()) m_organism->Fault(FAULT_LOC_THREAD_FORK, FAULT_TYPE_FORK_TH);
    getIP().Set( save_pos );
  }
  
  return true;
}

bool cHardwareCPU::Inst_ForkThreadLabelIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) {
    ReadLabel();
    return false;
  }
  return Inst_ForkThreadLabel(ctx);
}

bool cHardwareCPU::Inst_ForkThreadLabelIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0) {
    ReadLabel();
    return false;
  }
  return Inst_ForkThreadLabel(ctx);
}

bool cHardwareCPU::Inst_KillThread(cAvidaContext&)
{
  if (!KillThread()) m_organism->Fault(FAULT_LOC_THREAD_KILL, FAULT_TYPE_KILL_TH);
  else m_advance_ip = false;
  return true;
}

bool cHardwareCPU::Inst_ThreadID(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = m_threads[m_cur_thread].GetID();
  return true;
}


// Head-based instructions

bool cHardwareCPU::Inst_SetHead(cAvidaContext&)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  m_threads[m_cur_thread].cur_head = static_cast<unsigned char>(head_used);
  return true;
}

bool cHardwareCPU::Inst_AdvanceHead(cAvidaContext&)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_WRITE);
  getHead(head_used).Advance();
  return true;
}

bool cHardwareCPU::Inst_MoveHead(cAvidaContext&)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  const int target = nHardware::HEAD_FLOW;
  getHead(head_used).Set(getHead(target));
  if (head_used == nHardware::HEAD_IP) m_advance_ip = false;
  return true;
}

bool cHardwareCPU::Inst_JumpHead(cAvidaContext&)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  getHead(head_used).Jump(GetRegister(REG_CX) );
  // @JEB - probably shouldn't advance IP after jumping here?
  // Any negative number jumps to the beginning of the genome (pos 0)
  // and then we immediately advance past that first instruction.
  return true;
}

bool cHardwareCPU::Inst_GetHead(cAvidaContext&)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  GetRegister(REG_CX) = getHead(head_used).GetPosition();
  return true;
}

bool cHardwareCPU::Inst_IfLabel(cAvidaContext&)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  if (GetLabel() != GetReadLabel())  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLabelDirect(cAvidaContext&)
{
  ReadLabel();
  if (GetLabel() != GetReadLabel())  getIP().Advance();
  return true;
}

// This is a variation on IfLabel that will skip the next command if the "if"
// is false, but it will also skip all nops following that command.
bool cHardwareCPU::Inst_IfLabel2(cAvidaContext&)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  if (GetLabel() != GetReadLabel()) {
    getIP().Advance();
    if (m_inst_set->IsNop( getIP().GetNextInst() ))  getIP().Advance();
  }
  return true;
}

bool cHardwareCPU::Inst_HeadDivideMut(cAvidaContext& ctx, double mut_multiplier)
{
  AdjustHeads();
  const int divide_pos = getHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  getHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_Main(ctx, divide_pos, extra_lines, mut_multiplier);
  // Re-adjust heads.
  AdjustHeads();

  // If using tolerance and a successful divide, place in BX register if the offspring was born into parent's group. 
  if (m_world->GetConfig().TOLERANCE_WINDOW.Get() && ret_val) {
	  GetRegister(REG_BX) = (int) m_organism->GetPhenotype().BornParentGroup();
  }
  
  return ret_val; 
}

bool cHardwareCPU::Inst_HeadDivide(cAvidaContext& ctx)
{
  return Inst_HeadDivideMut(ctx, 1);
  
}

/*
 Resample Divide -- AWC 06/29/06
 */

bool cHardwareCPU::Inst_HeadDivideRS(cAvidaContext& ctx)
{
  AdjustHeads();
  const int divide_pos = getHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  getHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_MainRS(ctx, divide_pos, extra_lines, 1);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}

/*
 Resample Divide -- single mut on divide-- AWC 07/28/06
 */

bool cHardwareCPU::Inst_HeadDivide1RS(cAvidaContext& ctx)
{
  AdjustHeads();
  const int divide_pos = getHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  getHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_Main1RS(ctx, divide_pos, extra_lines, 1);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}

/*
 Resample Divide -- double mut on divide-- AWC 08/29/06
 */

bool cHardwareCPU::Inst_HeadDivide2RS(cAvidaContext& ctx)
{
  AdjustHeads();
  const int divide_pos = getHead(nHardware::HEAD_READ).GetPosition();
  int child_end =  getHead(nHardware::HEAD_WRITE).GetPosition();
  if (child_end == 0) child_end = m_memory.GetSize();
  const int extra_lines = m_memory.GetSize() - child_end;
  bool ret_val = Divide_Main2RS(ctx, divide_pos, extra_lines, 1);
  // Re-adjust heads.
  AdjustHeads();
  return ret_val; 
}


bool cHardwareCPU::Inst_HeadDivideSex(cAvidaContext& ctx)  
{ 
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(1);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivideAsex(cAvidaContext& ctx)  
{ 
  m_organism->GetPhenotype().SetDivideSex(false);
  m_organism->GetPhenotype().SetCrossNum(0);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivideAsexWait(cAvidaContext& ctx)  
{ 
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(0);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivideMateSelect(cAvidaContext& ctx)  
{ 
  // Take the label that follows this divide and use it as the ID for which
  // other organisms this one is willing to mate with.
  ReadLabel();
  m_organism->GetPhenotype().SetMateSelectID( GetLabel().AsInt(NUM_NOPS) );
  
  // Proceed as normal with the rest of mate selection.
  m_organism->GetPhenotype().SetDivideSex(true);
  m_organism->GetPhenotype().SetCrossNum(1);
  return Inst_HeadDivide(ctx); 
}

bool cHardwareCPU::Inst_HeadDivide1(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 1); }
bool cHardwareCPU::Inst_HeadDivide2(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 2); }
bool cHardwareCPU::Inst_HeadDivide3(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 3); }
bool cHardwareCPU::Inst_HeadDivide4(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 4); }
bool cHardwareCPU::Inst_HeadDivide5(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 5); }
bool cHardwareCPU::Inst_HeadDivide6(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 6); }
bool cHardwareCPU::Inst_HeadDivide7(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 7); }
bool cHardwareCPU::Inst_HeadDivide8(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 8); }
bool cHardwareCPU::Inst_HeadDivide9(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 9); }
bool cHardwareCPU::Inst_HeadDivide10(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 10); }
bool cHardwareCPU::Inst_HeadDivide16(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 16); }
bool cHardwareCPU::Inst_HeadDivide32(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 32); }
bool cHardwareCPU::Inst_HeadDivide50(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 50); }
bool cHardwareCPU::Inst_HeadDivide100(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 100); }
bool cHardwareCPU::Inst_HeadDivide500(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 500); }
bool cHardwareCPU::Inst_HeadDivide1000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 1000); }
bool cHardwareCPU::Inst_HeadDivide5000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 5000); }
bool cHardwareCPU::Inst_HeadDivide10000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 10000); }
bool cHardwareCPU::Inst_HeadDivide50000(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 50000); }
bool cHardwareCPU::Inst_HeadDivide0_5(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.5); }
bool cHardwareCPU::Inst_HeadDivide0_1(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.1); }
bool cHardwareCPU::Inst_HeadDivide0_05(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.05); }
bool cHardwareCPU::Inst_HeadDivide0_01(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.01); }
bool cHardwareCPU::Inst_HeadDivide0_001(cAvidaContext& ctx)  { return Inst_HeadDivideMut(ctx, 0.001); }

bool cHardwareCPU::Inst_HeadRead(cAvidaContext& ctx)
{
  const int dst = REG_BX;
  
  const int head_id = FindModifiedHead(nHardware::HEAD_READ);
  getHead(head_id).Adjust();
  
  // Mutations only occur on the read, for the moment.
  int read_inst = 0;
  if (m_organism->TestCopyMut(ctx)) {
    read_inst = m_inst_set->GetRandomInst(ctx).GetOp();
  } else {
    read_inst = getHead(head_id).GetInst().GetOp();
  }
  GetRegister(dst) = read_inst;
  ReadInst(read_inst);
  
  if (m_slip_read_head && m_organism->TestCopySlip(ctx)) {
    getHead(head_id).Set(ctx.GetRandom().GetInt(m_memory.GetSize()));
  }
  
  getHead(head_id).Advance();
  return true;
}

bool cHardwareCPU::Inst_HeadWrite(cAvidaContext& ctx)
{
  const int src = REG_BX;
  const int head_id = FindModifiedHead(nHardware::HEAD_WRITE);
  cHeadCPU& active_head = getHead(head_id);
  
  active_head.Adjust();
  
  int value = GetRegister(src);
  if (value < 0 || value >= m_inst_set->GetSize()) value = 0;
  
  active_head.SetInst(Instruction(value));
  active_head.SetFlagCopied();
  
  if (m_organism->TestCopyIns(ctx)) active_head.InsertInst(m_inst_set->GetRandomInst(ctx));
  if (m_organism->TestCopyDel(ctx)) active_head.RemoveInst();
  if (m_organism->TestCopyUniform(ctx)) doUniformCopyMutation(ctx, active_head);
  if (!m_slip_read_head && m_organism->TestCopySlip(ctx)) {
    doSlipMutation(ctx, m_memory, active_head.GetPosition());
  }
  
  // Advance the head after write...
  active_head.Advance();
  
  return true;
}

bool cHardwareCPU::Inst_HeadCopy(cAvidaContext& ctx)
{
  // For the moment, this cannot be nop-modified.
  cHeadCPU& read_head = getHead(nHardware::HEAD_READ);
  cHeadCPU& write_head = getHead(nHardware::HEAD_WRITE);
  
  read_head.Adjust();
  write_head.Adjust();
  
  // Do mutations.
  Instruction read_inst = read_head.GetInst();
  ReadInst(read_inst.GetOp());
  
  //checkNoMutList is for head to head kaboom experiments
  if (m_organism->TestCopyMut(ctx) && !(checkNoMutList(read_head))) {
    read_inst = m_inst_set->GetRandomInst(ctx);
    write_head.SetFlagMutated();
    write_head.SetFlagCopyMut();
  }
  
  write_head.SetInst(read_inst);
  write_head.SetFlagCopied();  // Set the copied flag...
  
  if (m_organism->TestCopyIns(ctx)) write_head.InsertInst(m_inst_set->GetRandomInst(ctx));
  if (m_organism->TestCopyDel(ctx)) write_head.RemoveInst();
  if (m_organism->TestCopyUniform(ctx)) doUniformCopyMutation(ctx, write_head);
  if (m_organism->TestCopySlip(ctx)) {
    if (m_slip_read_head) {
      read_head.Set(ctx.GetRandom().GetInt(m_memory.GetSize()));
    } else {
      doSlipMutation(ctx, m_memory, write_head.GetPosition());
    }
  }
  
  read_head.Advance();
  write_head.Advance();
  return true;
}

bool cHardwareCPU::HeadCopy_ErrorCorrect(cAvidaContext& ctx, double reduction)
{
  // For the moment, this cannot be nop-modified.
  cHeadCPU & read_head = getHead(nHardware::HEAD_READ);
  cHeadCPU & write_head = getHead(nHardware::HEAD_WRITE);
  
  read_head.Adjust();
  write_head.Adjust();
  
  // Do mutations.
  Instruction read_inst = read_head.GetInst();
  ReadInst(read_inst.GetOp());
  //checkNoMutList for head to head kaboom experiments
  if ( ctx.GetRandom().P(m_organism->GetCopyMutProb() / reduction) && !(checkNoMutList(read_head))) {
    read_inst = m_inst_set->GetRandomInst(ctx);
    write_head.SetFlagMutated();
    write_head.SetFlagCopyMut();
  }
  
  write_head.SetInst(read_inst);
  write_head.SetFlagCopied();  // Set the copied flag...
  
  if (ctx.GetRandom().P(m_organism->GetCopyInsProb() / reduction)) write_head.InsertInst(m_inst_set->GetRandomInst(ctx));
  if (ctx.GetRandom().P(m_organism->GetCopyDelProb() / reduction)) write_head.RemoveInst();
  if (ctx.GetRandom().P(m_organism->GetCopyUniformProb() / reduction)) doUniformCopyMutation(ctx, write_head);
  if (ctx.GetRandom().P(m_organism->GetCopySlipProb() / reduction)) {
    if (m_slip_read_head) {
      read_head.Set(ctx.GetRandom().GetInt(m_memory.GetSize()));
    } else {
      doSlipMutation(ctx, m_memory, write_head.GetPosition());
    }
  }
  
  read_head.Advance();
  write_head.Advance();
  return true;
}

bool cHardwareCPU::Inst_HeadCopy2(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 2); }
bool cHardwareCPU::Inst_HeadCopy3(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 3); }
bool cHardwareCPU::Inst_HeadCopy4(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 4); }
bool cHardwareCPU::Inst_HeadCopy5(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 5); }
bool cHardwareCPU::Inst_HeadCopy6(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 6); }
bool cHardwareCPU::Inst_HeadCopy7(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 7); }
bool cHardwareCPU::Inst_HeadCopy8(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 8); }
bool cHardwareCPU::Inst_HeadCopy9(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 9); }
bool cHardwareCPU::Inst_HeadCopy10(cAvidaContext& ctx) { return HeadCopy_ErrorCorrect(ctx, 10); }

bool cHardwareCPU::Inst_HeadSearch(cAvidaContext&)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  cHeadCPU found_pos = FindLabel(0);
  const int search_size = found_pos.GetPosition() - getIP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  getHead(nHardware::HEAD_FLOW).Set(found_pos);
  getHead(nHardware::HEAD_FLOW).Advance();
  return true; 
}

bool cHardwareCPU::Inst_HeadSearchDirect(cAvidaContext&)
{
  ReadLabel();
  cHeadCPU found_pos = FindLabel(1);
  const int search_size = found_pos.GetPosition() - getIP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  getHead(nHardware::HEAD_FLOW).Set(found_pos);
  getHead(nHardware::HEAD_FLOW).Advance();
  return true; 
}

bool cHardwareCPU::Inst_SetFlow(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  getHead(nHardware::HEAD_FLOW).Set(GetRegister(reg_used));
  return true; 
}



bool cHardwareCPU::Inst_GetUpdate(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = m_world->GetStats().GetUpdate();
  
  return true;
}


/*! This method places the calling organism's x-y coordinates in ?BX? and ?++BX?.
 
 Note that this method *will not work* from within the test CPU, so we have to guard
 against that.
 */
bool cHardwareCPU::Inst_GetCellPosition(cAvidaContext& ctx)
{
  int x = m_organism->GetOrgInterface().GetCellXPosition();
  int y = m_organism->GetOrgInterface().GetCellYPosition();
  // Fail if we're running in the test CPU.
  if (x == -1 || y == -1) return false;
  
  const int xreg = FindModifiedRegister(REG_BX);
  const int yreg = FindNextRegister(xreg);
  GetRegister(xreg) = x;
  GetRegister(yreg) = y;
  return true;
}

/*! This method places the calling organism's x coordinate in ?BX?.
 
 Note that this method *will not work* from within the test CPU, so we have to guard
 against that.
 */
bool cHardwareCPU::Inst_GetCellPositionX(cAvidaContext&)
{
  int x = m_organism->GetOrgInterface().GetCellXPosition();
  // Fail if we're running in the test CPU.
  if (x == -1) return false;
  
  const int xreg = FindModifiedRegister(REG_BX);
  GetRegister(xreg) = x;

  return true;
}

/*! This method places the calling organism's y coordinates in ?BX?.
 
 Note that this method *will not work* from within the test CPU, so we have to guard
 against that.
 */
bool cHardwareCPU::Inst_GetCellPositionY(cAvidaContext&)
{
  int y = m_organism->GetOrgInterface().GetCellYPosition();
  // Fail if we're running in the test CPU.
  if (y == -1) return false;
  
  const int yreg = FindModifiedRegister(REG_BX);
  GetRegister(yreg) = y;

  return true;
}

bool cHardwareCPU::Inst_GetDistanceFromDiagonal(cAvidaContext&)
{
//  int absolute_cell_ID = m_organism->GetOrgInterface().GetCellID();
  
  assert(false);
  // @TODO - distance from diagonal
  
//  std::pair<int, int> pos = m_world->GetPopulation().GetCellPosition(absolute_cell_ID);
//  const int reg = FindModifiedRegister(REG_BX);
//  
//  if (pos.first > pos.second) {
//    GetRegister(reg) = (int)ceil((pos.first - pos.second)/2.0);
//  } else {
//    GetRegister(reg) = (int)floor((pos.first - pos.second)/2.0);
//  }
  //  std::cerr<<"x = "<<pos.first<<"  y = "<<pos.second<<"  ans = "<<GetRegister(reg)<<std::endl;
  
  return true;
}

bool cHardwareCPU::Inst_GetDirectionOffNorth(cAvidaContext&) {
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetFacedDir();
  return true;
}

bool cHardwareCPU::Inst_GetNortherly(cAvidaContext&) {
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetNortherly();
  return true;  
}

bool cHardwareCPU::Inst_GetEasterly(cAvidaContext&) {
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetEasterly();
  return true;  
}

bool cHardwareCPU::Inst_ZeroEasterly(cAvidaContext&) {
  m_organism->ClearEasterly();
  return true;
}

bool cHardwareCPU::Inst_ZeroNortherly(cAvidaContext&) {
  m_organism->ClearNortherly();
  return true;
}

//// Promoter Model ////

bool cHardwareCPU::Inst_Promoter(cAvidaContext&)
{
  // Promoters don't do anything themselves
  return true;
}

// Move the instruction ptr to the next active promoter
bool cHardwareCPU::Inst_Terminate(cAvidaContext& ctx)
{
  // Optionally,
  // Reset the thread.
  if (m_world->GetConfig().TERMINATION_RESETS.Get())
  {
    //const int write_head_pos = getHead(nHardware::HEAD_WRITE).GetPosition();
    //const int read_head_pos = getHead(nHardware::HEAD_READ).GetPosition();
    m_threads[m_cur_thread].Reset(this, m_threads[m_cur_thread].GetID());
    //getHead(nHardware::HEAD_WRITE).Set(write_head_pos);
    //getHead(nHardware::HEAD_READ).Set(read_head_pos);
    
    //Setting this makes it harder to do things. You have to be modular.
    m_organism->GetOrgInterface().ResetInputs(ctx);   // Re-randomize the inputs this organism sees
    m_organism->ClearInput();                         // Also clear their input buffers, or they can still claim
    // rewards for numbers no longer in their environment!
  }
  
  // Reset our count
  m_threads[m_cur_thread].ResetPromoterInstExecuted();
  m_advance_ip = false;
  const int reg_used = REG_BX; // register to put chosen promoter code in, for now always BX
  
  // Search for an active promoter  
  int start_offset = m_promoter_offset;
  int start_index  = m_promoter_index;
  
  bool no_promoter_found = true;
  if ( m_promoters.GetSize() > 0 ) {
    while (true) {
      // If the next promoter is active, then break out
      NextPromoter();
      if (IsActivePromoter()) {
        no_promoter_found = false;
        break;
      }
      
      // If we just checked the promoter that we were originally on, then there
      // are no active promoters.
      if ( (start_offset == m_promoter_offset) && (start_index == m_promoter_index) ) break;
      
      // If we originally were not on a promoter, then stop once we check the
      // first promoter and an offset of zero
      if (start_index == -1) {
        start_index = 0;
      }
    } 
  }
  
  if (no_promoter_found) {
    if ((m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 0) || (m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 2)) {
      // Set defaults for when no active promoter is found
      m_promoter_index = -1;
      getIP().Set(0);
      GetRegister(reg_used) = 0;
    }
    // Death to organisms that refuse to use promoters!
    else if (m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() == 1) {
      m_organism->Die(ctx); 
    } else {
      cout << "Unrecognized NO_ACTIVE_PROMOTER_EFFECT setting: " << m_world->GetConfig().NO_ACTIVE_PROMOTER_EFFECT.Get() << endl;
    }
  }
  else
  {
    // We found an active match, offset to just after it.
    // cHeadCPU will do the mod genome size for us
    getIP().Set(m_promoters[m_promoter_index].m_pos + 1);
    
    // Put its bit code in BX for the organism to have if option is set
    if ( m_world->GetConfig().PROMOTER_TO_REGISTER.Get() ) {
      GetRegister(reg_used) = m_promoters[m_promoter_index].m_bit_code;
    }
  }
  
  return true;
}

// Set a new regulation code (which is XOR'ed with ALL promoter codes).
bool cHardwareCPU::Inst_Regulate(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  int regulation_code = GetRegister(reg_used);
  
  for (int i=0; i< m_promoters.GetSize();i++) {
    m_promoters[i].m_regulation = regulation_code;
  }
  
  return true;
}

// Set a new regulation code, but only on a subset of promoters.
bool cHardwareCPU::Inst_RegulateSpecificPromoters(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  int regulation_code = GetRegister(reg_used);
  
  const int reg_promoter = FindModifiedRegister((reg_used+1) % NUM_REGISTERS);
  int regulation_promoter = GetRegister(reg_promoter);
  
  for (int i=0; i< m_promoters.GetSize();i++) {
    //Look for consensus bit matches over the length of the promoter code
    int test_p_code = m_promoters[i].m_bit_code;    
    int test_r_code = regulation_promoter;
    int bit_count = 0;
    for (int j=0; j<m_world->GetConfig().PROMOTER_EXE_LENGTH.Get();j++) {      
      if ((test_p_code & 1) == (test_r_code & 1)) bit_count++;
      test_p_code >>= 1;
      test_r_code >>= 1;
    }
    if (bit_count >= m_world->GetConfig().PROMOTER_EXE_LENGTH.Get() / 2) {
      m_promoters[i].m_regulation = regulation_code;
    }
  }
  
  return true;
}


bool cHardwareCPU::Inst_SenseRegulate(cAvidaContext& ctx)
{
  unsigned int bits = 0;
  const Apto::Array<double> & res_count = m_organism->GetOrgInterface().GetResources(ctx);
  assert (res_count.GetSize() != 0);
  for (int i=0; i<m_world->GetConfig().PROMOTER_CODE_SIZE.Get(); i++) {
    int b = i % res_count.GetSize();
    bits <<= 1;
    bits += (res_count[b] != 0);
  }  
  
  for (int i=0; i< m_promoters.GetSize();i++) {
    m_promoters[i].m_regulation = bits;
  }
  
  return true;
}

// Create a number from inst bit codes
bool cHardwareCPU::Do_Numberate(cAvidaContext&, int num_bits)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  // advance the IP now, so that it rests on the beginning of our number
  getIP().Advance();
  m_advance_ip = false;
  
  int num = Numberate(getIP().GetPosition(), +1, num_bits);
  GetRegister(reg_used) = num;
  
  return true;
}

// Move to the next promoter.
void cHardwareCPU::NextPromoter()
{
  // Move promoter index, rolling over if necessary
  m_promoter_index++;
  if (m_promoter_index == m_promoters.GetSize()) {
    m_promoter_index = 0;
    
    // Move offset, rolling over when there are not enough bits before we would have to wrap around left
    m_promoter_offset+=m_world->GetConfig().PROMOTER_EXE_LENGTH.Get();
    if (m_promoter_offset + m_world->GetConfig().PROMOTER_EXE_LENGTH.Get() > m_world->GetConfig().PROMOTER_CODE_SIZE.Get()) {
      m_promoter_offset = 0;
    }
  }
}


// Check whether the current promoter is active.
bool cHardwareCPU::IsActivePromoter()
{
  assert( m_promoters.GetSize() != 0 );
  int count = 0;
  unsigned int code = m_promoters[m_promoter_index].GetRegulatedBitCode();
  for (int i=0; i<m_world->GetConfig().PROMOTER_EXE_LENGTH.Get(); i++) {
    int offset = m_promoter_offset + i;
    offset %= m_world->GetConfig().PROMOTER_CODE_SIZE.Get();
    int state = code >> offset;
    count += (state & 1);
  }
  
  return (count >= m_world->GetConfig().PROMOTER_EXE_THRESHOLD.Get());
}

// Construct a promoter bit code from instruction bit codes
int cHardwareCPU::Numberate(int _pos, int _dir, int _num_bits)
{  
  int code_size = 0;
  unsigned int code = 0;
  unsigned int max_bits = sizeof(code) * 8;
  assert(_num_bits <= (int)max_bits);
  if (_num_bits == 0) _num_bits = max_bits;
  
  // Enforce a boundary, sometimes -1 can be passed for _pos
  int j = _pos + m_memory.GetSize();
  j %= m_memory.GetSize();
  assert(j >=0);
  assert(j < m_memory.GetSize());
  while (code_size < _num_bits) {
    unsigned int inst_code = (unsigned int) GetInstSet().GetInstructionCode(m_memory[j]);
    // shift bits in, one by one ... excuse the counter variable pun
    for (int code_on = 0; (code_size < _num_bits) && (code_on < m_world->GetConfig().INST_CODE_LENGTH.Get()); code_on++) {
      if (_dir < 0) {
        code >>= 1; // shift first so we don't go one too far at the end
        code += (1 << (_num_bits - 1)) * (inst_code & 1);
        inst_code >>= 1; 
      } else {
        code <<= 1; // shift first so we don't go one too far at the end;        
        code += (inst_code >> (m_world->GetConfig().INST_CODE_LENGTH.Get() - 1)) & 1;
        inst_code <<= 1; 
      }
      code_size++;
    }
    
    // move back one inst
    j += m_memory.GetSize() + _dir;
    j %= m_memory.GetSize();    
  }
  
  return code;
}


//// Copied from cHardwareExperimental -- @JEB
static const unsigned int CONSENSUS = (sizeof(int) * 8) / 2;
static const unsigned int CONSENSUS24 = 12;

inline unsigned int cHardwareCPU::BitCount(unsigned int value) const
{
  const unsigned int w = value - ((value >> 1) & 0x55555555);
  const unsigned int x = (w & 0x33333333) + ((w >> 2) & 0x33333333);
  const unsigned int bit_count = ((x + (x >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
  return bit_count;
}

bool cHardwareCPU::Inst_BitConsensus(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedNextRegister(reg_used);
  GetRegister(reg_used) = (BitCount(GetRegister(op1)) >= CONSENSUS) ? 1 : 0;
  return true; 
}

bool cHardwareCPU::Inst_BitConsensus24(cAvidaContext&)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int op1 = FindModifiedNextRegister(reg_used);
  GetRegister(reg_used) = (BitCount(GetRegister(op1) & MASK24) >= CONSENSUS24) ? 1 : 0;
  return true; 
}

bool cHardwareCPU::Inst_IfConsensus(cAvidaContext&)
{
  const int op1 = FindModifiedRegister(REG_BX);
  if (BitCount(GetRegister(op1)) <  CONSENSUS)  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfConsensus24(cAvidaContext&)
{
  const int op1 = FindModifiedRegister(REG_BX);
  if (BitCount(GetRegister(op1) & MASK24) <  CONSENSUS24)  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLessConsensus(cAvidaContext&)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindModifiedNextRegister(op1);
  if (BitCount(GetRegister(op1)) >=  BitCount(GetRegister(op2)))  getIP().Advance();
  return true;
}

bool cHardwareCPU::Inst_IfLessConsensus24(cAvidaContext&)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindModifiedNextRegister(op1);
  if (BitCount(GetRegister(op1) & MASK24) >=  BitCount(GetRegister(op2) & MASK24))  getIP().Advance();
  return true;
}

//// End copied from cHardwareExperimental


/* Bit masking instructions */

// masks sign bit in a register
bool cHardwareCPU::Inst_MaskSignBit(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASK_SIGNBIT;
  return true;
}

// masks lower 16 bits in ?BX? register
bool cHardwareCPU::Inst_MaskOffLower16Bits(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST16;
  return true;
}

// masks lower 16 bits in ?AX? register
bool cHardwareCPU::Inst_MaskOffLower16Bits_defaultAX(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_AX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST16;
  return true;
}

// masks lower 15 bits in ?BX? register
bool cHardwareCPU::Inst_MaskOffLower15Bits(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST15;
  return true;
}

// masks lower 15 bits in ?AX? register
bool cHardwareCPU::Inst_MaskOffLower15Bits_defaultAX(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_AX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST15;
  return true;
}

// masks lower 14 bits in ?BX? register
bool cHardwareCPU::Inst_MaskOffLower14Bits(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST14;
  return true;
}

// masks lower 14 bits in ?AX? register
bool cHardwareCPU::Inst_MaskOffLower14Bits_defaultAX(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_AX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST14;
  return true;
}

// masks lower 13 bits in ?BX? register
bool cHardwareCPU::Inst_MaskOffLower13Bits(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST13;
  return true;
}

// masks lower 13 bits in ?AX? register
bool cHardwareCPU::Inst_MaskOffLower13Bits_defaultAX(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_AX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST13;
  return true;
}

// masks lower 12 bits in ?BX? register
bool cHardwareCPU::Inst_MaskOffLower12Bits(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST12;
  return true;
}

// masks lower 12 bits in ?AX? register
bool cHardwareCPU::Inst_MaskOffLower12Bits_defaultAX(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_AX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST12;
  return true;
}

// masks lower 8 bits in ?BX? register
bool cHardwareCPU::Inst_MaskOffLower8Bits(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST8;
  return true;
}

// masks lower 8 bits in ?AX? register
bool cHardwareCPU::Inst_MaskOffLower8Bits_defaultAX(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_AX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST8;
  return true;
}

// masks lower 4 bits in ?BX? register
bool cHardwareCPU::Inst_MaskOffLower4Bits(cAvidaContext&)
{
  const int reg = FindModifiedRegister(REG_BX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST4;
  return true;
}

// masks lower 4 bits in ?AX? register
bool cHardwareCPU::Inst_MaskOffLower4Bits_defaultAX(cAvidaContext&) 
{
  const int reg = FindModifiedRegister(REG_AX);
  GetRegister(reg) = GetRegister(reg) & MASKOFF_LOWEST4;
  return true;
}




//// Placebo insts ////
bool cHardwareCPU::Inst_Skip(cAvidaContext&)
{
  getIP().Advance();
  return true;
}




/*! Set this organism's current opinion to the value in ?BX?.
 */
bool cHardwareCPU::Inst_SetOpinion(cAvidaContext&)
{
  assert(m_organism != 0);
  m_organism->GetOrgInterface().SetOpinion(GetRegister(FindModifiedRegister(REG_BX)), m_organism);
  return true;
}


/*! Sense this organism's current opinion, placing the opinion in register ?BX?
 and the age of that opinion (in updates) in register !?BX?.  If the organism has no
 opinion, do nothing.
 */
bool cHardwareCPU::Inst_GetOpinion(cAvidaContext&)
{
  assert(m_organism != 0);
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    const int opinion_reg = FindModifiedRegister(REG_BX);
    const int age_reg = FindNextRegister(opinion_reg);
    
    GetRegister(opinion_reg) = m_organism->GetOpinion().first;
    GetRegister(age_reg) = m_world->GetStats().GetUpdate() - m_organism->GetOpinion().second;
  }
  return true;
}

bool cHardwareCPU::Inst_GetOpinionOnly_ZeroIfNone(cAvidaContext&)
{
  assert(m_organism != 0);
  const int opinion_reg = FindModifiedRegister(REG_BX);
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    GetRegister(opinion_reg) = m_organism->GetOpinion().first;
  } else {
    GetRegister(opinion_reg) = 0;
  }
  
  return true;
}


bool cHardwareCPU::Inst_ClearOpinion(cAvidaContext&)
{
  assert(m_organism != 0);
  m_organism->GetOrgInterface().ClearOpinion(m_organism);
  
  return true;
}

/*! If the organism has an opinion then execute the next instruction, else skip.
 */
bool cHardwareCPU::Inst_IfOpinionSet(cAvidaContext&)
{
  assert(m_organism != 0);
  if (!m_organism->GetOrgInterface().HasOpinion(m_organism)) getIP().Advance();
  
  return true;
}

bool cHardwareCPU::Inst_IfOpinionNotSet(cAvidaContext&)
{
  assert(m_organism != 0);
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) getIP().Advance();
  
  return true;
}

/* Sets the organism's opinion to the number 0 and checks for completed tasks. */
bool cHardwareCPU::Inst_SetOpinionToZero(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = 0;
  m_organism->GetOrgInterface().SetOpinion(GetRegister(FindModifiedRegister(REG_BX)), m_organism);
  m_organism->DoOutput(ctx, 0);
  return true;
}

/* Sets the organism's opinion to the number 1 and checks for completed tasks. */
bool cHardwareCPU::Inst_SetOpinionToOne(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = 1;
  m_organism->GetOrgInterface().SetOpinion(GetRegister(FindModifiedRegister(REG_BX)), m_organism);
  m_organism->DoOutput(ctx, 1);
  return true;
}

/* Sets the organism's opinion to the number 2 and checks for completed tasks. */
bool cHardwareCPU::Inst_SetOpinionToTwo(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = 2;
  m_organism->GetOrgInterface().SetOpinion(GetRegister(FindModifiedRegister(REG_BX)), m_organism);
  m_organism->DoOutput(ctx, 2);
  return true;
}




/*! Collect this cell's data, and place it in ?BX?.  Set the flag indicating that
 this organism has collected cell data to true, and set the last collected cell data
 as well.
 */
bool cHardwareCPU::Inst_CollectCellData(cAvidaContext&)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetCellData();
  // Update last collected cell data:
  m_last_cell_data = std::make_pair(true, GetRegister(out_reg));
  
  return true;
}


/*! Detect if the cell data in which this organism lives has changed since the
 last time that this organism has collected cell data.  Note that this process
 DOES NOT take into account organism movement, and it only works with explicit
 collection of cell data.
 */
bool cHardwareCPU::Inst_IfCellDataChanged(cAvidaContext&)
{
  assert(m_organism != 0);
  // If we haven't collected cell data yet, or it's the same as the current cell data, advance
  // the IP:
  if (!m_last_cell_data.first || (m_last_cell_data.second == m_organism->GetCellData())) {
    getIP().Advance();
  }
  
  return true;
}




bool cHardwareCPU::Inst_ReadCellData(cAvidaContext&)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetCellData();
  
  return true;
}

bool cHardwareCPU::Inst_ReadFacedCellData(cAvidaContext&)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  // return % diff (FacedCellData is already int)
  int my_vit = (int) (m_organism->GetVitality() + 0.5);
  int vit_diff = (m_organism->GetFacedCellData() - my_vit)/my_vit * 100;
  GetRegister(out_reg) = vit_diff;
  
  return true;
}

bool cHardwareCPU::Inst_ReadFacedCellDataOrgID(cAvidaContext&)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetFacedCellDataOrgID();
  
  return true;
}

bool cHardwareCPU::Inst_ReadFacedCellDataFreshness(cAvidaContext&)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_world->GetStats().GetUpdate() - m_organism->GetFacedCellDataUpdate();
  
  return true;
}

bool cHardwareCPU::Inst_MarkCellWithID(cAvidaContext&)
{
  assert(m_organism != 0);
  m_organism->SetCellData(m_organism->GetID());
  
  return true;
}

bool cHardwareCPU::Inst_GetResStored(cAvidaContext&)
//Get amount of stored collect specific resource (how much do I have available for res_cost instructions).
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  const int resource = m_world->GetConfig().COLLECT_SPECIFIC_RESOURCE.Get();
  // needs to return int...we round down so that they don't think there is more available than they need
  int res_stored = (int) (m_organism->GetRBin(resource) * 100 - 0.5);
  GetRegister(out_reg) = res_stored;
  return true;
}

bool cHardwareCPU::Inst_MarkCellWithVitality(cAvidaContext&)
{
  assert(m_organism != 0);
  // SetCellData() needs to be int
  int my_vit = (int) (m_organism->GetVitality() + 0.5);
  m_organism->SetCellData(my_vit);
  
  return true;
}

bool cHardwareCPU::Inst_GetID(cAvidaContext&)
{
  assert(m_organism != 0);
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = m_organism->GetID();
  
  return true;
}

bool cHardwareCPU::Inst_GetFacedVitalityDiff(cAvidaContext&)
//Get difference in vitality of this organism and faced neighbor.
{
  assert(m_organism != 0);
  
  if (!m_organism->IsNeighborCellOccupied()) return false;
  
  cOrganism * neighbor = m_organism->GetNeighbor();
  if (neighbor->IsDead())  return false; 
  
  const int out_reg = FindModifiedRegister(REG_BX);
  // return % diff
  int vit_diff = (int) ((neighbor->GetVitality() -  m_organism->GetVitality())/m_organism->GetVitality() * 100 + 0.5);
  GetRegister(out_reg) = vit_diff;
  return true;
}


bool cHardwareCPU::Inst_GetFacedOrgID(cAvidaContext&)
//Get ID of organism faced by this one, if there is an organism in front.
{
  if (!m_organism->IsNeighborCellOccupied()) return false;
  
  cOrganism * neighbor = m_organism->GetNeighbor();
  if (neighbor->IsDead())  return false;  
  
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = neighbor->GetID();
  return true;
}


//Attack organism faced by this one, if there is an organism in front. This will use vitality bins if those are set.
bool cHardwareCPU::Inst_AttackFacedOrg(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  if (!m_organism->IsNeighborCellOccupied()) return false;
  
  cOrganism* target = m_organism->GetNeighbor();
  if (target->IsDead()) return false;  
  
  const int target_cell = target->GetCellID();
  
  //Use vitality settings to decide who wins this battle.
  bool kill_attacker = true;
  if (m_world->GetConfig().MOVEMENT_COLLISIONS_SELECTION_TYPE.Get() == 0) 
    // 50% chance, no modifiers
    kill_attacker = ctx.GetRandom().P(0.5);
  else if (m_world->GetConfig().MOVEMENT_COLLISIONS_SELECTION_TYPE.Get() == 1) {
    //vitality based
    const double attacker_vitality = m_organism->GetVitality();
    const double target_vitality = target->GetVitality();
    const double attacker_win_odds = ((attacker_vitality) / (attacker_vitality + target_vitality));
    const double target_win_odds = ((target_vitality) / (attacker_vitality + target_vitality)); 
    
    const double odds_someone_dies = max(attacker_win_odds, target_win_odds);
    const double odds_target_dies = (1 - target_win_odds) * odds_someone_dies;
    const double decider = ctx.GetRandom().GetDouble(1);
    
    if (decider < (1 - odds_someone_dies)) return true;
    else if (decider < ((1 - odds_someone_dies) + odds_target_dies)) kill_attacker = false;    
  }
  if (kill_attacker) {
    m_organism->Die(ctx);
    return true;
  }
  
  m_world->GetPopulation().AttackFacedOrg(ctx, target_cell); 
  return true;
} 		

//Get odds of winning or tieing in a fight. This will use vitality bins if those are set.
bool cHardwareCPU::Inst_GetAttackOdds(cAvidaContext&)
{
  assert(m_organism != 0);
  if (!m_organism->IsNeighborCellOccupied()) return false;
  
  cOrganism* target = m_organism->GetNeighbor();
  if (target->IsDead()) return false;  
  
  const double attacker_vitality = m_organism->GetVitality();
  const double target_vitality = target->GetVitality();
  
  const double attacker_win_odds = ((attacker_vitality) / (attacker_vitality + target_vitality));
  const double target_win_odds = ((target_vitality) / (attacker_vitality + target_vitality)); 
  
  const double odds_someone_dies = max(attacker_win_odds, target_win_odds);
  // my win odds are odds nobody dies or someone dies and it's the target
  const double odds_I_dont_die = (1 - odds_someone_dies) + ((1 - target_win_odds) * odds_someone_dies);
  
  // return odds as %
  const int out_reg = FindModifiedRegister(REG_BX);
  GetRegister(out_reg) = (int) (odds_I_dont_die * 100 + 0.5);
  return true;
} 	




//! Loads the current neighborhood into the organism's memory.
bool cHardwareCPU::Inst_GetNeighborhood(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  m_organism->LoadNeighborhood(ctx);
  
  return true;
}


//! Test if the current neighborhood has changed from that in the organism's memory.
bool cHardwareCPU::Inst_IfNeighborhoodChanged(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  if (!m_organism->HasNeighborhoodChanged(ctx)) {
    getIP().Advance();
  }
	
  return true;
}


/*! Find the first occurence of the passed-in instruction from the getIP() forward,
 wrapping around the genome as required.  If the given instruction is not in the
 genome, return the starting position.
 */
cHeadCPU cHardwareCPU::Find(const char* instr)
{
  cHeadCPU ptr(getIP());
  const int current = ptr.GetPosition();
  ptr.Advance();
  while (ptr.GetPosition() != current) {
    ptr.Advance();
    if (m_inst_set->GetName(ptr.GetInst())==instr) {
      break;
    }
  }
  
  return ptr;
}


bool cHardwareCPU::Inst_IfLessEnd(cAvidaContext&)
{
  const int x = FindModifiedRegister(REG_BX);
  const int y = FindNextRegister(x);
	
  if (GetRegister(x) >= GetRegister(y)) { Else_TopHalf(); }
  return true;
}


bool cHardwareCPU::Inst_IfNotEqualEnd(cAvidaContext&)
{
  const int x = FindModifiedRegister(REG_BX);
  const int y = FindNextRegister(x);
  
  if (GetRegister(x) == GetRegister(y)) { Else_TopHalf(); }
  return true;  
}


bool cHardwareCPU::Inst_IfGrtEquEnd(cAvidaContext&)
{
  const int x = FindModifiedRegister(REG_BX);
  const int y = FindNextRegister(x);
  
  if (GetRegister(x) < GetRegister(y)) { Else_TopHalf(); }
  return true;
}


/*! This is the top-half of the else instruction, meant to be executed if the if
 condition evaluates to false.
 */
void cHardwareCPU::Else_TopHalf()
{
  cHeadCPU else_head = Find("else");
  cHeadCPU endif_head = Find("endif");  
	
  // Condition failed.  If there's an else-clause, jump to it.
  // If there isn't an else-clause, try to jump to the endif.
  // Note that the IP is unconditionally advanced *after* this instruction
  // has executed.  If there is no else or endif, advance one instruction.
  if (else_head.GetPosition() != getIP().GetPosition()) {
    getIP().Set(else_head);
  } else if (endif_head.GetPosition() != getIP().GetPosition()) {
    getIP().Set(endif_head);
  } else {
    // No else or endif.  Advance past the next instruction (as normal).
    getIP().Advance();
  }
}


/*! The only way that this instruction can be executed is if the if passed, or
 if there was no if.  In both cases, we're going to jump to the first <end-if>, or
 skip one instruction.
 */
bool cHardwareCPU::Inst_Else(cAvidaContext&)
{
  cHeadCPU endif_head = Find("endif");
  if (endif_head.GetPosition() != getIP().GetPosition()) {
    // If the <end-if> is somewhere else, jump to it.
    getIP().Set(endif_head);
  } else {
    // Otherwise, just skip one instruction.
    getIP().Advance();
  }
  
  return true; 
}

/*! This is just a placeholder; it has no functionality of its own.
 */
bool cHardwareCPU::Inst_EndIf(cAvidaContext&) { 
  return true; 
}




/* Donate if the neighbor previously donated to the organism. */
bool cHardwareCPU::Inst_DonateIfDonor(cAvidaContext& ctx)
{
  cOrganism * neighbor = m_organism->GetNeighbor();
  if (neighbor != NULL) {
    // check if the neighbor was a donor
    if (m_organism->IsDonor(neighbor->GetID())) {
      m_world->GetStats().IncDonateToDonor();
      Inst_DonateFacingRawMaterialsOtherSpecies(ctx);	
    }
  }
  
  return true;
}

/* Donate raw materials (of one kind) to a neighbor, but 
 only if the neighbor is of a different species. If the 
 instruction fails, there is no consequence. */
bool cHardwareCPU::Inst_DonateFacingRawMaterialsOtherSpecies(cAvidaContext& ctx) 
{ 
  cOrganism * neighbor = m_organism->GetNeighbor();
  if (neighbor != NULL) {
    
    int spec_self =  m_organism->GetLineageLabel();
    int spec_neighbor = neighbor->GetLineageLabel();
    
    if (spec_self != spec_neighbor) {
      Inst_DonateFacingString(ctx);	
    }
  }
  return true;
}

/* Donate a string that you have produced to the facing organism */
bool cHardwareCPU::Inst_DonateFacingString(cAvidaContext& ctx)
{
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  int cost = m_world->GetConfig().ALT_COST.Get(); 
  int my_string = m_organism->GetLineageLabel();
  
  // Donate only if we have found a neighbor.
  if (neighbor != NULL) {
		
    // Check if the organism has enough of this string on hand.
    if ((m_organism->GetNumberStringsOnHand(my_string) >= cost) && (neighbor->CanReceiveString(my_string, cost))) { 
			
      // sometimes the donation will fail. 
      // get the probability of failure
      unsigned int prob_fail = m_world->GetConfig().DONATION_FAILURE_PERCENT.Get(); 
      unsigned int rand_num = ctx.GetRandom().GetUInt(0, 100); 
      // neighbor donates to organism.
      if (rand_num < prob_fail) { 
        // EXIT
        return true; 
      }
      
      m_organism->DonateString(my_string, cost);
      neighbor->AddOtherRawMaterials(cost, m_organism->GetID()); 
      neighbor->ReceiveString(my_string, cost, m_organism->GetID()); 
      neighbor->AddDonatedLineage(m_organism->GetLineageLabel());
			
      // track stats
      m_organism->Donated();
			
      ComputeReputation();			
    }
  }
  return true;
}

/* Donate raw materials to the facing organism. */
bool cHardwareCPU::Inst_DonateFacingRawMaterials(cAvidaContext& ctx)
{
  
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  int cost = m_world->GetConfig().ALT_COST.Get(); 
  
  // Donate only if we have found a neighbor.
  if (neighbor != NULL) {
		
    // Subtract raw materials from the organism (currently subtracts 1 resource...)
    // fails if the organism does not have any more resources
    if (m_organism->SubtractSelfRawMaterials(cost)) {
      
      // sometimes the donation will fail. 
      // get the probability of failure
      unsigned int prob_fail = m_world->GetConfig().DONATION_FAILURE_PERCENT.Get(); 
      unsigned int rand_num = ctx.GetRandom().GetUInt(0, 100); 
      // neighbor donates to organism.
      if (rand_num < prob_fail) { 
        // EXIT
        return true; 
      }
      
      neighbor->AddOtherRawMaterials(cost, m_organism->GetID()); 
      neighbor->AddDonatedLineage(m_organism->GetLineageLabel());
      
      // rotate recipient to face donor 
      // by rotating until the recipient faces the donor
      // adding a new comment.
      if (m_world->GetConfig().ROTATE_ON_DONATE.Get()) {
        while (neighbor->GetNeighbor() != m_organism) {
          neighbor->Rotate(ctx, 1);
        }
      }
      
      // track stats
      m_organism->Donated();
			
      ComputeReputation();
			
    }
  }
  return true;
}  

/* An organism artificially increases its reputation without donating. */
bool cHardwareCPU::Inst_Pose(cAvidaContext&)
{
  // update reputation to include this phony donation.
  // get the current reputation; increment by 1.	
  m_organism->SetReputation(m_organism->GetReputation() + 1);
  
  return true;
}



/*! An organism's reputation is stored as an opinion. This instruction 
 uses Inst_GetNeighborsOpinion to do the heavy lifting, but includes
 default behavior suitable for reputations. Specifically, if an 
 neighbor has no reputation (i.e., it has not donated) or does not
 exist, then this instruction puts zeros into the registers.
 */
bool cHardwareCPU::Inst_GetNeighborsReputation(cAvidaContext&)
{
  // Get faced neighbor
  cOrganism * neighbor = m_organism->GetNeighbor();
  if (neighbor != NULL) { 
    const int raw_mat_reg = FindModifiedRegister(REG_AX);
    GetRegister(raw_mat_reg) = neighbor->GetReputation();	
  } 
  return true;
}


/*! An organism's reputation is stored as an opinion. This instruction 
 uses Inst_GetOpinion to do the heavy lifting, but includes
 default behavior suitable for reputations. Specifically, if an 
 organism has no reputation (i.e., it has not donated), then this 
 instruction puts zeros into the registers.
 */
bool cHardwareCPU::Inst_GetReputation(cAvidaContext&)
{
  const int opinion_reg = FindModifiedRegister(REG_BX);
  GetRegister(opinion_reg) = m_organism->GetReputation();
  return true;
}

/* Sense the number of raw materials an organism has. Store in
 ?REG_AX? */
bool cHardwareCPU::Inst_GetAmountOfRawMaterials(cAvidaContext&)
{
  const int raw_mat_reg = FindModifiedRegister(REG_AX);
  GetRegister(raw_mat_reg) = m_organism->GetNumberStringsOnHand(0);
  return true;
}

/* Sense the number of raw materials an organism has. Store in
 ?REG_BX? */
bool cHardwareCPU::Inst_GetAmountOfOtherRawMaterials(cAvidaContext&)
{
  const int raw_mat_reg = FindModifiedRegister(REG_BX);
  GetRegister(raw_mat_reg) = m_organism->GetNumberStringsOnHand(1);
  return true;
}



/* Rotate to face the organism with the highest reputation */
bool cHardwareCPU::Inst_RotateToGreatestReputation(cAvidaContext&) 
{
  m_organism->GetOrgInterface().RotateToGreatestReputation();
	
  return true;	
}

/* Rotate to face the organism with the highest reputation that has
 a different tag. */
bool cHardwareCPU::Inst_RotateToGreatestReputationWithDifferentTag(cAvidaContext&)
{
  m_organism->GetOrgInterface().RotateToGreatestReputationWithDifferentTag(m_organism->GetTagLabel());
  return true;	
}

/* Rotate to face the organism with the highest reputation that has
 a different lineage. */
bool cHardwareCPU::Inst_RotateToGreatestReputationWithDifferentLineage(cAvidaContext&)
{
  m_organism->GetOrgInterface().RotateToGreatestReputationWithDifferentLineage(m_organism->GetLineageLabel());
  return true;	
}


/* Rotate to face the organism with the highest reputation and then
 immediately donate */
bool cHardwareCPU::Inst_RotateToGreatestReputationAndDonate(cAvidaContext& ctx) 
{
  Inst_RotateToGreatestReputation(ctx);
  Inst_DonateFacingRawMaterials(ctx);
  
  return true;
}



bool cHardwareCPU::Inst_RotateToDifferentTag(cAvidaContext& ctx)
{
  //get the neighborhood size
  const int num_neighbors = m_organism->GetNeighborhoodSize();
	
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) m_organism->Rotate(ctx, 1);
  cOrganism * neighbor = m_organism->GetNeighbor();
	
  int max_id = neighbor_id + num_neighbors;
	
  //we have not found a match yet
  bool found = false;
	
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
    neighbor = m_organism->GetNeighbor();
    
    //if neighbor exists, do they have a different tag?
    if (neighbor != NULL) {
      if (m_organism->GetTagLabel() != neighbor->GetTagLabel()) found = true;      
    }
		
    // stop searching through the neighbors if we already found one
    if (found == true) {
      break;
    }
		
    m_organism->Rotate(ctx, 1);
    neighbor_id++;
  }
	
  return true;
}



/* Execute the next instruction if the neighbor was a donor. */ 
bool cHardwareCPU::Inst_IfDonor(cAvidaContext&) 
{
  bool donor = false;
  cOrganism * neighbor = m_organism->GetNeighbor();
  if (neighbor != NULL) {
    // check if the neighbor was a donor
    if (m_organism->IsDonor(neighbor->GetID())) {
      donor = true;
    }
  }
  if (!donor)  getIP().Advance();
	
  return true; 
}


void cHardwareCPU::ComputeReputation() 
{
  cOrganism * neighbor = m_organism->GetNeighbor();
  
  // update reputation to include this donation.
  // get the current reputation; increment by 1.
  // includes a concept of standing
  if (m_world->GetConfig().AUTO_REPUTATION.Get() == 1) {
    int my_rep = m_organism->GetReputation();
    m_organism->SetReputation(my_rep +1);
    // get neighbor reputation
    int rep = neighbor->GetReputation(); 
    // if the organism has not yet donated, put it into bad standing
    if (rep == 0) neighbor->SetReputation(-1);
  } else if (m_world->GetConfig().AUTO_REPUTATION.Get() == 2) {
    // reputation is proportional to how much you have donated/received
    int my_rep = m_organism->GetReputation();
    m_organism->SetReputation(my_rep +1);
    // get neighbor reputation
    int rep = neighbor->GetReputation(); 
    neighbor->SetReputation(rep-1);
  } else if (m_world->GetConfig().AUTO_REPUTATION.Get() == 3)  {
    // set rep to 1, since the organism donated
    m_organism->SetReputation(1);
    // get neighbor reputation
    int rep = neighbor->GetReputation(); 
    // if the organism has not yet donated, put it into bad standing
    if (rep == 0) neighbor->SetReputation(-1);		
  } else if (m_world->GetConfig().AUTO_REPUTATION.Get() == 4) {
    // Similar to 1, except does not include standing.
    int my_rep = m_organism->GetReputation();
    m_organism->SetReputation(my_rep +1);
  }
}



/* Check if the string in the organisms buffer corresponds to the 
 string it is producing. If so, -1 out the buffer and increment the 
 number of raw materials the organism has. Otherwise, do nothing. */

bool cHardwareCPU::Inst_ProduceString(cAvidaContext&)
{	
  int num = 0;
  int max_num = 0; 
  int max_string = -1;
  int string_size = 0;
  bool val; 
	
  m_organism->InitStringMap(); 
  
  // Figure out if it has produced any of the strings 
  std::vector < cString > temp_strings = m_world->GetEnvironment().GetMatchStringsFromTask(); 
  if (temp_strings.size()) string_size = temp_strings[0].GetSize();
  for (unsigned int i=0; i < temp_strings.size(); i++){
    num = m_organism->MatchOutputBuffer(temp_strings[i]); 
    if (num > max_num) { 
      max_num = num; 
      max_string = i; 
    }
  }
	
  // Determine if it has to produce one in particular. 
  if (m_world->GetConfig().SPECIALISTS.Get()) { 
    if (m_organism->GetLineageLabel() != max_string) { 
      max_num = 0;
    }
  }
	
  // If still ok, add the raw material and clear the buffer
  if (max_num == string_size) { 
    // Indicate organism has produced the string
    val = m_organism->ProduceString(max_string); 
    
    // temp until old code is phased out: 
    m_organism->AddSelfRawMaterials(1); 
    
    // Clear buffer if the organism has received credit for the string
    if (val) m_organism->SetOutputNegative1(); 
  }
  
  return true;
}

//! An organism joins a group by setting it opinion to the group id. 
bool cHardwareCPU::Inst_JoinGroup(cAvidaContext& ctx)
{
  int opinion = m_world->GetConfig().DEFAULT_GROUP.Get();
  // Check if the org is currently part of a group
  assert(m_organism != 0);
  
  int prop_group_id = GetRegister(FindModifiedRegister(REG_BX));
  
  // check if this is a valid group
  if (m_world->GetConfig().USE_FORM_GROUPS.Get() == 2 &&
      !(m_world->GetEnvironment().IsGroupID(prop_group_id))) return false;
  
  // injected orgs might not have an opinion
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    opinion = m_organism->GetOpinion().first;
    
    //return false if org setting opinion to current one (avoid paying costs for not switching)
    if (opinion == prop_group_id) return false;

    // A random chance for failure to join group based on config.
    if (m_world->GetConfig().JOIN_GROUP_FAILURE.Get() != 0) {
      int percent_failure = m_world->GetConfig().JOIN_GROUP_FAILURE.Get();
      double prob_failure = abs((double) percent_failure / 100.0);
      double rand = ctx.GetRandom().GetDouble();
      if (rand <= prob_failure && percent_failure > 0) return true;
      else if (rand <= prob_failure && percent_failure < 0) {
        m_organism->Die(ctx);
        return true;
      }
    }

    // If tolerances are on the org must pass immigration chance 
    if (m_world->GetConfig().TOLERANCE_WINDOW.Get() > 0) {
      m_organism->GetOrgInterface().AttemptImmigrateGroup(ctx, prop_group_id, m_organism);
      return true;
    }
    else {
      // otherwise, subtract org from current group
      m_organism->LeaveGroup(opinion);
    }
  }
  
  // Set the opinion
  m_organism->GetOrgInterface().SetOpinion(prop_group_id, m_organism);
  
  // Add org to group count
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    opinion = m_organism->GetOpinion().first;	
    m_organism->JoinGroup(opinion);
  }

  return true;
}

bool cHardwareCPU::Inst_JoinMTGroup(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) return false;  
  return Inst_JoinGroup(ctx);
}

/* Must be nop-modified.
 * Moves organism +1 group if the nop-register has a positive number,
 * moves organism -1 group if the nop-register has a negative number,
 * wraps from the top group back to group 1 (skipping 0). 
 */
bool cHardwareCPU::Inst_JoinNextGroup(cAvidaContext& ctx)
{
  // Check for an opinion.
  if (!m_organism->GetOrgInterface().HasOpinion(m_organism)) return false;
  
  if (m_world->GetConfig().USE_FORM_GROUPS.Get() != 2) return false;
  
  // There must be more than the org's current group and the 0 group, which is skipped.
  int num_groups = 0;
  std::set<int> fts_avail = m_world->GetEnvironment().GetGroupIDs();
  set <int>::iterator itr;    
  for (itr = fts_avail.begin();itr!=fts_avail.end();itr++) num_groups++; 

  if (num_groups <= 2) return false;
  
  // If not nop-modified, fails to execute.
  if (!(m_inst_set->IsNop(getIP().GetNextInst()))) return false;
  // Retrieves the value from the nop-modifying register.
  const int nop_reg = FindModifiedRegister(REG_BX);
  int reg_value = GetRegister(nop_reg);
  // If no group change
  if (reg_value == 0) return false;

  // A random chance for failure to join group based on config.
  if (m_world->GetConfig().JOIN_GROUP_FAILURE.Get() != 0) {
    int percent_failure = m_world->GetConfig().JOIN_GROUP_FAILURE.Get();
    double prob_failure = abs((double) percent_failure / 100.0);
    double rand = ctx.GetRandom().GetDouble();
    if (rand <= prob_failure && percent_failure > 0) return true;
    else if (rand <= prob_failure && percent_failure < 0) {
      m_organism->Die(ctx);
      return true;
    }
  }
  
  int opinion = m_organism->GetOpinion().first;
  int new_opinion = -1;
  if (opinion == (num_groups - 1)) {
    if (reg_value > 0) {
      new_opinion = 1;
    }
    else if (reg_value < 0) {
      new_opinion = opinion - 1;
    }
  }
  else if ((opinion == 1) || (opinion == 0)) {
    if (reg_value > 0) {
      new_opinion = opinion + 1;
    }
    else if (reg_value < 0) {
      new_opinion = num_groups - 1;
    }
  }
  else if ((opinion != (num_groups - 1)) && (opinion != 1) && (opinion != 0)) {
    if (reg_value > 0) {
      new_opinion = opinion + 1;
    }
    else if (reg_value < 0) {
      new_opinion = opinion - 1;
    }
  }
  if (new_opinion == -1) return false;
  
  if (m_world->GetConfig().TOLERANCE_WINDOW.Get() > 0) {
    m_organism->GetOrgInterface().AttemptImmigrateGroup(ctx, new_opinion, m_organism);
  }
  else {
    m_organism->GetOrgInterface().SetOpinion(new_opinion, m_organism);
    m_organism->LeaveGroup(opinion);
    m_organism->JoinGroup(new_opinion);
  }
  return true;
}

bool cHardwareCPU::Inst_JoinNextMTGroup(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) return false;  
  return Inst_JoinNextGroup(ctx);
}

//! Gets the number of organisms in the current organism's group 
//! and places the value in the ?BX? register
bool cHardwareCPU::Inst_NumberOrgsInMyGroup(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  
  int num_orgs = 0;
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    int opinion = m_organism->GetOpinion().first;
    num_orgs = m_organism->GetOrgInterface().NumberOfOrganismsInGroup(opinion);
  }
  const int num_org_reg = FindModifiedRegister(REG_BX);
  GetRegister(num_org_reg) = num_orgs;
  return true;
}

bool cHardwareCPU::Inst_NumberMTInMyGroup(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) return false;  

  int num_fem = 0;
  int num_male = 0;
  int num_juv = 0;
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    int opinion = m_organism->GetOpinion().first;
    num_fem = m_organism->GetOrgInterface().NumberGroupFemales(opinion);
    num_male = m_organism->GetOrgInterface().NumberGroupMales(opinion);
    num_juv = m_organism->GetOrgInterface().NumberGroupJuvs(opinion);
  }
  const int reg1 = FindModifiedRegister(REG_BX);  
  const int reg2 = FindModifiedNextRegister(reg1);  
  const int reg3 = FindModifiedNextRegister(reg2);  
  GetRegister(reg1) = num_fem;
  GetRegister(reg2) = num_male;
  GetRegister(reg3) = num_juv;
  return true;
}

//! Gets the number of organisms in the group of a given id
//! specified by the ?BX? register and places the value in the ?CX? register
bool cHardwareCPU::Inst_NumberOrgsInGroup(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  const int group_id = FindModifiedRegister(REG_BX);
  const int num_org_reg = FindModifiedRegister(REG_CX);
  
  int num_orgs = m_organism->GetOrgInterface().NumberOfOrganismsInGroup(group_id);
  GetRegister(num_org_reg) = num_orgs;
  return true;
}

bool cHardwareCPU::Inst_NumberMTInGroup(cAvidaContext& ctx)
{
  assert(m_organism != 0);
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) return false;  
  
  const int group_id = FindModifiedRegister(REG_BX);
  
  int num_fem = m_organism->GetOrgInterface().NumberGroupFemales(group_id);
  int num_male = m_organism->GetOrgInterface().NumberGroupMales(group_id);
  int num_juv = m_organism->GetOrgInterface().NumberGroupJuvs(group_id);
  const int reg1 = FindModifiedRegister(REG_BX);  
  const int reg2 = FindModifiedNextRegister(reg1);  
  const int reg3 = FindModifiedNextRegister(reg2);  
  GetRegister(reg1) = num_fem;
  GetRegister(reg2) = num_male;
  GetRegister(reg3) = num_juv;
  return true;
}


/* Must be nop-modified.
 Places the number of orgs in the +1 group in the BX register, if the nop-modifying register is positive,
 places the number of orgs in the -1 group in the BX register, if the nop-modifying register is negative,
 wraps from the top group back to group 1 (skipping 0). 
 */
bool cHardwareCPU::Inst_NumberNextGroup(cAvidaContext& ctx)
{
  // Check for an opinion.
  if (!m_organism->GetOrgInterface().HasOpinion(m_organism)) return false;
  
  if (m_world->GetConfig().USE_FORM_GROUPS.Get() != 2) return false;
  int opinion = m_organism->GetOpinion().first;
  
  const int num_groups = m_organism->GetOrgInterface().GetResources(ctx).GetSize();
  if (num_groups <= 2) return false;
  
  // If not nop-modified, fails to execute.
  if (!(m_inst_set->IsNop(getIP().GetNextInst()))) return false;
  // Retrieves the value from the nop-modifying register.
  const int nop_reg = FindModifiedRegister(REG_BX);
  int reg_value = GetRegister(nop_reg);
  // If no group change
  if (reg_value == 0) return false;
  
  int query_group = opinion;
  
  if (opinion == (num_groups - 1)) {
    if (reg_value > 0) query_group = 1;
    else if (reg_value < 0) query_group = opinion - 1;
  }
  else if ((opinion == 1) || (opinion == 0)) {
    if (reg_value > 0) query_group = opinion + 1;
    else if (reg_value < 0) query_group = num_groups - 1;
  }
  else if ((opinion != (num_groups - 1)) && (opinion != 1) && (opinion != 0)) {
    if (reg_value > 0) query_group = opinion + 1;
    else if (reg_value < 0) query_group = opinion - 1;
  }
  
  GetRegister(REG_BX) = m_organism->GetOrgInterface().NumberOfOrganismsInGroup(query_group);
  
  return true;
}

bool cHardwareCPU::Inst_NumberMTNextGroup(cAvidaContext& ctx)
{
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) return false;  

  // Check for an opinion.
  if (!m_organism->GetOrgInterface().HasOpinion(m_organism)) return false;
  
  if (m_world->GetConfig().USE_FORM_GROUPS.Get() != 2) return false;
  int opinion = m_organism->GetOpinion().first;
  
  const int num_groups = m_organism->GetOrgInterface().GetResources(ctx).GetSize();
  if (num_groups <= 2) return false;
  
  // If not nop-modified, fails to execute.
  if (!(m_inst_set->IsNop(getIP().GetNextInst()))) return false;
  // Retrieves the value from the nop-modifying register.
  const int nop_reg = FindModifiedRegister(REG_BX);
  int reg_value = GetRegister(nop_reg);
  // If no group change
  if (reg_value == 0) return false;
  
  int query_group = opinion;
  
  if (opinion == (num_groups - 1)) {
    if (reg_value > 0) query_group = 1;
    else if (reg_value < 0) query_group = opinion - 1;
  }
  else if ((opinion == 1) || (opinion == 0)) {
    if (reg_value > 0) query_group = opinion + 1;
    else if (reg_value < 0) query_group = num_groups - 1;
  }
  else if ((opinion != (num_groups - 1)) && (opinion != 1) && (opinion != 0)) {
    if (reg_value > 0) query_group = opinion + 1;
    else if (reg_value < 0) query_group = opinion - 1;
  }
  
  return true;
}

//Kill some other random organism in group 
bool cHardwareCPU::Inst_KillGroupMember(cAvidaContext& ctx)
{
  int opinion;
  // Check if the org is currently part of a group
  assert(m_organism != 0);
  
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    opinion = m_organism->GetOpinion().first;
    // Kill organism in group
    m_world->GetPopulation().KillGroupMember(ctx, opinion, m_organism);
  }  
  return true;
}

/* Increases tolerance towards the addition of members to the group:
 nop-A: increases tolerance towards immigrants
 nop-B: increases tolerance towards own offspring
 nop-C: increases tolerance towards other offspring of the group.
 Removes the record of a previous update when dec-tolerance was executed,
 and places the modified tolerance total in the BX register. 
 */
bool cHardwareCPU::Inst_IncTolerance(cAvidaContext& ctx)
{
  // Exit if tolerance is not enabled
  if (!m_world->GetConfig().USE_FORM_GROUPS.Get()) return false;
  if (m_world->GetConfig().TOLERANCE_WINDOW.Get() <= 0) return false;
  // Exit if organism is not in a group
  if (!m_organism->GetOrgInterface().HasOpinion(m_organism)) return false;
  // Exit if the instruction is not nop-modified
  if (!m_inst_set->IsNop(getIP().GetNextInst())) return false;
  
  int toleranceType = 0;
  if (m_world->GetConfig().TOLERANCE_VARIATIONS.Get() == 0) {
    const int tolerance_to_modify = FindModifiedRegister(REG_BX);
    
    toleranceType = -1;
    if (tolerance_to_modify == REG_AX) toleranceType = 0;
    else if (tolerance_to_modify == REG_BX) toleranceType = 1;
    else if (tolerance_to_modify == REG_CX) toleranceType = 2;
    
    // Not a recognized register
    if (toleranceType == -1) return false;
  }
   
  // Update the tolerance and store the result in register B
  int result = m_organism->GetOrgInterface().IncTolerance(toleranceType, ctx);
  if (result == -1) return false;
  GetRegister(REG_BX) = result;
  return true;
}

/* Decreases tolerance towards the addition of members to the group,
 nop-A: decreases tolerance towards immigrants
 nop-B: decreases tolerance towards own offspring
 nop-C: decreases tolerance towards other offspring of the group.
 Adds to records the update during which dec-tolerance was executed,
 and places the modified tolerance total in the BX register. 
 */
bool cHardwareCPU::Inst_DecTolerance(cAvidaContext& ctx)
{
  // Exit if tolerance is not enabled
  if (!m_world->GetConfig().USE_FORM_GROUPS.Get()) return false;
  if (m_world->GetConfig().TOLERANCE_WINDOW.Get() <= 0) return false;
  // Exit if organism is not in a group
  if (!m_organism->GetOrgInterface().HasOpinion(m_organism)) return false;
  // Exit if the instruction is not nop-modified
  if (!m_inst_set->IsNop(getIP().GetNextInst())) return false;
  
  int toleranceType = 0;
  if (m_world->GetConfig().TOLERANCE_VARIATIONS.Get() == 0) {
    const int tolerance_to_modify = FindModifiedRegister(REG_BX);
    
    toleranceType = -1;
    if (tolerance_to_modify == REG_AX) toleranceType = 0;
    else if (tolerance_to_modify == REG_BX) toleranceType = 1;
    else if (tolerance_to_modify == REG_CX) toleranceType = 2;
    
    // Not a recognized register
    if (toleranceType == -1) return false;
  }
  
  // Update the tolerance and store the result in register B
  GetRegister(REG_BX) = m_organism->GetOrgInterface().DecTolerance(toleranceType, ctx);
  return true;
}

/* Retrieve current tolerance levels, placing each tolerance in a different register.
 Register AX: tolerance towards immigrants
 Register BX: tolerance towards own offspring
 Register CX: tolerance towards other offspring in the group 
 */
bool cHardwareCPU::Inst_GetTolerance(cAvidaContext& ctx)
{
  bool exec_success = false;
  if (m_world->GetConfig().USE_FORM_GROUPS.Get() && m_world->GetConfig().TOLERANCE_WINDOW.Get() > 0) {
    if(m_organism->GetOrgInterface().HasOpinion(m_organism)) {
      m_organism->GetOrgInterface().PushToleranceInstExe(6, ctx);

      int tolerance_immigrants = m_organism->GetPhenotype().CalcToleranceImmigrants();
      int tolerance_own = m_organism->GetPhenotype().CalcToleranceOffspringOwn();
      int tolerance_others = m_organism->GetPhenotype().CalcToleranceOffspringOthers();
      GetRegister(REG_AX) = tolerance_immigrants;
      GetRegister(REG_BX) = tolerance_own;
      GetRegister(REG_CX) = tolerance_others;
      exec_success = true;
    }
  }
  return exec_success;
}  

/* Retrieve group tolerances placing each in a different register.
 Register AX: group tolerance towards immigrants
 Register BX: group tolerance towards own offspring
 Register CX: group tolerance towards offspring 
 */
bool cHardwareCPU::Inst_GetGroupTolerance(cAvidaContext& ctx)
{
  bool exec_success = false;
  // If groups are used and tolerances are on...
  if (m_world->GetConfig().USE_FORM_GROUPS.Get() && m_world->GetConfig().TOLERANCE_WINDOW.Get() > 0) {
    if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
      m_organism->GetOrgInterface().PushToleranceInstExe(7, ctx);

      const int group_id = m_organism->GetOpinion().first;

      int mating_type = -1;
      if (m_world->GetConfig().TOLERANCE_VARIATIONS.Get() == 2) {
        if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_FEMALE) mating_type = 0;
        else if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_MALE) mating_type = 1;
        else mating_type = 2;
      }
      double immigrant_odds = m_organism->GetOrgInterface().CalcGroupOddsImmigrants(group_id, mating_type);
      double offspring_own_odds = m_organism->GetOrgInterface().CalcGroupOddsOffspring(m_organism);
      double offspring_others_odds = m_organism->GetOrgInterface().CalcGroupOddsOffspring(group_id);
      
      // Convert all odds to percent
      double percent_immigrants = immigrant_odds * 100 + 0.5;
      double percent_offspring_own = offspring_own_odds * 100 + 0.5;
      double percent_offspring_others = offspring_others_odds * 100 + 0.5;
      
      // Truncate percent to integer and place in registers
      GetRegister(REG_AX) = (int) percent_immigrants;
      GetRegister(REG_BX) = (int) percent_offspring_own;
      GetRegister(REG_CX) = (int) percent_offspring_others;
      exec_success = true;
    }
  }
  return exec_success;
}




bool cHardwareCPU::Inst_GetTimeUsed(cAvidaContext&)
{
  GetRegister(FindModifiedRegister(REG_BX)) = m_organism->GetPhenotype().GetTimeUsed();
  return true;
}


void cHardwareCPU::IncrementTaskSwitchingCost(int cost)
{
  m_task_switching_cost += cost;
}


bool cHardwareCPU::Inst_ApplyPointMutations(cAvidaContext& ctx)
{
  // If repairs are off...
  if(m_world->GetConfig().POINT_MUT_REPAIR_START.Get() == 0) {
    double point_mut_prob = m_world->GetConfig().INST_POINT_MUT_PROB.Get();
    int num_mut = m_organism->GetHardware().PointMutate(ctx, point_mut_prob);
    m_organism->IncPointMutations(num_mut);
  } else {
    // incur cost of repairs.
    int cost = m_world->GetConfig().INST_POINT_REPAIR_COST.Get(); 
    m_task_switching_cost += cost;
  }
  return true;
}

bool cHardwareCPU::Inst_ApplyVaryingPointMutations(cAvidaContext& ctx)
{
  int last_task = m_organism->GetPhenotype().GetLastTaskID();
  // Check that the org performed a task...
  if (last_task != -1) {
    // Point mut prob is mutation rate * slope * task last performed
    double point_mut_prob = m_world->GetConfig().INST_POINT_MUT_PROB.Get() * m_world->GetConfig().INST_POINT_MUT_SLOPE.Get() * last_task;
    int num_mut = m_organism->GetHardware().PointMutate(ctx, point_mut_prob);
    m_organism->IncPointMutations(num_mut);
  }
  return true;
}





bool cHardwareCPU::Inst_RepairPointMutOn(cAvidaContext& ctx){
  m_organism->RepairPointMutOn();
  return true;
}

bool cHardwareCPU::Inst_RepairPointMutOff(cAvidaContext& ctx){
  m_organism->RepairPointMutOff();
  return true;
}

/***
    Mating type instructions
***/

bool  cHardwareCPU::Inst_SetMatingTypeMale(cAvidaContext&)
{
  //Check if the organism has already set its sex to female
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_FEMALE) {
    //If so, fail
    return false;
  } else {
    //Otherwise, set the current sex to male
    if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
      // since org can't be female by this point, only need to figure out if male (type 1) or juv (type 2)
      int old_type = 1;
      if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) old_type = 2;
      m_organism->GetOrgInterface().ChangeGroupMatingTypes(m_organism, m_organism->GetOpinion().first, old_type, 1);
    }
    m_organism->GetPhenotype().SetMatingType(MATING_TYPE_MALE);
  }
  return true;
}

bool  cHardwareCPU::Inst_SetMatingTypeFemale(cAvidaContext&)
{
  //Check if the organism has already set its sex to male
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_MALE) {
    //If so, fail
    return false;
  } else {
    //Otherwise, set the current sex to female
    if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
      // since org can't be male by this point, only need to figure out if female (type 0) or juv (type 2)
      int old_type = 0;
      if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) old_type = 2;
      m_organism->GetOrgInterface().ChangeGroupMatingTypes(m_organism, m_organism->GetOpinion().first, old_type, 0);
    }
    m_organism->GetPhenotype().SetMatingType(MATING_TYPE_FEMALE);
  }
  return true;
}

bool  cHardwareCPU::Inst_SetMatingTypeJuvenile(cAvidaContext&)
{
  //Set the organism's sex to juvenile
  //In this way, an organism that has already matured as male or female can change its sex
  // if this instruction is included in the instruction set
  if (m_organism->GetOrgInterface().HasOpinion(m_organism)) {
    int old_type = 0;
    if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_MALE) old_type = 1;
    else if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) old_type = 2;
    m_organism->GetOrgInterface().ChangeGroupMatingTypes(m_organism, m_organism->GetOpinion().first, old_type, 2);
  }
  m_organism->GetPhenotype().SetMatingType(MATING_TYPE_JUVENILE);
  return true;
}

bool cHardwareCPU::Inst_DivideSexMatingType(cAvidaContext& ctx)
{
  //Check if the organism is sexually mature
  if (m_organism->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) {
    //If not, fail
    return false;
  } else {
    //Otherwise, divide
    return Inst_HeadDivideSex(ctx);
  }
}

bool cHardwareCPU::Inst_IfMatingTypeMale(cAvidaContext&)
{
  //Execute the next instruction if the organism's mating type is male
  if (m_organism->GetPhenotype().GetMatingType() != MATING_TYPE_MALE)  getIP().Advance();
  return true; 
} 

bool cHardwareCPU::Inst_IfMatingTypeFemale(cAvidaContext&)
{
  //Execute the next instruction if the organism's mating type is female
  if (m_organism->GetPhenotype().GetMatingType() != MATING_TYPE_FEMALE)  getIP().Advance();
  return true; 
}

bool cHardwareCPU::Inst_IfMatingTypeJuvenile(cAvidaContext&)
{
  //Execute the next instruction if the organism has not matured sexually
  if (m_organism->GetPhenotype().GetMatingType() != MATING_TYPE_JUVENILE)  getIP().Advance();
  return true; 
}

bool cHardwareCPU::Inst_IncrementMatingDisplayA(cAvidaContext&)
{
  //Increment the organism's mating display A trait
  int counter = m_organism->GetPhenotype().GetCurMatingDisplayA();
  counter++;
  m_organism->GetPhenotype().SetCurMatingDisplayA(counter);
  return true;
}

bool cHardwareCPU::Inst_IncrementMatingDisplayB(cAvidaContext&)
{
  //Increment the organism's mating display A trait
  int counter = m_organism->GetPhenotype().GetCurMatingDisplayB();
  counter++;
  m_organism->GetPhenotype().SetCurMatingDisplayB(counter);
  return true;
}

bool cHardwareCPU::Inst_SetMatingDisplayA(cAvidaContext&)
//Sets the display value a to be equal to the value of ?BX?
{
  //Get the register and its contents as the new display value
  const int reg_used = FindModifiedRegister(REG_BX);
  const int new_display = GetRegister(reg_used);
  
  //Set the organism's mating display A trait
  m_organism->GetPhenotype().SetCurMatingDisplayA(new_display);
  return true;
}

bool cHardwareCPU::Inst_SetMatingDisplayB(cAvidaContext&)
//Sets the display value b to be equal to the value of ?BX?
{
  //Get the register and its contents as the new display value
  const int reg_used = FindModifiedRegister(REG_BX);
  const int new_display = GetRegister(reg_used);
  
  //Set the organism's mating display A trait
  m_organism->GetPhenotype().SetCurMatingDisplayB(new_display);
  return true;
}

bool cHardwareCPU::Inst_SetMatePreference(cAvidaContext&, int mate_pref)
{
  m_organism->GetPhenotype().SetMatePreference(mate_pref);
  return true;
}
bool cHardwareCPU::Inst_SetMatePreferenceHighestDisplayA(cAvidaContext& ctx) { return Inst_SetMatePreference(ctx, MATE_PREFERENCE_HIGHEST_DISPLAY_A); }
bool cHardwareCPU::Inst_SetMatePreferenceHighestDisplayB(cAvidaContext& ctx) { return Inst_SetMatePreference(ctx, MATE_PREFERENCE_HIGHEST_DISPLAY_B); }
bool cHardwareCPU::Inst_SetMatePreferenceRandom(cAvidaContext& ctx) { return Inst_SetMatePreference(ctx, MATE_PREFERENCE_RANDOM); }
bool cHardwareCPU::Inst_SetMatePreferenceHighestMerit(cAvidaContext& ctx) { return Inst_SetMatePreference(ctx, MATE_PREFERENCE_HIGHEST_MERIT); }
