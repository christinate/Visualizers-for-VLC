#pragma once
#include "../../vendor/WDL/eel2/ns-eel.h"
#define NSEEL_code_compile(ctx,code) NSEEL_code_compile(ctx,code,0)
inline void NSEEL_VM_resetvars(NSEEL_VMCTX ctx) { NSEEL_VM_remove_all_nonreg_vars(ctx); NSEEL_VM_enumallvars(ctx, [](const char*, EEL_F* value, void*) { *value=0.0; return 1; }, nullptr); NSEEL_VM_freeRAM(ctx); }
