// For open-source license, please refer to [License](https://github.com/HikariObfuscator/Hikari/wiki/License).
//===----------------------------------------------------------------------===//
#include "llvm/Transforms/Obfuscation/Obfuscation.h"
#include "llvm/Transforms/Obfuscation/CryptoUtils.h"
#include "llvm/Transforms/Utils.h"    //wangchuanju 2021-11-23
#include "llvm/InitializePasses.h"    //wangchuanju 2021-12-15
#include <fcntl.h>

using namespace llvm;

namespace {

struct Flattening : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  bool flag;

  Flattening() : FunctionPass(ID) { 
    this->flag = true; 
    initializeFlatteningPass(*PassRegistry::getPassRegistry());   //wangchuanju 2021-12-15
  }   

  Flattening(bool flag) : FunctionPass(ID) { 
    this->flag = flag; 
    initializeFlatteningPass(*PassRegistry::getPassRegistry());   //wangchuanju 2021-12-15
  }

  bool runOnFunction(Function &F) override;

  bool flatten(Function *f);

  //++[ wangchuanju 2021-12-15
  void getAnalysisUsage(AnalysisUsage &AU) const override
  {
    AU.addRequiredID(LowerSwitchID);
    FunctionPass::getAnalysisUsage(AU);
  }
  //]++ wangchuanju 2021-12-15

};

} // namespace


char Flattening::ID = 0;

FunctionPass *llvm::createFlatteningPass(bool flag) { return new Flattening(flag); }

FunctionPass *llvm::createFlatteningPass() { return new Flattening(); }

// INITIALIZE_PASS(Flattening, "cffobf", "Enable Control Flow Flattening.", true,
//                 true)

// ++[ wangchuanju 2021-12-15
// When LLVM>=9.0, LowerSwitchPass depends on LazyValueInfoWrapperPass. Invoking directly will 
// cause AssertError. Insted it with Dependency. 
INITIALIZE_PASS_BEGIN(Flattening, "cffobf", "Enable Control Flow Flattening.", true, true)
INITIALIZE_PASS_DEPENDENCY(LowerSwitchLegacyPass)
INITIALIZE_PASS_END(Flattening, "cffobf", "Enable Control Flow Flattening.", true, true)
// ]++ wangchuanju 2021-12-15

bool Flattening::runOnFunction(Function &F) {
  Function *tmp = &F;
  bool ret = false;
  // Do we obfuscate
  if (toObfuscate(flag, tmp, "fla")) {
    errs() << "Running ControlFlowFlattening on function: " << F.getName() << "\n";
    ret = flatten(tmp);
    errs() << "Finished ControlFlowFlattening on function: " << F.getName() << "\n";
  }

  return ret;
}

bool Flattening::flatten(Function *f) {
  vector<BasicBlock *> origBB;
  BasicBlock *loopEntry;
  BasicBlock *loopEnd;
  LoadInst *load;
  SwitchInst *switchI;
  AllocaInst *switchVar;

  // SCRAMBLER
  std::map<uint32_t,uint32_t> scrambling_key;
  // END OF SCRAMBLER

#if LLVM_VERSION_MAJOR >= 9
    // >=9.0, LowerSwitchPass depends on LazyValueInfoWrapperPass, which cause AssertError.
    // So I move LowerSwitchPass into register function, just before FlatteningPass.
#else
  // Lower switch
  FunctionPass *lower = createLowerSwitchPass();
  lower->runOnFunction(*f);
#endif

  // Save all original BB
  for (Function::iterator i = f->begin(); i != f->end(); ++i) {
    BasicBlock *tmp = &*i;
    if (tmp->isEHPad() || tmp->isLandingPad()) {
          errs() << f->getName() <<" Contains Exception Handing Instructions and is unsupported for flattening in the open-source version of Hikari.\n";
          return false;
    }
    origBB.push_back(tmp);

    BasicBlock *bb = &*i;
    if (!isa<BranchInst>(bb->getTerminator()) && !isa<ReturnInst>(bb->getTerminator())) {
      return false;
    }
  }

  // Nothing to flatten
  if (origBB.size() <= 1) {
    return false;
  }

  // Remove first BB
  origBB.erase(origBB.begin());

  // Get a pointer on the first BB
  Function::iterator tmp = f->begin(); //++tmp;
  BasicBlock *insert = &*tmp;

  // If main begin with an if
  BranchInst *br = NULL;
  if (isa<BranchInst>(insert->getTerminator())) {
    br = cast<BranchInst>(insert->getTerminator());
  }

  if ((br != NULL && br->isConditional()) ||
      insert->getTerminator()->getNumSuccessors() > 1) {
    BasicBlock::iterator i = insert->end();
    --i;

    if (insert->size() > 1) {
      --i;
    } else { 
      errs() << "Flattening :  ...insert->size() <=1 .. \n"; 
    }

    BasicBlock *tmpBB = insert->splitBasicBlock(i, "first");
    origBB.insert(origBB.begin(), tmpBB);
  }

  // Remove jump
  Instruction* oldTerm = insert->getTerminator();

  // Create switch variable and set as it
  switchVar = new AllocaInst(Type::getInt32Ty(f->getContext()), 0, "switchVar", oldTerm);
  oldTerm->eraseFromParent();
 
  new StoreInst(
      ConstantInt::get(Type::getInt32Ty(f->getContext()),
                       llvm::cryptoutils->scramble32(0, scrambling_key)),
      switchVar, insert);

  // Create main loop
  loopEntry = BasicBlock::Create(f->getContext(), "loopEntry", f, insert);
  loopEnd = BasicBlock::Create(f->getContext(), "loopEnd", f, insert);

#if LLVM_VERSION_MAJOR >= 10    //not so sure which version required
  load = new LoadInst(switchVar->getType()->getElementType(), switchVar, "switchVar", loopEntry);       //wangchuanju 2021-11-23
#else
  load = new LoadInst(switchVar, "switchVar", loopEntry);
#endif

  // Move first BB on top
  insert->moveBefore(loopEntry);

  BranchInst::Create(loopEntry, insert);

  // loopEnd jump to loopEntry
  BranchInst::Create(loopEntry, loopEnd);

  BasicBlock *swDefault = BasicBlock::Create(f->getContext(), "switchDefault", f, loopEnd);
  BranchInst::Create(loopEnd, swDefault);

  // Create switch instruction itself and set condition
  switchI = SwitchInst::Create(&*f->begin(), swDefault, 0, loopEntry);
  switchI->setCondition(load);

  // Remove branch jump from 1st BB and make a jump to the while
  f->begin()->getTerminator()->eraseFromParent();      
  BranchInst::Create(loopEntry, &*f->begin());

  // Put all BB in the switch
  for (vector<BasicBlock *>::iterator b = origBB.begin(); b != origBB.end();
       ++b) {
    BasicBlock *i = *b;
    ConstantInt *numCase = NULL;

    // Move the BB inside the switch (only visual, no code logic)
    i->moveBefore(loopEnd);

    // Add case to switch
    numCase = cast<ConstantInt>(ConstantInt::get(
        switchI->getCondition()->getType(),
        llvm::cryptoutils->scramble32(switchI->getNumCases(), scrambling_key)));
    switchI->addCase(numCase, i);
  }

  // Recalculate switchVar
  for (vector<BasicBlock *>::iterator b = origBB.begin(); b != origBB.end();
       ++b) {
    BasicBlock *i = *b;
    ConstantInt *numCase = NULL;

    // Ret BB
    if (i->getTerminator()->getNumSuccessors() == 0) {
      continue;
    }

    // If it's a non-conditional jump
    if (i->getTerminator()->getNumSuccessors() == 1) {
      // Get successor and delete terminator
      BasicBlock *succ = i->getTerminator()->getSuccessor(0);
      i->getTerminator()->eraseFromParent();

      // Get next case
      numCase = switchI->findCaseDest(succ);

      // If next case == default case (switchDefault)
      if (numCase == NULL) {
        numCase = cast<ConstantInt>(
            ConstantInt::get(switchI->getCondition()->getType(),
                             llvm::cryptoutils->scramble32(
                                 switchI->getNumCases() - 1, scrambling_key)));
        switchI->addCase(numCase, succ);
      }

      // Update switchVar and jump to the end of loop
      new StoreInst(numCase, load->getPointerOperand(), i);
      BranchInst::Create(loopEnd, i);
      continue;
    }

    // If it's a conditional jump      
    // if (i->getTerminator()->getNumSuccessors() == 2) {
    //wangchuanju 2021-12-16
    if ( i->getTerminator()->getNumSuccessors() == 2 && isa<BranchInst>(i->getTerminator()) ) {
      BasicBlock *succTrue = i->getTerminator()->getSuccessor(0);
      BasicBlock *succFalse = i->getTerminator()->getSuccessor(1);

      // Get next cases
      ConstantInt *numCaseTrue = switchI->findCaseDest(i->getTerminator()->getSuccessor(0));
      ConstantInt *numCaseFalse = switchI->findCaseDest(i->getTerminator()->getSuccessor(1));

      // Check if next case == default case (switchDefault)
      if (numCaseTrue == NULL) {
        numCaseTrue = cast<ConstantInt>(
            ConstantInt::get(switchI->getCondition()->getType(),
                             llvm::cryptoutils->scramble32(
                                 switchI->getNumCases() - 1, scrambling_key)));
        switchI->addCase(numCaseTrue, succTrue);
      }

      if (numCaseFalse == NULL) {
        numCaseFalse = cast<ConstantInt>(
            ConstantInt::get(switchI->getCondition()->getType(),
                             llvm::cryptoutils->scramble32(
                                 switchI->getNumCases() - 1, scrambling_key)));
        switchI->addCase(numCaseFalse, succFalse);
      }

      // FIXME: 这里不一定能转换成功。。。可能是IndirectInst ??
      // Create a SelectInst
      if (!isa<BranchInst>(i->getTerminator())) {
        errs() << "Flattening : i->getTerminator() is not a BranchInst. This will cause crash. \n";
      }
      BranchInst *br = cast<BranchInst>(i->getTerminator());
      SelectInst *sel =
          SelectInst::Create(br->getCondition(), numCaseTrue, numCaseFalse, "",
                             i->getTerminator());

      // Erase terminator
      i->getTerminator()->eraseFromParent();
      // Update switchVar and jump to the end of loop
      new StoreInst(sel, load->getPointerOperand(), i);
      BranchInst::Create(loopEnd, i);
      continue;
    }
  }

  errs()<<"Fixing Stack\n";

  fixStack(f);

  errs()<<"Fixed Stack\n";

  return true;
}
