#include "cling/Interpreter/Interpreter.h"
#include "cling/Interpreter/Value.h"
#include "core/container/string_view.h"

void execute(string_view str) {
    const char* LLVMRESDIR = "/Users/antonellacalvia/Desktop/Coding/NextEngine/Notec/vendor/inst"; //path to llvm resource directory
    const char* args[] = {"/Users/antonellacalvia/Desktop/Coding/NextEngine/bin/Debug-macosx-x86_64/TheUnpluggingRunner/TheUnpluggingRunner", "-std=c++17"};
    
    cling::Interpreter interp(1, args, LLVMRESDIR);
    
    interp.AddIncludePath("/Users/antonellacalvia/Desktop/Coding/NextEngine/NextCore/include");
    
    printf("%s", str.c_str());
    
    interp.process("#include <core/container/vector.h>");
    
    try {
        cling::Value value;
        cling::Interpreter::CompilationResult result = interp.process(str.c_str(), &value);
        
        if (result == cling::Interpreter::kSuccess) {
            if (value.hasValue()) value.dump();
        } else {
            printf("Compilation Failed!!!!");
        }
    } catch (std::exception) {
        printf("Compilation Failed!");
    }
}
