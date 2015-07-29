//===-- AAPISelDAGToDAG.cpp - A dag to dag inst selector for AAP ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the AAP target.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "AAP-isel"
#include "AAP.h"
#include "AAPMachineFunctionInfo.h"
#include "AAPRegisterInfo.h"
#include "AAPSubtarget.h"
#include "AAPTargetMachine.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// AAPDAGToDAGISel - AAP specific code to select AAP machine
// instructions for SelectionDAG operations.
//===----------------------------------------------------------------------===//
namespace {

class AAPDAGToDAGISel : public SelectionDAGISel {

  /// TM - Keep a reference to AAPTargetMachine.
  AAPTargetMachine &TM;

public:
  AAPDAGToDAGISel(AAPTargetMachine &tm, CodeGenOpt::Level OptLevel)
      : SelectionDAGISel(tm), TM(tm) {}

  // Pass Name
  virtual const char *getPassName() const {
    return "AAP DAG->DAG Pattern Instruction Selection";
  }

// Include the pieces autogenerated from the target description.
#include "AAPGenDAGISel.inc"

private:
  /// getTargetMachine - Return a reference to the TargetMachine, casted
  /// to the target-specific type.
  const AAPTargetMachine &getTargetMachine() {
    return static_cast<const AAPTargetMachine &>(TM);
  }

  SDNode *Select(SDNode *N);

  // Complex Pattern for address selection.
  bool SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset);
  bool SelectAddr_MO3(SDValue Addr, SDValue &Base, SDValue &Offset);
  bool SelectAddr_MO10(SDValue Addr, SDValue &Base, SDValue &Offset);

  // getI32Imm - Return a target constant with the specified value, of type i32.
  inline SDValue getI32Imm(unsigned Imm) {
    return CurDAG->getTargetConstant(Imm, MVT::i32);
  }
};
} // end anonymous namespace

/// Select instructions not customized! Used for
/// expanded, promoted and normal instructions
SDNode *AAPDAGToDAGISel::Select(SDNode *Node) {
  unsigned Opcode = Node->getOpcode();
  SDLoc dl(Node);

  // Dump information about the Node being selected
  DEBUG(errs() << "Selecting: "; Node->dump(CurDAG); errs() << "\n");

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    DEBUG(errs() << "== "; Node->dump(CurDAG); errs() << "\n");
    return NULL;
  }

  ///
  // Instruction Selection not handled by the auto-generated
  // tablegen selection should be handled here.
  ///
  switch (Opcode) {
  case ISD::FrameIndex: {
    assert(Node->getValueType(0) == MVT::i16);

    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i16);

    // Handle single use
    // This is not correct if we have a frame pointer, as the offset will be
    // negative
    return CurDAG->getMachineNode(AAP::ADD_i10, dl, MVT::i16,
                                  TFI, CurDAG->getTargetConstant(0, MVT::i16));
  }
  default:
    break;
  }

  // Select the default instruction
  SDNode *ResNode = SelectCode(Node);

  DEBUG(errs() << "=> ");
  if (ResNode == NULL || ResNode == Node)
    DEBUG(Node->dump(CurDAG));
  else
    DEBUG(ResNode->dump(CurDAG));
  DEBUG(errs() << "\n");
  return ResNode;
}

static bool isImm3(int64_t Imm) { return (Imm >= 0 && Imm <= 7); }
static bool isOff10(int64_t Imm) { return (Imm >= -512 && Imm <= 511); }

bool AAPDAGToDAGISel::SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset) {
  // if Address is FI, get the TargetFrameIndex
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
    Offset = CurDAG->getTargetConstant(0, MVT::i16);
    return true;
  }

  if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
       Addr.getOpcode() == ISD::TargetGlobalAddress)) {
    return false;
  }

  // Addresses of the form FI+const or FI|const
  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1));
    if (isInt<16>(CN->getSExtValue())) {
      // If the first operand is a FI, get the TargetFI Node
      if (FrameIndexSDNode *FIN =
              dyn_cast<FrameIndexSDNode>(Addr.getOperand(0))) {
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
      } else {
        Base = Addr.getOperand(0);
      }

      Offset = CurDAG->getTargetConstant(CN->getZExtValue(), MVT::i16);
      return true;
    }
  }
  return false;
}

bool AAPDAGToDAGISel::SelectAddr_MO3(SDValue Addr, SDValue &Base,
                                     SDValue &Offset) {
  SDValue B, O;
  bool ret = SelectAddr(Addr, B, O);
  if (ret && isa<ConstantSDNode>(O)) {
    int64_t c = dyn_cast<ConstantSDNode>(O)->getSExtValue();
    if (isImm3(c)) {
      Base = B;
      Offset = O;
      return true;
    }
  }
  return false;
}
bool AAPDAGToDAGISel::SelectAddr_MO10(SDValue Addr, SDValue &Base,
                                     SDValue &Offset) {
  SDValue B, O;
  bool ret = SelectAddr(Addr, B, O);
  if (ret && isa<ConstantSDNode>(O)) {
    int64_t c = dyn_cast<ConstantSDNode>(O)->getSExtValue();
    if (isOff10(c)) {
      Base = B;
      Offset = O;
      return true;
    }
  }
  return false;
}

/// createAAPISelDag - This pass converts a legalized DAG into a
/// AAP-specific DAG, ready for instruction scheduling.
FunctionPass *llvm::createAAPISelDag(AAPTargetMachine &TM,
                                     CodeGenOpt::Level OptLevel) {
  return new AAPDAGToDAGISel(TM, OptLevel);
}
