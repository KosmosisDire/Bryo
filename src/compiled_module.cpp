#include "compiled_module.hpp"
#include "jit.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <iostream>

namespace Myre
{
    static void initializeCommonTargets()
    {
        static bool initialized = false;
        if (initialized)
            return;

        // X86 (Intel/AMD - Windows, Linux)
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86Target();
        LLVMInitializeX86TargetMC();
        LLVMInitializeX86AsmPrinter();
        LLVMInitializeX86AsmParser();

        // AArch64 (ARM64 - Apple Silicon, ARM Windows/Linux)
        LLVMInitializeAArch64TargetInfo();
        LLVMInitializeAArch64Target();
        LLVMInitializeAArch64TargetMC();
        LLVMInitializeAArch64AsmPrinter();
        LLVMInitializeAArch64AsmParser();

        initialized = true;
    }

    bool CompiledModule::write_ir(const std::string &filename) const
    {
        if (!is_valid())
        {
            std::cerr << "Cannot write IR: module is invalid\n";
            return false;
        }

        std::error_code EC;
        llvm::raw_fd_ostream output(filename, EC, llvm::sys::fs::OF_None);

        if (EC)
        {
            std::cerr << "Could not open file " << filename << ": " << EC.message() << "\n";
            return false;
        }

        module->print(output, nullptr);
        return true;
    }

    std::string CompiledModule::get_ir_string() const
    {
        if (!is_valid())
            return "";

        std::string ir_str;
        llvm::raw_string_ostream stream(ir_str);
        module->print(stream, nullptr);
        return stream.str();
    }

    void CompiledModule::dump_ir() const
    {
        if (!is_valid())
        {
            std::cerr << "Cannot dump IR: module is invalid\n";
            return;
        }

        std::cout << "\n=== LLVM IR ===\n";
        module->print(llvm::outs(), nullptr);
        std::cout << "\n===============\n";
    }

    bool CompiledModule::write_object_file(const std::string &filename) const
    {
        if (!is_valid())
        {
            std::cerr << "Cannot generate object file: module is invalid\n";
            return false;
        }

        initializeCommonTargets();

        // Clone module since we need to modify it
        auto cloned_module = llvm::CloneModule(*module);

        // Get target triple
        auto target_triple = llvm::sys::getDefaultTargetTriple();
        cloned_module->setTargetTriple(target_triple);

        // Get target
        std::string error;
        auto target = llvm::TargetRegistry::lookupTarget(target_triple, error);

        if (!target)
        {
            std::cerr << "Target lookup failed: " << error << "\n";
            return false;
        }

        // Create target machine
        auto CPU = "generic";
        auto features = "";
        llvm::TargetOptions opt;
        auto RM = std::optional<llvm::Reloc::Model>();
        auto target_machine = target->createTargetMachine(
            target_triple, CPU, features, opt, RM);

        cloned_module->setDataLayout(target_machine->createDataLayout());

        // Open output file
        std::error_code EC;
        llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);

        if (EC)
        {
            std::cerr << "Could not open file: " << EC.message() << "\n";
            return false;
        }

        // Generate object code
        llvm::legacy::PassManager pass;
        auto file_type = llvm::CodeGenFileType::ObjectFile;

        if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type))
        {
            std::cerr << "Target machine can't emit object file\n";
            return false;
        }

        pass.run(*cloned_module);
        dest.flush();

        return true;
    }

    bool CompiledModule::write_assembly(const std::string &filename) const
    {
        if (!is_valid())
        {
            std::cerr << "Cannot generate assembly: module is invalid\n";
            return false;
        }

        initializeCommonTargets();

        auto cloned_module = llvm::CloneModule(*module);
        auto target_triple = llvm::sys::getDefaultTargetTriple();
        cloned_module->setTargetTriple(target_triple);

        std::string error;
        auto target = llvm::TargetRegistry::lookupTarget(target_triple, error);
        if (!target)
        {
            std::cerr << "Target lookup failed: " << error << "\n";
            return false;
        }

        auto CPU = "generic";
        auto features = "";
        llvm::TargetOptions opt;
        auto RM = std::optional<llvm::Reloc::Model>();
        auto target_machine = target->createTargetMachine(
            target_triple, CPU, features, opt, RM);

        cloned_module->setDataLayout(target_machine->createDataLayout());

        std::error_code EC;
        llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
        if (EC)
        {
            std::cerr << "Could not open file: " << EC.message() << "\n";
            return false;
        }

        llvm::legacy::PassManager pass;
        auto file_type = llvm::CodeGenFileType::AssemblyFile;

        if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type))
        {
            std::cerr << "Target machine can't emit assembly file\n";
            return false;
        }

        pass.run(*cloned_module);
        dest.flush();

        return true;
    }

    int CompiledModule::execute_jit()
    {
        if (!is_valid())
        {
            std::cerr << "Cannot execute: module is invalid\n";
            for (const auto &error : errors)
            {
                std::cerr << "  - " << error << "\n";
            }
            return -1;
        }

        // Verify module before JIT execution
        std::string verify_error;
        llvm::raw_string_ostream error_stream(verify_error);
        if (llvm::verifyModule(*module, &error_stream))
        {
            std::cerr << "Module verification failed:\n"
                      << verify_error << "\n";
            return -1;
        }

        JIT jit;

        // Move ownership to JIT (this will invalidate our module/context pointers)
        if (!jit.add_module(std::move(module), std::move(context)))
        {
            std::cerr << "Failed to add module to JIT\n";
            return -1;
        }

        // Note: After this point, module and context are nullptr
        return jit.run_main();
    }

} // namespace Myre