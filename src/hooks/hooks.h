#include <MinHook.h>

#include "DynamicLC/DynamicLC.h"

namespace Hooks {
    void Install() { 
        MH_Initialize();

        DynamicLC::Install();

        MH_EnableHook(MH_ALL_HOOKS);
        return;
    }

    void InstallLate() {

        

    }
}