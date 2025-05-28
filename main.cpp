#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <optional>
#include <fstream>

// #include "script_tokenizer.hpp"
#include "script_ast.hpp"
#include "script_parser.hpp"
#include "script_compiler.hpp"
#include "hot_reload.hpp"
#include "platform.hpp"

#include "llvm/Support/DynamicLibrary.h" // <--- ADD THIS INCLUDE

// using namespace Mycelium::UI::Lang;
using namespace Mycelium::Scripting::Lang;

void Compile(std::string input)
{
    // ... (rest of your Compile function remains the same)
    std::cout << "--- Input ---" << std::endl;
    std::cout << input << std::endl;
    std::cout << "-------------" << std::endl;

    try
    {
        // Parsing
        std::cout << "\n--- Parsing ---" << std::endl;
        ScriptParser parser(input, "test.sp");
        auto result = parser.parse();
        auto AST = result.first;
       for (const auto &error : result.second)
        {
            std::cerr << "Parse Error: " << error.message << " at " << error.location.to_string() << std::endl;
        }
        if (!result.second.empty() && !AST) { // If there were errors and AST is null
            std::cerr << "Parsing failed to produce an AST due to errors." << std::endl;
            return;
        }
        if (!AST) {
             std::cerr << "Parsing produced a null AST without explicit errors. Aborting compilation." << std::endl;
            return;
        }
        std::cout << "Parsing Successful!" << std::endl;
        std::cout << "---------------" << std::endl;


        // compile
        std::cout << "\n--- Compiling ---" << std::endl;
        ScriptCompiler compiler; // Compiler is created
        compiler.compile_ast(AST, "MyceliumModule"); // AST is compiled

        std::ofstream outFile("tests/build/test.ll");
        if (outFile)
        {
            outFile << compiler.get_ir_string();
            outFile.close();
        }

        // compiler.compile_to_object_file("tests/build/test.o");

        std::cout << "Compilation Successful!" << std::endl;
        std::cout << "----------------" << std::endl;

        // JIT execution
        std::cout << "\n--- JIT Execution ---" << std::endl;
        // The compiler instance used for compile_ast is the same one used for jit_execute_function
        auto value = compiler.jit_execute_function("main", {}); // Assuming main returns int now
        std::cout << "Output (IntVal): " << value.IntVal.getSExtValue() << std::endl; // Use getSExtValue() for APInt
        std::cout << "JIT Execution Successful!" << std::endl;
        std::cout << "---------------------" << std::endl;

    }
    catch (const std::runtime_error &e)
    {
        // More specific error context could be helpful here
        std::cerr << "\n*** RUNTIME ERROR DURING COMPILATION/JIT ***" << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        // Consider printing the stack trace if possible, or more context from your compiler/parser.
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n*** UNEXPECTED STANDARD EXCEPTION ***" << std::endl;
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }
    // It's good practice to catch more specific LLVM exceptions if you can identify them,
    // or at least be aware that LLVM operations can throw.
}

int main()
{
    // --- ADD THIS LINE AT THE VERY BEGINNING OF main() ---
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    // ------------------------------------------------------

    HotReload fileReloader({"tests/test.sp"}, [](const std::string &filePath, const std::string &newContent)
    {
        std::cout << "\n--- " << filePath << " Reloaded ---" << std::endl;
        Compile(newContent);
    });

    fileReloader.poll_changes(); // Initial compile

    while (true)
    {
        fileReloader.poll_changes();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
