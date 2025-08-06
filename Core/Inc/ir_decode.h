#ifndef IR_DECODE_H_
#define IR_DECODE_H_

#include <stdint.h>
#include "main.h"

void check_ir(uint16_t period);
void reset_ir_state(void);

#endif /* IR_DECODE_H_ */
