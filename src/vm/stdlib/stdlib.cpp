#include "vm/stdlib/stdlib.h"

#include <array>
#include <memory>
#include <string>

#include "vm/stdlib/library.h"

#include "vm/stdlib/tablelib.h"
#include "vm/stdlib/stringlib.h"
#include "vm/stdlib/iolib.h"
#include "vm/stdlib/baselib.h"

#include "vm/types/luatable.h"
#include "vm/types/value.h"
#include "vm/vm.h"

struct Lib 
{
    std::string name;
    std::unique_ptr<Library> handle;
};

void StdLib::initLibraries(VMGlobal &vmGlobal)
{
    std::array<Lib, 4> libraries =
    {{
        {"_G", std::make_unique<BaseLib>()},
        {"table", std::make_unique<TableLib>()},
        {"string", std::make_unique<StringLib>()},
        {"io", std::make_unique<IoLib>()},
    }};

    for (const Lib &lib : libraries)
    {
       vmGlobal.globals->set(vmGlobal.main, lib.name, lib.handle->openLibrary(vmGlobal));
    }
}