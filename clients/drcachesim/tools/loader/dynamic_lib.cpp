#include <string>
#include "dynamic_lib.h"

namespace dynamorio {
namespace drmemtrace {

dynamic_lib::dynamic_lib(const char *filename)
{
#ifdef UNIX
    handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
#elif WINDOWS
    handle = LoadLibrary(filename);
#endif
    if (handle == nullptr)
        throw dynamic_lib_error(error());
}

dynamic_lib::dynamic_lib(dynamic_lib &&lib)
{
    handle = lib.handle;
    lib.handle = nullptr;
}

dynamic_lib::~dynamic_lib()
{
    if (handle) {
#ifdef UNIX
        dlclose(handle);
#elif WINDOWS
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
    }
}

dynamic_lib &
dynamic_lib::operator=(dynamic_lib &&lib)
{
    if (handle) {
#ifdef UNIX
        dlclose(handle);
#elif WINDOWS
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
    }
    handle = lib.handle;
    lib.handle = nullptr;
    return *this;
}

std::string
dynamic_lib::error()
{
#ifdef UNIX
    return std::string(dlerror());
#elif WINDOWS
    return std::to_string(GetLastError());
#endif
}

} // namespace drmemtrace
} // namespace dynamorio
