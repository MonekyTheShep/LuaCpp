#include "vm/stdlib/stdlib.h"

#include <array>
#include <memory>

#include "vm/stdlib/library.h"

#include "vm/stdlib/tablelib.h"
#include "vm/stdlib/stringlib.h"
#include "vm/stdlib/iolib.h"
#include "vm/stdlib/baselib.h"

#include "vm/types/value.h"
#include "vm/vm.h"

std::array<StdLib::Lib, 4> StdLib::libraries =
{{
    {"_G", std::make_unique<BaseLib>()},
    {"table", std::make_unique<TableLib>()},
    {"string", std::make_unique<StringLib>()},
    {"io", std::make_unique<IoLib>()},
}};

void StdLib::initLibraries(VM &vm)
{
    for (const Lib &lib : libraries)
    {
       vm.globals->set(vm, lib.name, lib.handle->openLibrary(vm));
    }
}
