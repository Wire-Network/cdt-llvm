//===- SysioApply ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Pass.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <string>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "sysio_apply"
static cl::opt<std::string> entry_opt (
   "entry",
   cl::desc("Specify entry point")
);

namespace {
  // SysioApply - Mutate the apply function as needed
  struct SysioApplyPass : public FunctionPass {
    static char ID;
    SysioApplyPass() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override {
       if (F.hasFnAttribute("sysio_wasm_entry") || F.getName().equals("apply")) {
         auto wasm_ctors = F.getParent()->getOrInsertFunction("__wasm_call_ctors", AttributeList{}, Type::getVoidTy(F.getContext()));
         auto wasm_dtors = F.getParent()->getOrInsertFunction("__cxa_finalize", AttributeList{}, Type::getVoidTy(F.getContext()), Type::getInt32Ty(F.getContext()));

         IRBuilder<> builder(&F.getEntryBlock());
         builder.SetInsertPoint(&(F.getEntryBlock().front()));

         auto set_contract = F.getParent()->getOrInsertFunction("sysio_set_contract_name", AttributeList{}, Type::getVoidTy(F.getContext()), Type::getInt64Ty(F.getContext()));

         CallInst* set_contract_call = builder.CreateCall(set_contract, {F.arg_begin()}, "");
         if (const Function* F_ = dyn_cast<const Function>(set_contract.getCallee()->stripPointerCasts()))
            set_contract_call->setCallingConv(F_->getCallingConv());

         CallInst* wasm_ctor_call = builder.CreateCall(wasm_ctors, {}, "");
         if (const Function* F_ = dyn_cast<const Function>(wasm_ctors.getCallee()->stripPointerCasts()))
            wasm_ctor_call->setCallingConv(F_->getCallingConv());

         for ( Function::iterator bb = F.begin(); bb != F.end(); bb++ ) {
            if (isa<ReturnInst>((*bb).getTerminator())) {
               builder.SetInsertPoint((*bb).getTerminator());
               // for now just call with null
               CallInst* wasm_dtor_call = builder.CreateCall(wasm_dtors, {Constant::getNullValue(Type::getInt32Ty(F.getContext()))}, "");
               if (const Function* F_ = dyn_cast<Function>(wasm_dtors.getCallee()->stripPointerCasts()))
                  wasm_dtor_call->setCallingConv(F_->getCallingConv());
            }
         }

         return true;
      }
      return false;
    }
  };
}

char SysioApplyPass::ID = 0;
static RegisterPass<SysioApplyPass> X("apply_fixup", "Sysio Apply Fixups");

static void registerSysioApplyPass(const PassManagerBuilder&, legacy::PassManagerBase& PM) { PM.add(new SysioApplyPass()); }
static RegisterStandardPasses RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible, registerSysioApplyPass);
