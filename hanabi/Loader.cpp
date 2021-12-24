// For open-source license, please refer to [License](https://github.com/HikariObfuscator/Hikari/wiki/License).
#include "substrate.h"
#include <llvm/Transforms/Obfuscation/Obfuscation.h>
#include <llvm/Config/abi-breaking.h>
#include <string>
#include <mach-o/dyld.h>

#if LLVM_ENABLE_ABI_BREAKING_CHECKS==1
#error "Configure LLVM with -DLLVM_ABI_BREAKING_CHECKS=FORCE_OFF"
#endif

using namespace std;


// llvm::PassManagerBuilder::populateModulePassManager(llvm::legacy::PassManagerBase&)
#define SYMBOL_POPULATE_PASSMANAGER  	"__ZN4llvm18PassManagerBuilder25populateModulePassManagerERNS_6legacy15PassManagerBaseE"
// llvm::Pass* llvm::callDefaultCtor<(anonymous namespace)::LowerSwitch>()
#define SYMBOL_CALL_DEFAULT_CTOR 		"__ZN4llvm15callDefaultCtorIN12_GLOBAL__N_111LowerSwitchEEEPNS_4PassEv"



void (*old_pmb)(void* dis, legacy::PassManagerBase &MPM);

Pass* (*old_get_LS)();

// llvm::createLowerSwitchPass(void)
extern "C" Pass* _ZN4llvm21createLowerSwitchPassEv(){
  errs() << "Loader.cpp -- _ZN4llvm21createLowerSwitchPassEv -- calling old_get_LS() \n";
  return old_get_LS();
}

static void new_pmb(void* dis,legacy::PassManagerBase &MPM){
  errs() << "Loader.cpp -- new_pmb -- start... \n";
  MPM.add(createObfuscationPass());
  errs() << "Loader.cpp -- new_pmb -- calling old_pmb \n";
  old_pmb(dis, MPM);
  errs() << "Loader.cpp -- new_pmb -- finished \n";
}

static __attribute__((__constructor__)) void Inj3c73d(int argc, char* argv[]){
  char* executablePath=argv[0];
  fprintf(stderr, "Loader.cpp -- Inj3c73d -- argc: %d,  executablePath: %s \n", argc, executablePath);

  //Initialize our own LLVM Library
  MSImageRef exeImagemage = MSGetImageByName(executablePath);
  errs() << "Loader.cpp -- Inj3c73d -- Applying Apple Clang Hooks...\n";
  MSHookFunction((void*)MSFindSymbol(exeImagemage, SYMBOL_POPULATE_PASSMANAGER), (void*)new_pmb, (void**)&old_pmb);
  old_get_LS=(Pass* (*)())MSFindSymbol(exeImagemage, SYMBOL_CALL_DEFAULT_CTOR);
  errs() << "Loader.cpp -- Inj3c73d -- Hook finished \n";
}
