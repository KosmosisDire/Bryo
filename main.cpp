#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include "hot_reload.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>

using namespace Mycelium::UI::Lang;

void parse(std::string input)
{

    std::cout << "--- Input ---" << std::endl;
    std::cout << input << std::endl;
    std::cout << "-------------" << std::endl;

    std::vector<Token> tokens;
    std::unique_ptr<ProgramNode> astRoot;

    try
    {
        // Lexing
        std::cout << "\n--- Lexing ---" << std::endl;
        Lexer lexer(input);
        tokens = lexer.tokenizeAll();
        for (const auto &token : tokens)
        {
            std::cout << "Token - \"" << token.text << "\"" << std::endl;
        }
        std::cout << "--------------" << std::endl;

        // Parsing
        std::cout << "\n--- Parsing ---" << std::endl;
        Parser parser(tokens);
        astRoot = parser.parseProgram();
        std::cout << "Parsing Successful!" << std::endl;
        std::cout << "---------------" << std::endl;

        // Print AST
        std::cout << "\n--- AST ---" << std::endl;
        if (astRoot)
        {
            astRoot->print();
        }
        else
        {
            std::cout << "AST is null (this shouldn't happen if parsing didn't throw)." << std::endl;
        }
        std::cout << "-----------" << std::endl;
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
    HotReload fileReloader({"tests/test.mui"}, [](const std::string &filePath, const std::string &newContent)
                           {
                               std::cout << "\n--- " << filePath << " Reloaded ---" << std::endl;
                               parse(newContent); // Call the parse function with the new content
                           });

    fileReloader.poll_changes();

    while (true)
    {
        fileReloader.poll_changes();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0; // Indicate success
}