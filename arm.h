
#define SP  13
#define LR	14
#define PC	15
#define FLAGS 16
#define NUM_REGS	17

reg arm_reg(int reg_num);
void set_reg(int reg_num, reg val);

typedef reg arm_cond_t;

typedef enum { 
    ARM_INSTR_ILLEGAL,
    ARM_INSTR_B,
    ARM_INSTR_BL,
    ARM_INSTR_BX_RM,
    ARM_INSTR_BLX_RM,
    ARM_INSTR_BLX,     // condition = 0xF
    ARM_INSTR_SWI,

    ARM_INSTR_AND,     // Beginining of logic ops
    ARM_INSTR_EOR,
    ARM_INSTR_SUB,
    ARM_INSTR_RSB,
    ARM_INSTR_ADD,
    ARM_INSTR_ADC,
    ARM_INSTR_SBC,
    ARM_INSTR_RSC,
    ARM_INSTR_TST,
    ARM_INSTR_TEQ,
    ARM_INSTR_CMP,
    ARM_INSTR_CMN,
    ARM_INSTR_ORR,
    ARM_INSTR_MOV,
    ARM_INSTR_BIC,
    ARM_INSTR_MVN,

    ARM_INSTR_MUL,    // Beginning of multiply ops
    ARM_INSTR_MLA,
    ARM_INSTR_UMULL,
    ARM_INSTR_UMLAL,
    ARM_INSTR_SMULL,
    ARM_INSTR_SMLAL,

    ARM_INSTR_STR,
    ARM_INSTR_LDR,
    ARM_INSTR_STB,
    ARM_INSTR_LDB,
    ARM_INSTR_LDSH,
    ARM_INSTR_LDSB,
    ARM_INSTR_LDUH,
    ARM_INSTR_STH,
    ARM_INSTR_STM,
    ARM_INSTR_LDM,
} arm_instr_t;

arm_instr_t arm_decode_instr(reg instr);
