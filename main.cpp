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


// using namespace Mycelium::UI::Lang;
using namespace Mycelium::Scripting::Lang;

void Compile(std::string input)
{
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
        std::cout << "Parsing Successful!" << std::endl;
        std::cout << "---------------" << std::endl;

        // compile
        std::cout << "\n--- Compiling ---" << std::endl;
        ScriptCompiler compiler;
        compiler.compile_ast(AST, "MyceliumModule");

        std::ofstream outFile("tests/build/test.ll");
        if (outFile)
        {
            outFile << compiler.get_ir_string();
            outFile.close();
        }

        compiler.compile_to_object_file("tests/build/test.o");

        std::cout << "Compilation Successful!" << std::endl;
        std::cout << "----------------" << std::endl;

        // JIT execution
        std::cout << "\n--- JIT Execution ---" << std::endl;
        auto value = compiler.jit_execute_function("main", {});
        std::cout << "Output: " << value.DoubleVal << std::endl;
        std::cout << "JIT Execution Successful!" << std::endl;
        std::cout << "---------------------" << std::endl;

    }
    catch (const std::runtime_error &e)
    {
        std::cerr << "\n*** PARSING FAILED ***" << std::endl;
        std::cerr << "Error during parsing phase: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n*** UNEXPECTED ERROR ***" << std::endl;
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }
}

int main()
{
    HotReload fileReloader({"tests/test.sp"}, [](const std::string &filePath, const std::string &newContent)
    {
        std::cout << "\n--- " << filePath << " Reloaded ---" << std::endl;
        Compile(newContent);
    });

    fileReloader.poll_changes();

    while (true)
    {
        fileReloader.poll_changes();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0; // Indicate success
}