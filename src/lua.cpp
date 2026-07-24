#include "lua.h"

#include <cstddef>
#include <iostream>
#include <format>
#include <chrono>
#include <string>
#include <utility>

#include "bytecode.h"
#include "parser.h"
#include "compiler.h"
#include "value.h"
#include "vm.h"

#include "ast.h"
#include "disassembler.h"

void Lua::run(std::string code)
{
    Ast ast = Parser(std::move(code)).parse();

    FunctionHandle function = Compiler::makeTopLevel().compile(ast);
    
    vm.execute(function);
}

void Lua::debugRun(std::string code)
{
    Ast ast = Parser(std::move(code)).parse();
    {
        std::cout << "========= AST DEBUGGING ==========" << "\n";
        std::cout << AstPrinter{}.visitStmts(ast) << '\n';
    }

    FunctionHandle function = Compiler::makeTopLevel().compile(ast);
    {
        std::cout << "========= BYTECODE DEBUGGING ==========" << "\n";
        Disassembler().disassemble(function->chunk);
    }

    using Time = std::chrono::high_resolution_clock;
    using Seconds = std::chrono::duration<double>;

    {
        std::cout << "========= VM OUTPUT ==========" << "\n";
    }
    
    auto begin = Time::now();
    vm.execute(function);
    auto end = Time::now();
    {
        std::cout << "========= VM PERFORMANCE  ==========" << "\n";
        Seconds elapsed = end - begin;

        size_t totalOpCount = 0;

        for (size_t i = 0; i < vm.opCounts.size(); i++)
        {
            size_t opCount = vm.opCounts[i];
            if (opCount == 0) continue;
            std::string name = ByteCode::toString(Op(static_cast<int>(i)));

            std::cout << std::format("{} : {}", name , opCount) << "\n";
            totalOpCount += opCount;
        }

        double opPerSecond = static_cast<double>(totalOpCount) / elapsed.count();
        std::cout << std::format("Op/s: {:.14g} OpCount: {}", opPerSecond, totalOpCount) << "\n";
        std::cout << std::format("VM runtime: {:.9f} seconds", elapsed.count()) << "\n";
    }
}
