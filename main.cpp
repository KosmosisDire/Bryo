#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp" // For ProgramNode and printing
#include "hot_reload.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>

void parse(std::string input)
{

    std::cout << "--- Input ---" << std::endl;
    std::cout << input << std::endl;
    std::cout << "-------------" << std::endl;

    std::vector<Mycelium::UI::Lang::Token> tokens;
    std::unique_ptr<Mycelium::UI::Lang::ProgramNode> astRoot;

    try
    {
        // 1. Lexing
        std::cout << "\n--- Lexing ---" << std::endl;
        Mycelium::UI::Lang::Lexer lexer(input);
        tokens = lexer.tokenizeAll(); // tokenizeAll handles its own warnings now
        for (const auto &token : tokens)
        {
            std::cout << "Token { Type: " << Mycelium::UI::Lang::tokenTypeToString(token.type)
                      << ", Text: \"" << token.text << "\" }" << std::endl;
        }
        std::cout << "--------------" << std::endl;

        // 2. Parsing
        std::cout << "\n--- Parsing ---" << std::endl;
        Mycelium::UI::Lang::Parser parser(tokens);
        astRoot = parser.parseProgram(); // Can throw std::runtime_error
        std::cout << "Parsing Successful!" << std::endl;
        std::cout << "---------------" << std::endl;

        // 3. Print AST (only if parsing succeeded)
        std::cout << "\n--- AST ---" << std::endl;
        if (astRoot)
        { // Should always be true if no exception was thrown
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
        // Catch errors from Parser (Lexer errors are handled as warnings for now)
        std::cerr << "\n*** PARSING FAILED ***" << std::endl;
        // Parser::parseProgram already prints details, but we add context
        std::cerr << "Error during parsing phase: " << e.what() << std::endl;
        return; // Indicate failure
    }
    catch (const std::exception &e)
    {
        // Catch any other standard exceptions
        std::cerr << "\n*** UNEXPECTED ERROR ***" << std::endl;
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return;
    }
}

int main()
{
    HotReload fileReloader({"test.mui"}, [](const std::string &filePath, const std::string &newContent)
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