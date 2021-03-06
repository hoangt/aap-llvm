//===-- AAPInstrInfo.cpp - AAP Instruction Information ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the AAP implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "AAPInstrInfo.h"
#include "AAPMachineFunctionInfo.h"
#include "MCTargetDesc/AAPMCTargetDesc.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "AAPGenInstrInfo.inc"

// Pin the vtable to this file.
void AAPInstrInfo::anchor() {}

AAPInstrInfo::AAPInstrInfo(AAPSubtarget &STI)
    : AAPGenInstrInfo(AAP::ADJCALLSTACKDOWN, AAP::ADJCALLSTACKUP), TRI() {}

//===----------------------------------------------------------------------===//
// Branch Analysis
//===----------------------------------------------------------------------===//

bool AAPInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                 MachineBasicBlock *&TBB,
                                 MachineBasicBlock *&FBB,
                                 SmallVectorImpl<MachineOperand> &Cond,
                                 bool AllowModify) const {
  return true;
}

unsigned AAPInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TBB,
                                    MachineBasicBlock *FBB,
                                    ArrayRef<MachineOperand> Cond,
                                    const DebugLoc &DL,
                                    int *BytesAdded) const {
  return 0;
}

unsigned AAPInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                    int *BytesRemoved) const {
  return 0;
}

void AAPInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I,
                               const DebugLoc &DL, unsigned DestReg,
                               unsigned SrcReg, bool KillSrc) const {
  assert(AAP::GR64RegClass.contains(DestReg) &&
         AAP::GR64RegClass.contains(DestReg) &&
         "Impossible register-register copy");

  // FIXME: If possible with short insn, build that instead
  BuildMI(MBB, I, DL, get(AAP::MOV_r), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void AAPInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI,
                                       unsigned SrcReg, bool isKill,
                                       int FrameIdx,
                                       const TargetRegisterClass *RC,
                                       const TargetRegisterInfo *TRI) const {
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFrameInfo = MF.getFrameInfo();

  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOStore, MFrameInfo.getObjectSize(FrameIdx),
      MFrameInfo.getObjectAlignment(FrameIdx));

  assert((RC == &AAP::GR8RegClass || RC == &AAP::GR64RegClass) &&
         "Unknown register class to store to stack slot");

  BuildMI(MBB, MI, DL, get(AAP::STW))
      .addFrameIndex(FrameIdx)
      .addImm(0)
      .addReg(SrcReg, getKillRegState(isKill))
      .addMemOperand(MMO);
}

void AAPInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI,
                                        unsigned DstReg, int FrameIdx,
                                        const TargetRegisterClass *RC,
                                        const TargetRegisterInfo *TRI) const {
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFrameInfo = MF.getFrameInfo();

  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOLoad, MFrameInfo.getObjectSize(FrameIdx),
      MFrameInfo.getObjectAlignment(FrameIdx));

  assert((RC == &AAP::GR8RegClass || RC == &AAP::GR64RegClass) &&
         "Unknown register class to store to stack slot");

  BuildMI(MBB, MI, DL, get(AAP::LDW), DstReg)
      .addFrameIndex(FrameIdx)
      .addImm(0)
      .addMemOperand(MMO);
}

/// ReverseBranchCondition - Return the inverse opcode of the
/// specified Branch instruction.
bool AAPInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  return false;
}
