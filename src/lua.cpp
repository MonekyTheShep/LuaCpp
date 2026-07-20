#include "lua.h"

#include <iostream>
#include <format>
#include <chrono>
#include <string>
#include <utility>

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
        std::cout << PrintStmtVisitor{0}.visitStmts(ast) << '\n';
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
        double opPerSecond = vm.opcount / elapsed.count();
        std::cout << std::format("Op/s: {:.14g} OpCount: {}", opPerSecond, vm.opcount) << "\n";
        std::cout << std::format("VM runtime: {} seconds", elapsed.count()) << "\n";
    }
}
