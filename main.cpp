#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <optional>


#include "ui_lexer.hpp"
#include "ui_parser.hpp"
#include "ui_ast.hpp"
#include "script_tokenizer.hpp"
#include "script_ast.hpp"
#include "script_parser.hpp"
#include "hot_reload.hpp"
#include "platform.h"

#include "libtcc.h"

// using namespace Mycelium::UI::Lang;
using namespace Mycelium::Scripting::Lang;



// Helper structure to hold error information for the TCC callback
struct TccErrorReporter {
    std::string last_error_message;
    bool error_occurred = false;

    static void tcc_error_callback(void *opaque, const char *msg) {
        TccErrorReporter *reporter = static_cast<TccErrorReporter *>(opaque);
        if (reporter) {
            reporter->last_error_message = msg;
            reporter->error_occurred = true;
            std::cerr << "Compilation Error: " << msg << std::endl;
        }
    }
};
 
/**
 * @brief Compiles a C code string and executes a specified function from it.
 *
 * @param c_code_string The C code to compile.
 * @param entry_function_name The name of the function within the C code to execute.
 *                            This function should have the signature: int function_name(void).
 * @return std::optional<int> The integer result of the executed function if successful,
 *                            std::nullopt otherwise. Error messages are printed to std::cerr.
 */
std::optional<int> compileAndExecuteCString(
    const std::string& c_code_string,
    const std::string& entry_function_name = "main") // Default to "main"
{
    TCCState *s = nullptr;
    TccErrorReporter error_reporter;

    s = tcc_new();
    if (!s) {
        std::cerr << "Error: Could not create TCC state." << std::endl;
        return std::nullopt;
    }
    
    tcc_set_error_func(s, &error_reporter, TccErrorReporter::tcc_error_callback);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);


    std::string execDir = getExecutableDir();
    if (!execDir.empty())
    {
        tcc_add_include_path(s, (execDir + "/include").c_str());
        tcc_add_include_path(s, (execDir + "/lib").c_str());
    }
    else
    {
        std::cerr << "Warning: Could not determine executable directory. Include paths might be incorrect." << std::endl;
    }

    // Compile the C code string
    try{
    if (tcc_compile_string(s, c_code_string.c_str()) == -1) {
        std::cerr << "TCC compilation failed: " << error_reporter.last_error_message << std::endl;
        tcc_delete(s);
        return std::nullopt;
    }
    } catch (const std::exception& e) {
        std::cerr << "Error during TCC compilation: " << e.what() << std::endl;
        tcc_delete(s);
        return std::nullopt;
    }
    catch (...) {
        std::cerr << "Unknown error during TCC compilation." << std::endl;
        tcc_delete(s);
        return std::nullopt;
    }

    int code_size = tcc_relocate(s, TCC_RELOCATE_AUTO);
    if (code_size == -1) {
        std::cerr << "TCC relocation failed: " << error_reporter.last_error_message << std::endl;
        tcc_delete(s);
        return std::nullopt;
    }

    typedef int (*EntryPointFuncType)(void);
    EntryPointFuncType entry_func_ptr;

    entry_func_ptr = (EntryPointFuncType)tcc_get_symbol(s, entry_function_name.c_str());

    if (!entry_func_ptr) {
        std::cerr << "Error: Could not find symbol '" << entry_function_name << "' in compiled C code." << std::endl;
        if (error_reporter.error_occurred && !error_reporter.last_error_message.empty()) {
             std::cerr << " (TCC reported: " << error_reporter.last_error_message << ")" << std::endl;
        }
        tcc_delete(s);
        return std::nullopt;
    }

    // Execute the function
    int result = 0;
    try
    {
        result = entry_func_ptr();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: '" << entry_function_name << "': " << e.what() << std::endl;
        tcc_delete(s);
        return std::nullopt;
    } 
    catch (...)
    {
        std::cerr << "Error: Unknown exception '" << entry_function_name << "'." << std::endl;
        tcc_delete(s);
        return std::nullopt;
    }

    // Clean up TCC state
    tcc_delete(s);

    return result;
}



// std::string MUItoC(std::string input)
// {

//     std::cout << "--- Input ---" << std::endl;
//     std::cout << input << std::endl;
//     std::cout << "-------------" << std::endl;

//     std::vector<Token> tokens;
//     std::shared_ptr<ProgramNode> astRoot;

//     try
//     {
//         // Lexing
//         std::cout << "\n--- Lexing ---" << std::endl;
//         Lexer lexer(input);
//         tokens = lexer.tokenizeAll();
//         for (const auto &token : tokens)
//         {
//             std::cout << "Token - \"" << token.text << "\"" << std::endl;
//         }
//         std::cout << "--------------" << std::endl;

//         // Parsing
//         std::cout << "\n--- Parsing ---" << std::endl;
//         Parser parser(tokens);
//         astRoot = parser.parseProgram();
//         std::cout << "Parsing Successful!" << std::endl;
//         std::cout << "---------------" << std::endl;

//         // Print AST
//         std::cout << "\n--- AST ---" << std::endl;
//         if (astRoot)
//         {
//             return astRoot->toC();
//         }
//         else
//         {
//             std::cout << "AST is null (this shouldn't happen if parsing didn't throw)." << std::endl;
//         }
//         std::cout << "-----------" << std::endl;
//     }
//     catch (const std::runtime_error &e)
//     {
//         std::cerr << "\n*** PARSING FAILED ***" << std::endl;
//         std::cerr << "Error during parsing phase: " << e.what() << std::endl;
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "\n*** UNEXPECTED ERROR ***" << std::endl;
//         std::cerr << "Caught exception: " << e.what() << std::endl;
//     }

//     return std::string();
// }

std::string SharpieToC(std::string input)
{
    std::cout << "--- Input ---" << std::endl;
    std::cout << input << std::endl;
    std::cout << "-------------" << std::endl;

    std::vector<Token> tokens;
    std::shared_ptr<CompilationUnitNode> astRoot;

    try
    {
        // Tokenize
        std::cout << "\n--- Tokenizing ---" << std::endl;
        ScriptTokenizer tokenizer(input);
        tokens = tokenizer.tokenize_source();
        for (const auto &token : tokens)
        {
            std::cout << "Token - \"" << token.lexeme << "\"" << std::endl;
        }
        std::cout << "--------------" << std::endl;

        // Parsing
        std::cout << "\n--- Parsing ---" << std::endl;
        ScriptParser parser(tokens);
        astRoot = parser.parse_compilation_unit();
        std::cout << "Parsing Successful!" << std::endl;
        std::cout << "---------------" << std::endl;

        astRoot->print(std::cout); // Print the AST with indentation

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

    return std::string();
}

int main()
{
    // HotReload fileReloader({"tests/test.mui"}, [](const std::string &filePath, const std::string &newContent)
    // {
    //     std::cout << "\n--- " << filePath << " Reloaded ---" << std::endl;
    //     auto c = MUItoC(newContent);
    //     c = "#include \"mui_lib.c\"\n" + c; // Add necessary includes for TCC
    //     std::cout << c << std::endl;
    //     compileAndExecuteCString(c, "main"); 
    // });

    // fileReloader.poll_changes();

    // while (true)
    // {
    //     fileReloader.poll_changes();
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // }

    HotReload fileReloader({"tests/test.sp"}, [](const std::string &filePath, const std::string &newContent)
    {
        std::cout << "\n--- " << filePath << " Reloaded ---" << std::endl;
        SharpieToC(newContent);
    });

    fileReloader.poll_changes();

    while (true)
    {
        fileReloader.poll_changes();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0; // Indicate success
}