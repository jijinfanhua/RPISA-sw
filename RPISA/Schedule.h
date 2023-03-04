//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_SCHEDULE_H
#define RPISA_SW_SCHEDULE_H

#include "../pipeline.h"

// 所有对 dirty cam 的修改都需要同时修改 wait queue 和 schedule queue!!!
void update_wait_queue(u64 flow_id, flow_info_in_cam flow_info, ProcessorState& next_proc){
    if(next_proc.wait_queue_head.flow_addr == flow_id){
        next_proc.wait_queue_head = flow_info;
    }

    int s = next_proc.wait_queue.size();
    for(int i = 0; i < s; i++){
        auto item = next_proc.wait_queue.front();
        if(item.flow_addr == flow_id){
            next_proc.wait_queue.push(flow_info);
            next_proc.wait_queue.pop();
        }
        else{
            next_proc.wait_queue.push(item);
            next_proc.wait_queue.pop();
        }
    }

    int ss = next_proc.schedule_queue.size();
    for(int i = 0; i < ss; i++){
        auto item = next_proc.schedule_queue.front();
        if(item.flow_addr == flow_id){
            next_proc.schedule_queue.push(flow_info);
            next_proc.schedule_queue.pop();
        }
        else{
            next_proc.schedule_queue.push(item);
            next_proc.schedule_queue.pop();
        }
    }
}

u64 get_flow_id(PHV phv)
{
    // done: fetch flow id from two particular positions of phv
    return u32_to_u64(phv[flow_id_in_phv[0]], phv[flow_id_in_phv[1]]);
}

std::array<u64, 4> get_write_addr(PHV phv, u32 read_proc_id)
{
    std::array<u64, 4> output;
    for (int i = 0; i < num_of_stateful_tables[read_proc_id]; i++)
    {
        output[i] = u32_to_u64(phv[phv_id_to_save_hash_value[read_proc_id][i][0]], phv[phv_id_to_save_hash_value[read_proc_id][i][1]]);
    }
    return output;
}

PHV transfer_from_payload_to_phv(std::array<u32, 128> payload)
{
    PHV phv;
    // 64 * 8 | 96 * 16 | 64 * 32
    for (int i = 0; i < 16; i++)
    {
        phv[i * 4] = payload[i] >> 24;
        phv[i * 4 + 1] = payload[i] << 8 >> 24;
        phv[i * 4 + 2] = payload[i] << 16 >> 24;
        phv[i * 4 + 3] = payload[i] << 24 >> 24;
    }

    for (int i = 0; i < 48; i++)
    {
        phv[64 + i * 2] = payload[16 + i] >> 16;
        phv[64 + i * 2 + 1] = payload[16 + i] << 16 >> 16;
    }

    for (int i = 0; i < 64; i++)
    {
        phv[160 + i] = payload[64 + i];
    }
    return phv;
}

std::array<u32, 128> transfer_from_phv_to_payload(PHV phv)
{
    std::array<u32, 128> payload{};
    // 8bit * 64 ->16
    for (int i = 0; i < 16; i++)
    {
        payload[i] = (phv[i * 4] << 24) + (phv[i * 4 + 1] << 16) + (phv[i * 4 + 2] << 8) + phv[i * 4 + 3];
    }
    // 16bit * 96 -> 48
    for (int i = 16; i < 64; i++)
    {
        payload[i] = (phv[32 + i * 2] << 16) + phv[32 + i * 2 + 1];
    }
    // 32bit * 64 -> 32
    for (int i = 64; i < 128; i++)
    {
        payload[i] = phv[96 + i];
    }
    return payload;
}

struct VerifyStateChange : public Logic
{
    VerifyStateChange(int id) : Logic(id) {}

    // state change verification is only executed in the WRITE processor,
    // so the state_changed variable will not be passed to next processor
    void execute(const PipeLine *now, PipeLine *next) override
    {
        const VerifyStateChangeRegister &verifyReg = now->processors[processor_id].verifyState;
        PIWRegister &piwReg = next->processors[processor_id].piw[0];

        piwReg.enable1 = verifyReg.enable1;
        if (!verifyReg.enable1)
        {
            return;
        }

        auto state_saved = state_saved_idxs[processor_id];
        for (int i = 0; i < state_saved.state_num; i++)
        {
            if (verifyReg.phv_changed_tags[state_saved.saved_state_idx_in_phv[i]])
            {
                piwReg.state_changed = true;
            }
        }

        piwReg.phv = verifyReg.phv;
        piwReg.match_table_guider = verifyReg.match_table_guider;
        piwReg.gateway_guider = verifyReg.gateway_guider;

        piwReg.hash_values = verifyReg.hash_values;
    }
};

// after action and before PIW, there should be one cycle to determine whether states are changed
struct PIW : public Logic
{
    PIW(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const PIWRegister &first_piwReg = now->processors[processor_id].piw[0];

        // last processor & not write processor
        if (processor_id == PROCESSOR_NUM - 1 && proc_types[processor_id] != ProcType::WRITE)
        {
            // empty cycle; one cycle
            piw_empty_cycle();
            return;
        }
        else if (processor_id != PROCESSOR_NUM - 1 && proc_types[processor_id] != ProcType::WRITE)
        {
            // not the last -> only one cycle
            if (first_piwReg.enable1)
            {
                GetKeysRegister &getKeysReg = next->processors[processor_id + 1].getKeys;
                getKeysReg.enable1 = first_piwReg.enable1;
                getKeysReg.match_table_guider = first_piwReg.match_table_guider;
                getKeysReg.gateway_guider = first_piwReg.gateway_guider;
                getKeysReg.phv = first_piwReg.phv;
            }
            else
            {
            }
        }
        else if (proc_types[processor_id] == ProcType::WRITE)
        {
            PIWRegister &second_piwReg = next->processors[processor_id].piw[1];
            const ProcessorState &now_proc = now->proc_states[processor_id];
            ProcessorState &next_proc = next->proc_states[processor_id]; // to send pkt/hb/write

            piw_first_cycle(first_piwReg, second_piwReg, now_proc, next_proc);

            const PIWRegister &cur_second_piwReg = now->processors[processor_id].piw[1];

            // second cycle
            if (processor_id != PROCESSOR_NUM - 1)
            {
                BaseRegister &baseReg = next->processors[processor_id + 1].base;
                handle_piw_write(cur_second_piwReg, now_proc, next_proc);
                if (!cur_second_piwReg.pkt_backward && cur_second_piwReg.enable1)
                {
                    baseReg.enable1 = cur_second_piwReg.enable1;
                    baseReg.match_table_guider = cur_second_piwReg.match_table_guider;
                    baseReg.gateway_guider = cur_second_piwReg.gateway_guider;
                    baseReg.phv = cur_second_piwReg.phv;
                }
            }
            else
            {
                handle_piw_write(cur_second_piwReg, now_proc, next_proc);
            }
        }
        else
        {
            // nothing
        }

        // Asyn Code: receive cancel_dirty
    }

    void piw_first_cycle(const PIWRegister &now, PIWRegister &next, const ProcessorState &now_proc,
                         ProcessorState &next_proc)
    {
        // handle asyn RI's input -> cancel_dirty
        if (now.cd_come)
        {
            next_proc.dirty_cam.erase(now.cd_addr);
        }

        // search the dirty table in PIW using hash addr
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }

        next_proc.total_packets += 1;

        // in PIW, value in dirty cam is not important.
        auto flow_id = get_flow_id(now.phv);
        auto flow_cam = now_proc.dirty_cam;
        if (flow_cam.find(flow_id) == flow_cam.end())
        {
            // if need to writeback: stateless / stateful
            if (now.state_changed)
            { // state changed; state is in PHV
                // add item into dirty cam
                next_proc.dirty_cam.insert(std::pair<u64, flow_info_in_cam>(flow_id, flow_info_in_cam{}));
                next.state_writeback = true;
                next.pkt_backward = false;
                // for statistics
                next_proc.wb_packets += 1;
            }
            else
            { // state not changed / not stateful pkt
                next.state_writeback = false;
                next.pkt_backward = false;
            }
        }
        else
        {
            if (now.cd_come && now.cd_addr == flow_id)
            {
                next.state_writeback = false;
                next.pkt_backward = false;
            }
            else
            {
                next.state_writeback = false;
                next.pkt_backward = true;
                // statistics
                next_proc.bp_packets += 1;
            }
        }
        next.phv = now.phv;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.hash_values = now.hash_values;
    }

    void handle_piw_write(const PIWRegister &now, const ProcessorState &now_proc, ProcessorState &next_proc)
    {
        // send heartbeat / asyn
        RP2R_REG it{};
        it.rr.ctrl = 0b11;
        it.rr.dst1 = 1 << (PROCESSOR_NUM - read_proc_ids[processor_id] - 1);

        if (!now.enable1)
        {
            next_proc.p2r.push(it);
            return;
        }

        // send pkt / write
        if (now.state_writeback)
        {
            // build ringreg
            it.rr.ctrl = 0b00;
            it.rr.dst2 = it.rr.dst1;
            it.rr.write_addr = get_write_addr(now.phv, read_proc_ids[processor_id]);
            it.rr.flow_addr = get_flow_id(now.phv);
            for (int i = 0; i < 16; i++)
            {
                int tmp_idx = state_idx_in_phv[processor_id][i];
                if (tmp_idx < 0)
                {
                    break;
                }
                // todo: merge fields with different lengths;
                if (tmp_idx < 64)
                { // U8
                    it.rr.payload[i] = now.phv[tmp_idx];
                }
                else if (tmp_idx < 160)
                { // u16
                    it.rr.payload[i] = now.phv[tmp_idx];
                }
                else
                {
                    it.rr.payload[i] = now.phv[tmp_idx];
                }
            }
            next_proc.p2r.push(it);
        }
        else if (now.pkt_backward)
        {
            it.rr.ctrl = 0b01;
            it.rr.dst2 = it.rr.dst1;
            it.rr.flow_addr = get_flow_id(now.phv);
            it.rr.write_addr = get_write_addr(now.phv, read_proc_ids[processor_id]);
            it.rr.payload = transfer_from_phv_to_payload(now.phv); // todo: transform from phv(32bit*224) to payload(32bit * 128)
            it.gateway_guider = now.gateway_guider;
            it.match_table_guider = now.match_table_guider;
            next_proc.p2r.push(it);
        }
        else
        {
            // just to Base of next proc
        }
    }

    void piw_empty_cycle()
    {
    }
};

// 查dirty表，查到则将包信息放到p2p中，查不到则空转周期
struct PIR : public Logic {

    PIR(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override {
        const PIRegister &piReg = now->processors[processor_id].pi;
        PORegister &poReg = next->processors[processor_id].po;

        const ProcessorState &now_proc = now->proc_states[processor_id]; // can only be read
        ProcessorState &next_proc = next->proc_states[processor_id];     // can only be written

        handle_pir_cycle(piReg, now_proc, next_proc, poReg);

//        pir_pipe_first_cycle(piReg, now_proc, next_proc, nextPIReg);
//        pir_pipe_second_cycle(old_nextPIReg, now_proc, next_proc, poReg);
    }

    void handle_pir_cycle(const PIRegister &now, const ProcessorState &now_proc, ProcessorState &next_proc,
                          PORegister &next) {
        next.enable1 = now.enable1;
        if (!now.enable1) {
            next_proc.normal_pipe_schedule_flag = false;
            return;
        }
        auto flow_id = get_flow_id(now.phv);
        auto flow_cam = now_proc.dirty_cam;
        if (flow_cam.find(flow_id) == flow_cam.end()) {
            // normal pipe schedule for po
            next_proc.normal_pipe_schedule_flag = true;
            next.phv = now.phv;
            next.match_table_guider = now.match_table_guider;
            next.gateway_guider = now.gateway_guider;
            next.hash_values = now.hash_values;
            next.match_table_keys = now.match_table_keys;
        } else {
            // need to block
            // no normal pipe for po
            next_proc.normal_pipe_schedule_flag = false;
            auto &next_flow_info = next_proc.dirty_cam[flow_id];

            // rp2p tail & other paras may have changed, here directly using next_proc's
            if (next_flow_info.p2p_first_pkt_idx == -1) { // only write, no pkt come/ or queue only have backward pkt
                next_flow_info.p2p_first_pkt_idx = next_flow_info.p2p_last_pkt_idx = next_proc.rp2p_tail;
                next_proc.rp2p[next_proc.rp2p_tail] =
                        {now.phv, now.match_table_guider, now.gateway_guider,
                         now.hash_values, now.match_table_keys, false};
                next_flow_info.left_pkt_num = next_flow_info.left_pkt_num + 1;
                next_proc.rp2p_pointer[next_flow_info.p2p_last_pkt_idx] = -1;
            } else { // have buffered pkt -> SUSPEND/READY
                next_proc.rp2p[next_proc.rp2p_tail] =
                        {now.phv, now.match_table_guider, now.gateway_guider,
                         now.hash_values, now.match_table_keys, false};
                next_flow_info.left_pkt_num = next_flow_info.left_pkt_num + 1;
                next_proc.rp2p_pointer[next_flow_info.p2p_last_pkt_idx] = next_proc.rp2p_tail;
                next_flow_info.p2p_last_pkt_idx = next_proc.rp2p_tail;
            }
            // state transition
            if (next_flow_info.cur_state == flow_info_in_cam::FSMState::READY) {
                next_flow_info.cur_state = flow_info_in_cam::FSMState::READY;
            } else {
                next_flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;
            }

            update_wait_queue(flow_id, next_flow_info, next_proc);

            // update global tail
            next_proc.rp2p_tail = (next_proc.rp2p_tail + 1) % 128;
        }
    }
};

// 调度正常流水线数据包/rp2p中数据包
struct PO : public Logic
{
    PO(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const PORegister &poReg = now->processors[processor_id].po;
        GetAddressRegister &getAddrReg = next->processors[processor_id].getAddress;

        const ProcessorState &now_proc = now->proc_states[processor_id];
        ProcessorState &next_proc = next->proc_states[processor_id];

        asyn_schedule(poReg, getAddrReg, now_proc, next_proc);
    }

    void asyn_schedule(const PORegister &now, GetAddressRegister &next, const ProcessorState &now_proc, ProcessorState &next_proc)
    {
        if (now_proc.normal_pipe_schedule_flag)
        {
            cout << "normal packet, do not schedule" << endl;
            // schedule the po's register
            next.enable1 = true;
            next.phv = now.phv;
            next.match_table_keys = now.match_table_keys;
            next.gateway_guider = now.gateway_guider;
            next.match_table_guider = now.match_table_guider;
            next.hash_values = now.hash_values;
            return;
        }

        if (now_proc.schedule_queue.empty())
        {
            next.enable1 = false;
            return;
        }

        flow_info_in_cam schedule_flow = next_proc.schedule_queue.front();
        if (schedule_flow.lazy)
        { // ready -> suspend
            next_proc.schedule_queue.pop();
            next.enable1 = false;
        }
        else
        {
            cout << "schedule schedule queue, before: " << next_proc.schedule_queue.size() << endl;
            next.enable1 = true;
            schedule_flow.left_pkt_num -= 1;
            // schedule the pkt to getAddr Module
            auto pkt = now_proc.rp2p[schedule_flow.p2p_first_pkt_idx];
            next.match_table_guider = pkt.match_table_guider;
            next.gateway_guider = pkt.gateway_guider;
            next.phv = pkt.phv;
            next.match_table_keys = pkt.match_table_keys;
            if (pkt.backward_pkt)
            {
                next.backward_pkt = true;
            }
            else
            {
                next.hash_values = pkt.hash_values;
            }
            schedule_flow.p2p_first_pkt_idx = now_proc.rp2p_pointer[schedule_flow.p2p_first_pkt_idx];

            update_wait_queue(schedule_flow.flow_addr, schedule_flow, next_proc);

            if (schedule_flow.left_pkt_num == 0)
            {
                next_proc.schedule_queue.pop(); // just delete, not add to the queue tail
                next_proc.dirty_cam.erase(schedule_flow.flow_addr);
            }
            else
            {
                next_proc.schedule_queue.pop();
                next_proc.schedule_queue.push(schedule_flow);
            }

            cout << "schedule schedule queue, after: " << next_proc.schedule_queue.size() << endl;
            // todo: set the rp2p to null
        }
    }
};

struct RI : public Logic
{
    RI(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        //  -----------
        // |    PIR    |    <----- r2p <------|
        //  -----------                       |
        //       |                            |
        //      p2r                           |
        //       |                            |
        //  ----------                   ----------
        // |    RO    | <--- r2r <----- |    RI    |
        //  ----------                   ----------
        const ProcessorState &now_proc = now->proc_states[processor_id];
        ProcessorState &next_proc = next->proc_states[processor_id];

        const RIRegister &riReg = now->processors[processor_id].ri;
        PIRAsynRegister &pi_asynReg = next->processors[processor_id].pi_asyn;
        RORegister &roReg = next->processors[processor_id].ro;

        // add piw
        PIWRegister &piwReg = next->processors[processor_id].piw[0];

        handle_ri_signal(riReg, pi_asynReg, piwReg, next_proc);
    }

    void handle_ri_signal(const RIRegister &riReg, PIRAsynRegister &pi_asynReg, PIWRegister &piwReg, ProcessorState &next_proc)
    {
        if (!riReg.enable1)
        {
            return;
        }
        // get the one-hot code of the processor
        u32 proc_bitmap = 1 << (PROCESSOR_NUM - processor_id - 1);

        if (proc_types[processor_id] == ProcType::WRITE)
        {
            // get cancel_dirty down
            RP2R_REG it{};
            // 当目标processor的type为write时且与dst2相同时，该信号只能是cancel dirty
            if (riReg.ringReg.dst2 == proc_bitmap)
            {
                piwReg.cd_addr = riReg.ringReg.flow_addr;
                piwReg.cd_come = true;
                // if left something, left to r2r; else: nothing
                // cancel dirty被处理，如果有剩余的heartbeat则继续向下传
                if (riReg.ringReg.dst1 != 0)
                {
                    it.rr.ctrl = 0b11;
                    it.rr.dst1 = riReg.ringReg.dst1;
                    next_proc.r2r.push(it);
                }
                else
                {
                    // nothing
                }
            }
            else
            {
                // 当目标processor的type为write时且与dst2不同时，不必做任何操作直接向下传（因为write processor不需要RI->PI(R)线路）
                // directly to r2r
                it.rr = riReg.ringReg;
                if (riReg.ringReg.ctrl == 0b01)
                {
                    it.match_table_guider = riReg.match_table_guider;
                    it.gateway_guider = riReg.gateway_guider;
                }
                next_proc.r2r.push(it);
            }
            return;
        }
        else if (proc_types[processor_id] == ProcType::NONE)
        { // just send: it will not be blocked in this processor
            RP2R_REG it{};
            it.rr = riReg.ringReg;
            it.match_table_guider = riReg.match_table_guider;
            it.gateway_guider = riReg.gateway_guider;
            next_proc.r2r.push(it);
        }
        // READ processor

        else
        {

            // if dst2 == proc_bitmap: dst1-ctrl-dst2-addr-payload to pi_asyn; left dst1 to roReg
            // else :
            // if ctrl == 0b11: get dst1 to pi_asyn, left to ro; ctrl = 0b11
            // else: get dst1 to pi_asyn, pi.ctrl = 0b11; left to ro
            int flag = 0;
            RP2R_REG it{};
            if (riReg.ringReg.dst2 == proc_bitmap)
            { // the signal dst is this proc
                pi_asynReg.enable1 = true;
                pi_asynReg.ringReg = {
                    proc_bitmap & riReg.ringReg.dst1,
                    riReg.ringReg.ctrl,
                    riReg.ringReg.dst2,
                    riReg.ringReg.write_addr,
                    riReg.ringReg.flow_addr,
                    riReg.ringReg.payload};
                flag = 1; // bp, write to pi
                if (riReg.ringReg.dst1 - proc_bitmap)
                { // other hb left, to ro
                    it.rr = {
                        riReg.ringReg.dst1 - proc_bitmap,
                        0b11,
                        0};
                    next_proc.r2r.push(it);
                }
            }
            else
            {
                // if ctrl == 0b11: get dst1 to pi_asyn, left to ro; ctrl = 0b11
                if (riReg.ringReg.ctrl == 0b11)
                {
                    if (riReg.ringReg.dst1 & proc_bitmap)
                    { // have hb to this proc
                        pi_asynReg.ringReg.dst1 = proc_bitmap;
                        pi_asynReg.ringReg.ctrl = 0b11;
                        pi_asynReg.enable1 = true;
                        // to ro's r2r
                        if (riReg.ringReg.dst1 - proc_bitmap)
                        { // have left hb to other procs
                            it.rr = {
                                riReg.ringReg.dst1 - proc_bitmap,
                                0b11,
                                0};
                            next_proc.r2r.push(it);
                        }
                    }
                    else
                    { // have hb only to other procs
                        it.rr = {
                            riReg.ringReg.dst1,
                            0b11,
                            0};
                        next_proc.r2r.push(it);
                    }
                }
                else
                { // else: get dst1 to pi_asyn, pi.ctrl = 0b11; left to ro
                    if (riReg.ringReg.dst1 & proc_bitmap)
                    { // have hb to this proc
                        pi_asynReg.ringReg.dst1 = proc_bitmap;
                        pi_asynReg.ringReg.ctrl = 0b11;
                        pi_asynReg.enable1 = true;

                        it.rr = {
                            riReg.ringReg.dst1 - proc_bitmap,
                            riReg.ringReg.ctrl,
                            riReg.ringReg.dst2,
                            riReg.ringReg.write_addr,
                            riReg.ringReg.flow_addr,
                            riReg.ringReg.payload};
                    }
                    else
                    {
                        it.rr = {
                            riReg.ringReg.dst1,
                            riReg.ringReg.ctrl,
                            riReg.ringReg.dst2,
                            riReg.ringReg.write_addr,
                            riReg.ringReg.flow_addr,
                            riReg.ringReg.payload};
                    }

                    if (it.rr.ctrl == 0b01)
                    {
                        it.gateway_guider = riReg.gateway_guider;
                        it.match_table_guider = riReg.match_table_guider;
                    }
                    next_proc.r2r.push(it);
                };
            }

            // if backward pkt, receive the guider
            if (flag == 1 && pi_asynReg.ringReg.ctrl == 0b01)
            {
                pi_asynReg.gateway_guider = riReg.gateway_guider;
                pi_asynReg.match_table_guider = riReg.match_table_guider;
                pi_asynReg.enable1 = true;
            }
        }
    }
};

struct RO : public Logic
{
    RO(int id) : Logic(id) {}

    // manage p2r and r2r;
    // it is asynchronous logic
    void execute(const PipeLine *now, PipeLine *next) override
    {
        const ProcessorState &now_proc = now->proc_states[processor_id];
        ProcessorState &next_proc = next->proc_states[processor_id];

        //        const RORegister & roReg = now.processors[processor_id].ro;
        // get front processor id to get the front ri
        int front_proc_id = (PROCESSOR_NUM + processor_id - 1) % PROCESSOR_NUM;
        RIRegister &riReg = next->processors[front_proc_id].ri;

        handle_ro_queue(riReg, now_proc, next_proc);
    }

    // READ type and WRITE type
    void handle_ro_queue(RIRegister &riReg, const ProcessorState &now_proc, ProcessorState &next_proc)
    {
        if (now_proc.p2r.empty() && now_proc.r2r.empty())
        { // no signal (i.e., cancel_dirty, hb, write, bp)
            riReg.enable1 = false;
        }
        else if (now_proc.p2r.empty() && !now_proc.r2r.empty())
        {
            riReg.enable1 = true;
            auto r2r_reg = now_proc.r2r.front();
            riReg.ringReg = {
                r2r_reg.rr.dst1,
                r2r_reg.rr.ctrl,
                r2r_reg.rr.dst2,
                r2r_reg.rr.write_addr,
                r2r_reg.rr.flow_addr,
                r2r_reg.rr.payload};
            if (riReg.ringReg.ctrl == 0b01)
            { // bp
                riReg.gateway_guider = r2r_reg.gateway_guider;
                riReg.match_table_guider = r2r_reg.match_table_guider;
            }
            next_proc.r2r.pop();
        }
        else if (!now_proc.p2r.empty() && now_proc.r2r.empty())
        {
            riReg.enable1 = true;
            auto p2r_reg = now_proc.p2r.front();
            // 根据processor type处理
            if(proc_types[processor_id] == ProcType::READ) {
                riReg.ringReg.dst1 = 0;
            }
            else{
                riReg.ringReg.dst1 = p2r_reg.rr.dst1;
            }
            riReg.ringReg.ctrl = p2r_reg.rr.ctrl;
            riReg.ringReg.dst2 = p2r_reg.rr.dst2;
            riReg.ringReg.write_addr = p2r_reg.rr.write_addr;
            riReg.ringReg.flow_addr = p2r_reg.rr.flow_addr;
            riReg.ringReg.payload = p2r_reg.rr.payload;
            next_proc.p2r.pop();
        }
        else
        {
            riReg.enable1 = true;
            auto p2r_reg = now_proc.p2r.front();
            auto r2r_reg = now_proc.r2r.front();

            // hb with other -> merge
            int flag = 0; // r2r is 0, p2r is 1
            // 如果r2r需要调度的包只有hb，则merge两个包并发送
            if (r2r_reg.rr.ctrl == 0b11)
            {
                riReg.ringReg.dst1 = r2r_reg.rr.dst1 | p2r_reg.rr.dst1;
                riReg.ringReg.ctrl = p2r_reg.rr.ctrl;
                riReg.ringReg.dst2 = p2r_reg.rr.dst2;
                riReg.ringReg.write_addr = p2r_reg.rr.write_addr;
                riReg.ringReg.flow_addr = p2r_reg.rr.flow_addr;
                riReg.ringReg.payload = p2r_reg.rr.payload;
                next_proc.p2r.pop();
                next_proc.r2r.pop();
            }
            else if (p2r_reg.rr.ctrl == 0b11)
            {
                riReg.ringReg.dst1 = r2r_reg.rr.dst1 | p2r_reg.rr.dst1;
                riReg.ringReg.ctrl = r2r_reg.rr.ctrl;
                riReg.ringReg.dst2 = r2r_reg.rr.dst2;
                riReg.ringReg.write_addr = r2r_reg.rr.write_addr;
                riReg.ringReg.flow_addr = r2r_reg.rr.flow_addr;
                riReg.ringReg.payload = r2r_reg.rr.payload;
                next_proc.p2r.pop();
                next_proc.r2r.pop();
            }
            else
            {
                if (now_proc.round_robin_flag == 0)
                {
                    riReg.ringReg = r2r_reg.rr;
                    flag = 0;
                    next_proc.r2r.pop();
                }
                else
                {
                    riReg.ringReg = p2r_reg.rr;
                    flag = 1;
                    next_proc.p2r.pop();
                }
                next_proc.round_robin_flag = 1 - now_proc.round_robin_flag; // round robin schedule r2r and p2r
                // carry the guider from -
                if (flag == 0 && riReg.ringReg.ctrl == 0b01)
                {
                    riReg.match_table_guider = r2r_reg.match_table_guider;
                    riReg.gateway_guider = r2r_reg.gateway_guider;
                }
                else if (flag == 1 && riReg.ringReg.ctrl == 0b01)
                {
                    riReg.match_table_guider = p2r_reg.match_table_guider;
                    riReg.gateway_guider = p2r_reg.gateway_guider;
                }
            }
        }
    }
};

struct PIR_asyn : public Logic
{
    PIR_asyn(int id) : Logic(id) {}

    // from ring: store the pkt to stash, and when the dirty_table is free, search it.
    // 1. receive only hb
    // 2. receive write
    // 3. receive bp
    void execute(const PipeLine *now, PipeLine *next) override
    {

        const ProcessorState &now_proc = now->proc_states[processor_id];
        ProcessorState &next_proc = next->proc_states[processor_id];
        // current cycle's PIAsyn-level pipe
        const PIRAsynRegister &piAsynReg = now->processors[processor_id].pi_asyn;

        PORegister &poReg = next->processors[processor_id].po;

        u32 proc_bitmap = 1 << (PROCESSOR_NUM - processor_id - 1);

        handle_pi_asyn(now_proc, next_proc, piAsynReg, poReg);
    }

    void handle_pi_asyn(const ProcessorState& now_proc, ProcessorState & next_proc, const PIRAsynRegister & now, PORegister & next){
        bool hb = false;
        if(now.enable1){
            if(now.ringReg.ctrl == 0b11){
                hb = true;
            }
            else if(now.ringReg.ctrl == 0b01){
                // bp with hb
                hb = true;
                FlowInfo it{};
                it.phv = transfer_from_payload_to_phv(now.ringReg.payload);
                it.gateway_guider = now.gateway_guider;
                it.match_table_guider = now.match_table_guider;
                it.backward_pkt = true;
                next_proc.r2p_stash.push(it);
            }
            else if(now.ringReg.ctrl == 0b00){
                // write with hb
                hb = true;
                WriteInfo it{};
                it.write_addr = now.ringReg.write_addr;
                it.flow_addr = now.ringReg.flow_addr;
                it.state = now.ringReg.payload;
                next_proc.write_stash.push(it);
            }
        }

//        if(now_proc.normal_pipe_schedule_flag){
//            // todo: directly return?
//            // schedule normal packet,
//            return;
//        }

        auto res = std::pair<CAM_SEARCH_RES, u64>();

        if(!now_proc.write_stash.empty()){
            res = handle_write(now_proc, next_proc);
        }
        else if(!now_proc.r2p_stash.empty()){
            auto pkt = now_proc.r2p_stash.front();
            next_proc.r2p_stash.pop();
            if(now_proc.dirty_cam.find(get_flow_id(pkt.phv)) == now_proc.dirty_cam.end()){
                res.first = BP_NOT_FOUND;
            }
            else{
                res.first = BP_FOUND;
            }
            res.second = get_flow_id(pkt.phv);
        }

        switch(res.first){
            case BP_NOT_FOUND: {
                flow_info_in_cam flow_info{};
                flow_info.timer_offset = now_proc.increase_clk;
                next_proc.increase_clk = 0;
                if(!now_proc.wait_queue_head_flag){
                    next_proc.decrease_clk = now_proc.wait_queue_head.timer_offset;
                }
                flow_info.r2p_first_pkt_idx = flow_info.r2p_last_pkt_idx = now_proc.rp2p_tail;
                next_proc.rp2p_tail = (now_proc.rp2p_tail + 1) % 128;
                flow_info.flow_addr = res.second;
                flow_info.left_pkt_num = 1;
                flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;

                next_proc.dirty_cam.insert(std::pair<u64, flow_info_in_cam>(res.second, flow_info));

                FlowInfo pkt_info{
                        now.phv,
                        now.match_table_guider,
                        now.gateway_guider,
                        now.hash_values, true};
                next_proc.rp2p[flow_info.r2p_last_pkt_idx] = pkt_info;
                next_proc.rp2p_pointer[flow_info.r2p_last_pkt_idx] = -1;

                if(!next_proc.wait_queue_head_flag){
                    next_proc.wait_queue_head_flag = true;
                    next_proc.wait_queue_head = flow_info;
                }
                else {
                    next_proc.wait_queue.push(flow_info);
                }

                break;
            }
            case BP_FOUND: {
                const flow_info_in_cam &flow_info = now_proc.dirty_cam.at(res.second);
                flow_info_in_cam &next_flow_info = next_proc.dirty_cam[res.second];

                next_flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;
                FlowInfo pkt_info{
                        now.phv,
                        now.match_table_guider,
                        now.gateway_guider,
                        {}, {}, true};
                next_proc.rp2p[now_proc.rp2p_tail] = pkt_info;
                if (-1 == flow_info.r2p_first_pkt_idx)
                {
                    next_flow_info.r2p_first_pkt_idx = next_flow_info.r2p_last_pkt_idx = now_proc.rp2p_tail;
                    next_proc.rp2p_pointer[flow_info.r2p_last_pkt_idx] = -1;
                }
                else
                {
                    next_proc.rp2p_pointer[flow_info.r2p_last_pkt_idx] = now_proc.rp2p_tail;
                    next_flow_info.r2p_last_pkt_idx = now_proc.rp2p_tail;
                }

                next_flow_info.left_pkt_num += 1;

                update_wait_queue(res.second, next_flow_info, next_proc);

                next_proc.rp2p_tail = (now_proc.rp2p_tail + 1) % 128;
                break;
            }
            case WRITE_NOT_FOUND: {
                flow_info_in_cam flow_info{};
                flow_info.timer_offset = now_proc.increase_clk;
                next_proc.increase_clk = 0;
                if(!now_proc.wait_queue_head_flag){
                    next_proc.decrease_clk = backward_cycle_num;
                }
                flow_info.r2p_first_pkt_idx = flow_info.r2p_last_pkt_idx = -1;
                flow_info.p2p_first_pkt_idx = flow_info.r2p_last_pkt_idx = -1;
                flow_info.flow_addr = res.second;
                flow_info.left_pkt_num = 0;
                flow_info.cur_state = flow_info_in_cam::FSMState::WAIT;

                // add to dirty table
                cout << "write not found, insert to wait queue." << endl;
                cout << "wait queue size: " << next_proc.wait_queue.size() << endl;
                next_proc.dirty_cam.insert(std::pair<u64, flow_info_in_cam>(res.second, flow_info));
                // add to wait_queue
                if(!next_proc.wait_queue_head_flag){
                    next_proc.wait_queue_head_flag = true;
                    next_proc.wait_queue_head = flow_info;
                }
                else {
                    next_proc.wait_queue.push(flow_info);
                }

                break;
            }
            case WRITE_FOUND: {
                const flow_info_in_cam &flow_info = now_proc.dirty_cam.at(res.second);
                flow_info_in_cam &next_flow_info = next_proc.dirty_cam[res.second];

                if (flow_info.left_pkt_num == 0)
                {
                    next_flow_info.cur_state = flow_info_in_cam::FSMState::WAIT;
                }
                else
                {
                    next_flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;
                }
                next_flow_info.timer_offset = now_proc.increase_clk;
                next_flow_info.r2p_first_pkt_idx = next_flow_info.r2p_last_pkt_idx = -1;

                next_proc.increase_clk = 0;

                next_flow_info.lazy = true;

                update_wait_queue(res.second, next_flow_info, next_proc);

                if(!next_proc.wait_queue_head_flag){
                    next_proc.wait_queue_head_flag = true;
                    next_proc.wait_queue_head = next_flow_info;
                }
                else {
                    next_proc.wait_queue.push(next_flow_info);
                }

                break;
            }
            default: break;
        }

        if(hb){
            cout << "hb!" << endl;
            handle_heartbeat(next, now_proc, next_proc);
        }
    }

    // write信号到来之后立刻修改状态
    std::pair<CAM_SEARCH_RES, u64> handle_write(const ProcessorState &now_proc, ProcessorState &next_proc)
    {
        //
        auto write = next_proc.write_stash.front();
        auto state = write.state;

        int offset = 0;

        // Write State to the State table
        for (int i = 0; i < state_saved_idxs[processor_id].state_num; i++)
        {
            // every stateful table
            // get value to write
            b128 value{};
            auto length = state_saved_idxs[processor_id].state_lengths[i];
            if(length == 1){
                value[3] = state[offset];
            }
            else if(length == 2){
                value[2] = state[offset];
                value[3] = state[offset + 1];
            }
            else if(length == 3){
                value[1] = state[offset];
                value[2] = state[offset + 1];
                value[3] = state[offset + 2];
            }
            else if(length == 4){
                value[0] = state[offset];
                value[1] = state[offset + 1];
                value[2] = state[offset + 2];
                value[3] = state[offset + 3];
            }

            // get hash to find addr
            u64 hash_value = write.write_addr[i];
            b128 hash_values = u64_to_u16_array(hash_value);

            auto match_table = matchTableConfigs[processor_id].matchTables[stateful_table_ids[processor_id][i]];

            bool found_flag = false;
            u32 vacant_key_sram_id = -1;
            u32 vacant_value_sram_id = -1;
            u32 vacant_chip_addr = -1;

            for (int j = 0; j < match_table.number_of_hash_ways; j++)
            {
                if(found_flag) continue;
                // translate from hash to addr
                u32 start_key_index = (hash_values[j] >> 10) * match_table.key_width;
                u32 start_value_index = (hash_values[j] >> 10) * match_table.value_width;
                auto on_chip_addr = (hash_values[j] << 22 >> 22);
                std::array<int, 8> key_sram_columns{};
                std::array<int, 8> value_sram_columns{};
                std::array<b128, 8> obtained_key{};

                for (int k = 0; k < match_table.key_width; k++)
                {
                    // in fact, key_width & value_width here must be 1
                    key_sram_columns[k] = match_table.key_sram_index_per_hash_way[j][start_key_index + k];
                    obtained_key[k] = SRAMs[processor_id][key_sram_columns[k]].get(on_chip_addr);
                }
                for (int k = 0; k < match_table.value_width; k++)
                {
                    value_sram_columns[k] = match_table.value_sram_index_per_hash_way[j][start_value_index + k];
                }

                // compare key & flow_id (here use flow_id as identifier of a match key)
                if(u16_array_to_u64(obtained_key[0]) == write.flow_addr){
                    // key compare success
                    found_flag = true;

                    // write value to proper position
                    SRAMs[processor_id][value_sram_columns[0]].set(int(on_chip_addr), value);
                }
                if(obtained_key[0][0] == 0 && obtained_key[0][1] == 0 && obtained_key[0][2] == 0 && obtained_key[0][3] == 0){
                    // vacant place
                    vacant_key_sram_id = key_sram_columns[0];
                    vacant_value_sram_id = value_sram_columns[0];
                    vacant_chip_addr = on_chip_addr;
                }
            }

            if(!found_flag){
                // insert to vacant place
                if(vacant_key_sram_id != -1) {
                    SRAMs[processor_id][vacant_key_sram_id].set(int(vacant_chip_addr),
                                                                u64_to_u16_array(write.flow_addr));
                    SRAMs[processor_id][vacant_value_sram_id].set(int(vacant_chip_addr), value);
                }
            }

            offset += state_saved_idxs[processor_id].state_lengths[i];
        }

        auto flow_cam = now_proc.dirty_cam;
        auto res = std::pair<CAM_SEARCH_RES, u64>();
        if (flow_cam.find(write.flow_addr) == flow_cam.end())
        {
            res.first = WRITE_NOT_FOUND;
        }
        else
        {
            res.first = WRITE_FOUND;
        }
        res.second = write.flow_addr;
        next_proc.write_stash.pop();
        return res;
    }

    // todo: check all of the idx
    void handle_heartbeat(PORegister &poReg, const ProcessorState &now_proc, ProcessorState &next_proc)
    {
        next_proc.decrease_clk -= 1;
        next_proc.increase_clk += 1;
        // if wait_queue_empty, reset the clock
        if(now_proc.wait_queue.empty() && !now_proc.wait_queue_head_flag){
            next_proc.decrease_clk = 0;
            next_proc.increase_clk = backward_cycle_num;
            return;
        }
        // 将等待队列队首的流从等待队列中拿出加入调度队列；发送cancel dirty信号
        if (now_proc.decrease_clk == 0)
        {
            cout << "timer out! wait queue before: " << next_proc.wait_queue.size() << endl;
            cout << "schedule queue before: " << next_proc.schedule_queue.size() << endl;
            if (!now_proc.wait_queue.empty() || now_proc.wait_queue_head_flag)
            { // wait_queue has flow
                // if not empty, flag is true; empty, possible true
                const flow_info_in_cam wait_flow = now_proc.wait_queue_head;
                flow_info_in_cam& next_schedule_flow = next_proc.wait_queue_head;

                // caution: send cancel_dirty
                u64 flow_addr = wait_flow.flow_addr;
                RP2R_REG it = {
                    0,
                    0b10,
                    u32(1 << (PROCESSOR_NUM - write_proc_ids[processor_id] - 1)),
                    {},
                    flow_addr};
                next_proc.p2r.push(it);

                if (wait_flow.cur_state == flow_info_in_cam::FSMState::WAIT)
                {
                    // buffer is empty
                    // 1. delete the flow in dirty_cam
                    // 2. delete in the wait_queue
                    if (!now_proc.wait_queue.empty())
                    {
                        next_proc.wait_queue_head = now_proc.wait_queue.front();
                        next_proc.wait_queue.pop();
                    }
                    else
                    {
                        next_proc.wait_queue_head_flag = false;
                        next_proc.wait_queue_head = {};
                    }
                    next_proc.dirty_cam.erase(wait_flow.flow_addr);
                    cout << "wait directly delete, wait queue after: " << next_proc.wait_queue.size() << endl;
                }
                else if (wait_flow.cur_state == flow_info_in_cam::FSMState::SUSPEND)
                {
                        next_schedule_flow.cur_state = flow_info_in_cam::FSMState::READY; // SUSPEND->READY : wait->schedule

                        // merge queue
                        if (wait_flow.r2p_first_pkt_idx == -1)
                        { // only p2p buffered
                          // no need to merge
                        }
                        else if (wait_flow.p2p_first_pkt_idx == -1)
                        { // only r2p buffered
                            next_schedule_flow.p2p_first_pkt_idx = wait_flow.r2p_first_pkt_idx;
                            next_schedule_flow.p2p_last_pkt_idx = wait_flow.r2p_last_pkt_idx;
                            next_schedule_flow.r2p_first_pkt_idx = next_schedule_flow.r2p_last_pkt_idx = -1;
                        }
                        else
                        { // r2p and p2p buffered
                            next_schedule_flow.p2p_first_pkt_idx = wait_flow.r2p_first_pkt_idx;
                            next_proc.rp2p_pointer[wait_flow.r2p_last_pkt_idx] = wait_flow.p2p_first_pkt_idx;
                            next_schedule_flow.r2p_first_pkt_idx = next_schedule_flow.r2p_last_pkt_idx = -1;
                        }

                        next_proc.schedule_queue.push(next_schedule_flow);

                        // update in dirty cam
                        next_proc.dirty_cam[next_schedule_flow.flow_addr] = next_schedule_flow;

                        // delete the flow in the wait_queue
                        if (!now_proc.wait_queue.empty())
                        {
                            next_proc.wait_queue_head = now_proc.wait_queue.front();
                            next_proc.wait_queue.pop();
                        }
                        else
                        {
                            next_proc.wait_queue_head_flag = false;
                            next_proc.wait_queue_head = {};
                        }

                        cout << "suspend, wait queue to schedule queue: " << endl;
                        cout << "wait queue after: " << next_proc.wait_queue.size();
                        cout << "schedule queue after: " << next_proc.schedule_queue.size();
                }
                else
                {
                    // nothing
                }
                // todo: 更新自减时钟为新队首的timer offset
                if(next_proc.wait_queue_head_flag){
                    next_proc.decrease_clk = next_proc.wait_queue_head.timer_offset;
                }
               else{
                   next_proc.decrease_clk = backward_cycle_num;
               }
            }
            else
            {

                // wait queue empty
                // no op
            }
        }
        else
        {
        }
    }
};

#endif // RPISA_SW_SCHEDULE_H
