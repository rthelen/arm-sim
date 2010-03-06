
#define SP  13
#define LR	14
#define PC	15
#define FLAGS 16
#define NUM_REGS	17

reg arm_reg(int reg_num);
void set_reg(int reg_num, reg val);
