#include "vpmu.h"             // VPMU common headers
#include "vpmu-arm-insnset.h" // ARM Instruction SET
#include "vpmu-log.h"         // ERR_MSG

// Return array length if not found
ARM_Instructions get_index_of_arm_insn(const char *s)
{
#define etype(x) macro_str(x)
    // static is for putting it in global space
    static const char *str_arm_instructions[] = {ARM_INSTRUCTION};
    int                i;

    for (i = 0; i < sizeof(str_arm_instructions) / sizeof(const char *); i++) {
        if (strcmp(str_arm_instructions[i], s) == 0) return (ARM_Instructions)i;
    }

    ERR_MSG("get_index_of_arm_insn: could not find field \"%s\"\n", s);
#undef etype
    return ARM_INSTRUCTION_TOTAL_COUNTS;
}

#ifdef CONFIG_VPMU_VFP
// Return array length if not found
ARM_VFP_Instructions get_index_of_arm_vfp_insn(const char *s)
{
#define etype(x) macro_str(x)
    // static is for putting it in global space
    static const char *str_arm_vfp_instructions[] = {ARM_VFP_INSTRUCTION};
    int                i;

    for (i = 0; i < sizeof(str_arm_vfp_instructions) / sizeof(const char *); i++) {
        if (strcmp(str_arm_vfp_instructions[i], s) == 0) return (ARM_VFP_Instructions)i;
    }

    ERR_MSG("get_index_of_arm_vfp_insn: could not find field \"%s\"\n", s);
#undef etype
    return ARM_VFP_INSTRUCTION_TOTAL_COUNTS;
}
#endif
