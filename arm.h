
#define R0  0
#define R1  1
#define R2  2
#define R3  3
#define R4  4
#define R5  5
#define R6  6
#define R7  7
#define R8  8
#define R9  9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15
#define SP  R13
#define LR	R14
#define PC	R15
#define FLAGS 16
#define NUM_REGS	17

#define Z_SHIFT		0
#define V_SHIFT		1
#define C_SHIFT		2
#define N_SHIFT		3

#define Z	(1 << Z_SHIFT)
#define V   (1 << V_SHIFT)
#define C   (1 << C_SHIFT)
#define N   (1 << N_SHIFT)

#define IBITS(bit, nbits)       BITS(instr, bit, nbits)
#define IBIT(bit)               BIT(instr, bit)

reg arm_get_reg(int reg_num);
void arm_set_reg(int reg_num, reg val);
void arm_dump_registers(void);

extern char *regs[];

typedef reg arm_cond_t;

typedef enum { 
    ARM_INSTR_ILLEGAL,
    ARM_INSTR_B,
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

reg decode_dest_addr(reg addr, reg offset, int offset_sz, int half_flag);
arm_instr_t arm_decode_instr(reg instr);
int execute_one(void);
