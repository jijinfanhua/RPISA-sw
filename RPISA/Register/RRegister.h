//
// Created by root on 1/28/23.
//

#ifndef RPISA_SW_RREGISTER_H
#define RPISA_SW_RREGISTER_H

#include "../../defs.h"

/*********** fengyong add start ***************/
struct RingRegister {
    std::array<bool, PROC_NUM> dst1;
    u32 ctrl;
    std::array<bool, PROC_NUM> dst2;
    u32 addr;
    std::array<u32, 128> payload;
};

struct PIRegister {
    u32 decrease_clk, increase_clk;
    struct flow_info_in_cam {
        u32 timer_offset;
        u32 first_pkt_idx;
        u32 left_pkt;
        enum FSMState {
            RUN, WAIT, SUSPEND, READY
        } cur_state;
    };

    // hash of four ways (32bit) -> flow info
    std::map<u64, flow_info_in_cam> flow_cam;

    std::array<RingRegister, 128> rp2p;
    std::array<u32, 128> rp2p_pointer;

    queue<> wait_queue;
    u32 wait_head, wait_tail;

    queue<> schedule_queue;
    u32 schedule_head, schedule_tail;
};

/*********** fengyong add end ***************/

#endif //RPISA_SW_RREGISTER_H
