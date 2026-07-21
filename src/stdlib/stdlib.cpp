#include "stdlib/stdlib.h"

#include <array>
#include <memory>

#include "stdlib/library.h"

#include "stdlib/tablelib.h"
#include "stdlib/stringlib.h"
#include "stdlib/iolib.h"
#include "stdlib/baselib.h"

#include "value.h"
#include "vm.h"

std::array<Lib, 4> StdLib::libraries =
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
