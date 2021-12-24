# hikari-modules
Hikari Obfuscator toolchain 


BUILD

1. Get the source code of LLVM applicable version from llvm or apple. 
   (The LLVM version of Xcode 13.1 is 12.0, despite the displayed version of Xcode 13.1 is 13.0.0.)

   llvm repository:  https://github.com/llvm/llvm-project
   apple repository: https://github.com/apple/llvm-project

2. Copy hanabi to directory llvm/projects. A better alternative is just to create symbolic link named "hanabi", which links to the hanabi source code directory.

3. Copy files under "Headers" to directory llvm/include/llvm/Transforms/Obfuscation. A better alternative is just to create symbolic link named "Obfuscation", which links to the "Headers" directory.

4. Copy files under "Core" to directory llvm/lib/Transforms/Obfuscation. A better alternative is just to create symbolic link named "Obfuscation", which links to the "Core" directory.

5. Create a directory "build" under llvm.

6. Create build configuration under directory "build".

   cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DLLVM_CREATE_XCODE_TOOLCHAIN=ON -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64" -DLLVM_TARGET_ARCH=host -DLLVM_ABI_BREAKING_CHECKS=FORCE_OFF ../

7. Build hanabi dylib. (switch to directory llvm/build)

   ninja -j6

   If ninja is not the specified generator (e.g. no -GNinja parameter) during build configuration, you should execute make insteadly.

   make -j6



INJECTION

1. Make a copy of Xcode's default xctoochain to a new one. 

   cp /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain /Library/Developer/Toolchains/Hikari-Xcode13.1.xctoolchain

2. Copy libLLVMHanabi.dylib, libLLVMHanabiDeps.dylib under llvm/build/lib to the new xctoolchain's usr/bin directory.

   cp -f ./lib/libLLVMHanabi.dylib ./lib/libLLVMHanabiDeps.dylib  /Library/Developer/Toolchains/Hikari-Xcode13.1.xctoolchain/usr/bin/  

3. Copy libsubstitute.dylib (built in substitue) to the new xctoolchain's usr/bin directory.

   cp -f ./out/libsubstitute.dylib /Library/Developer/Toolchains/Hikari-Xcode13.1.xctoolchain/usr/bin 

4. Copy optool (built in optool) to /usr/local/bin.   

4. Inject the dylibs into clang under the new xctoolchain.
   Under directory usr/bin of the new xctoolchain, execute the following commands one by one. ( !!!ORDER IS VERY IMPORTANT!!! )

   sudo optool install  -c load -p @executable_path/libsubstitute.dylib -t /Library/Developer/ToolChains/Hikari-Xcode13.1.xctoolchain/usr/bin/clang -b

   sudo optool install  -c load -p @executable_path/libLLVMHanabiDeps.dylib -t /Library/Developer/ToolChains/Hikari-Xcode13.1.xctoolchain/usr/bin/clang -b

   sudo optool install  -c load -p @executable_path/libLLVMHanabi.dylib -t /Library/Developer/ToolChains/Hikari-Xcode13.1.xctoolchain/usr/bin/clang -b


USAGE

The most frequently used compile flags:
-mllvm -enable-strcry -mllvm -enable-bcfobf -mllvm -enable-cffobf -mllvm -enable-subobf


ATTENTION: If too large code file is set with Hikari compile flags (e.g. >= 500K), the building may exhaust all the memory, which would cause the system as in state of "no response". 


