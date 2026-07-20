#include "value.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "bytecode.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::optional<int> Chunk::makeConstant(Value value)
{
    int index = static_cast<int>(constants.size());
    if (index > UINT8_MAX) return std::nullopt;
    
    if (auto *string = std::get_if<std::string>(&value))
    {
        auto it = strings.find(*string);
        if (it != strings.end()) return it->second;
    }
    
    constants.emplace_back(std::move(value));

    if (auto *string = std::get_if<std::string>(&constants.back()))
    {
        strings[*string] = index;
    }
   
    return index;
}

void Chunk::write(uint8_t arg, int line) 
{
    code.emplace_back(arg);
    lines.emplace_back(line);
}

void Chunk::write(Op op, int line)
{
    write(static_cast<uint8_t>(op), line);
}

std::optional<double> ValueHelper::toNumber(const Value &value)
{
    return std::visit(overloaded 
    {
        [](const double &num) -> std::optional<double> 
        { 
            return num;
        },
        [](const std::string &string) -> std::optional<double>  
        {
            char *end;
            double value = std::strtod(string.c_str(), &end);
            if (end != string.c_str() + string.size()) return std::nullopt;
             return value;
        },
        [](const auto&) -> std::optional<double> 
        {
            return std::nullopt;
        }
    }, value);
}

std::optional<std::string> ValueHelper::toString(const Value &value)
{
    return std::visit(overloaded 
    {
        [](const double &num) -> std::optional<std::string> 
        { 
            return std::format("{:.14g}", num);
        },
        [](const std::string &string) -> std::optional<std::string>  
        {
            return string;
        },
        [](const auto &) -> std::optional<std::string> 
        {
            return std::nullopt;
        }
    }, value);
}
