#include "ir_decode.h"
#include "main.h"

void check_ir(uint16_t period)
{
    if (!receiving) {
        if (period > LEADER_MIN && period < LEADER_MAX) {  // 리더 코드 감지 (데이터 수신 시작)
            receiving = 1;
            bit_cnt = 0;
            ir_data = 0;
        }
        return;
    }

    ir_data >>= 1;  // 한 칸 시프트

    // 1비트 구간이면 MSB를 1로 설정
    if (period > BIT1_MIN && period < BIT1_MAX) {
        ir_data |= 0x80000000;
    }
    // 0비트 구간도 아니고 1비트 구간도 아니면 잘못된 신호로 간주
    else if (!(period > BIT0_MIN && period < BIT0_MAX)) {
        reset_ir_state();
        return;
    }

    // 32비트 수신 완료 시
    if (++bit_cnt == 32) {
        ir_key_ready = 1;
        receiving = 0;
    }
}

void reset_ir_state(void) // 신호의 유효값을 벗어나면 해당 값들을 0으로 초기화
{
    ir_data = 0;
    bit_cnt = 0;
    receiving = 0;
    ir_key_ready = 0;
}

