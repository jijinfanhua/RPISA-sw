//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_SCHEDULE_H
#define RPISA_SW_SCHEDULE_H

#include "../pipeline.h"

struct PIW : public Logic {
    PIW(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {
        const PIWRegister & first_piwReg = now.processors[processor_id].piw[0];

        // last processor & not write processor
        if (processor_id == PROCESSOR_NUM-1 && proc_types[processor_id] != ProcType::WRITE) {
            // empty cycle; one cycle
            piw_empty_cycle();
            return;
        } else if (processor_id != PROCESSOR_NUM-1 && proc_types[processor_id] != ProcType::WRITE) {
            // not the last -> only one cycle
            if (first_piwReg.enable1) {
                BaseRegister &baseReg = next.processors[processor_id + 1].base;
                baseReg.enable1 = first_piwReg.enable1;
                baseReg.match_table_guider = first_piwReg.match_table_guider;
                baseReg.gateway_guider = first_piwReg.gateway_guider;
                baseReg.phv = first_piwReg.phv;
            } else {

            }
        } else if (proc_types[processor_id] == ProcType::WRITE) {
            PIWRegister & second_piwReg = next.processors[processor_id].piw[1];
            const ProcessorState & now_proc = now.proc_states[processor_id];
            ProcessorState & next_proc = next.proc_states[processor_id]; // to send pkt/hb/write

            piw_first_cycle(first_piwReg, second_piwReg, now_proc, next_proc);

            const PIWRegister & cur_second_piwReg = now.processors[processor_id].piw[1];

            // second cycle
            if (processor_id != PROCESSOR_NUM - 1) {
                BaseRegister & baseReg = next.processors[processor_id + 1].base;
                handle_piw_write(cur_second_piwReg, now_proc, next_proc);
                if (!cur_second_piwReg.pkt_backward && cur_second_piwReg.enable1) {
                    baseReg.enable1 = cur_second_piwReg.enable1;
                    baseReg.match_table_guider = cur_second_piwReg.match_table_guider;
                    baseReg.gateway_guider = cur_second_piwReg.gateway_guider;
                    baseReg.phv = cur_second_piwReg.phv;
                }
            } else {
                handle_piw_write(cur_second_piwReg, now_proc, next_proc);
            }
        } else {
            // nothing
        }

        // Asyn Code: receive cancel_dirty
    }

    void piw_first_cycle(const PIWRegister &now, PIWRegister &next, const ProcessorState &now_proc,
                         ProcessorState &next_proc) {
        // search the dirty table in PIW using hash addr
        next.enable1 = now.enable1;
        if (!now.enable1) {
            return;
        }

        int table_id = stateful_table_ids[processor_id];
        // get the hash value
        // caution: stateful table only has one way hash
        auto hash_value = now.hash_values[table_id][0];
        auto flow_cam = now_proc.dirty_cam;
        if (flow_cam.find(hash_value) == flow_cam.end()) {
            // if need to writeback: stateless / stateful
            if (now.wb_flag) { // first stateful pkt
                if (now.state_changed) { // state changed; state is in PHV
                    next.state_writeback = true;
                    next.pkt_backward = false;
                } else { // state not changed
                    next.state_writeback = false;
                    next.pkt_backward = false;
                }
            } else { // not stateful pkt
                next.state_writeback = false;
                next.pkt_backward = false;
            }
        } else {
            next.state_writeback = false;
            next.pkt_backward = true;
            next.flow_info = flow_cam[hash_value];
        }
        next.phv = now.phv;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.hash_values = now.hash_values;
    }

    void handle_piw_write(const PIWRegister & now, const ProcessorState & now_proc, ProcessorState & next_proc) {
        // send heartbeat / asyn
        RP2R_REG it{};
        it.rr.ctrl = 0b11;
        it.rr.dst1 = 1 << (PROCESSOR_NUM - now_proc.read_proc_id - 1);

        if (!now.enable1) {
            next_proc.p2r.push(it);
            return;
        }

        // send pkt / write
        if (now.state_writeback) {
            // build ringreg
            it.hash_value = now.hash_values[stateful_table_ids[processor_id]][0];
            it.rr.ctrl = 0b00;
            it.rr.dst2 = it.rr.dst1;
            it.rr.addr = now.hash_values[stateful_table_ids[processor_id]][0];
            for(int i = 0; i < 16; i++) {
                int tmp_idx = state_idx_in_phv[processor_id][i];
                if (tmp_idx < 0) {
                    break;
                }
                // todo: merge fields with different lengths;
                if (tmp_idx < 64) { // U8
                    it.rr.payload[i] = now.phv[tmp_idx];
                } else if (tmp_idx < 160) { // u16
                    it.rr.payload[i] = now.phv[tmp_idx];
                } else {
                    it.rr.payload[i] = now.phv[tmp_idx];
                }
            }
            next_proc.p2r.push(it);
        } else if (now.pkt_backward) {
            it.hash_value = now.hash_values[stateful_table_ids[processor_id]][0];
            it.rr.ctrl = 0b01;
            it.rr.dst2 = it.rr.dst1;
            it.rr.addr = now.hash_values[stateful_table_ids[processor_id]][0];
            it.rr.payload = transfer_from_phv_to_payload(now.phv); // todo: transform from phv(32bit*224) to payload(32bit * 128)
            it.gateway_guider = now.gateway_guider;
            it.match_table_guider = now.match_table_guider;
            next_proc.p2r.push(it);
        } else {
            // just to Base of next proc
        }
    }

    std::array<u32, 128> transfer_from_phv_to_payload(PHV phv) {
        std::array<u32, 128> payload{};
        // 8bit * 64 ->16
        for(int i = 0 ; i < 16; i++) {
            payload[i] = (phv[i * 4] << 24) + (phv[i * 4 + 1] << 16) + (phv[i * 4 + 2] << 8) + phv[i * 4 + 3];
        }
        // 16bit * 96 -> 48
        for(int i = 16 ; i < 64; i++) {
            payload[i] = (phv[32 + i * 2] << 16) + phv[32 + i * 2 + 1];
        }
        // 32bit * 64 -> 32
        for(int i = 64 ; i < 128; i++) {
            payload[i] = phv[160 + i];
        }
        return payload;
    }

    void piw_empty_cycle() {

    }
};

struct PIR : public Logic {

    void execute(const PipeLine &now, PipeLine &next) override {
        const PIRegister & piReg = now.processors[processor_id].pi[0];
        const PIRegister & old_nextPIReg = now.processors[processor_id].pi[1];

        PIRegister & nextPIReg = next.processors[processor_id].pi[1];
        PORegister & poReg = next.processors[processor_id].po;

        const ProcessorState & now_proc = now.proc_states[processor_id]; // can only be read
        ProcessorState & next_proc = next.proc_states[processor_id]; // can only be written
        next_proc = now_proc;// copy to next cycle

        pir_pipe_first_cycle(piReg, now_proc, next_proc, nextPIReg);
        pir_pipe_second_cycle(old_nextPIReg, now_proc, next_proc, poReg);

    }

    void pir_pipe_second_cycle(const PIRegister &now, const ProcessorState &now_proc, ProcessorState &next_proc, PORegister &next) {
        next.enable1 = now.enable1;
        if (!now.enable1) {
            return;
        }

        if (!now.need_to_block) { // normal stateless pkt
            next.phv = now.phv;
            next.match_table_guider = now.match_table_guider;
            next.gateway_guider = now.gateway_guider;
            // to let the PO scheduler know that there is a normal pipe pkt
            next_proc.normal_pipe_schedule_flag = true;
            return;
        }

        // pkt is blocked
        next.enable1 = false;

        int table_id = stateful_table_ids[processor_id];
        auto hash_value = now.hash_values[table_id][0];

        const flow_info_in_cam & flow_info = now.flow_info;
//        const flow_info_in_cam & flow_info = now_proc.dirty_cam.at(hash_value);
        flow_info_in_cam & next_flow_info = next_proc.dirty_cam[hash_value];

        if (flow_info.p2p_first_pkt_idx == -1) {// only write, no pkt come/ or queue only have backward pkt
            next_flow_info.p2p_first_pkt_idx = next_flow_info.p2p_last_pkt_idx = now_proc.rp2p_tail;
            next_proc.rp2p[now_proc.rp2p_tail] =
                    {now.phv, now.match_table_guider, now.gateway_guider,
                     now.hash_values, hash_value, false, table_id};
            next_flow_info.left_pkt_num = flow_info.left_pkt_num + 1;
            next_proc.rp2p_pointer[flow_info.p2p_last_pkt_idx] = -1;
        } else { // have buffered pkt -> SUSPEND/READY
            next_proc.rp2p[now_proc.rp2p_tail] =
                    {now.phv, now.match_table_guider, now.gateway_guider,
                     now.hash_values, hash_value, false, table_id};
            next_flow_info.left_pkt_num = flow_info.left_pkt_num + 1;
            next_proc.rp2p_pointer[flow_info.p2p_last_pkt_idx] = now_proc.rp2p_tail;
            next_flow_info.p2p_last_pkt_idx = now_proc.rp2p_tail;
        }

        // state transition
        if (flow_info.cur_state == flow_info_in_cam::FSMState::READY) {
            next_flow_info.cur_state = flow_info_in_cam::FSMState::READY;
        } else {
            next_flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;
        }

        // update global tail
        next_proc.rp2p_tail = (now_proc.rp2p_tail + 1) % 128;
    }

    void pir_pipe_first_cycle(const PIRegister &now, const ProcessorState & now_proc, ProcessorState & next_proc, PIRegister &next) {
        next.enable1 = now.enable1;
        if (!now.enable1) {
            return;
        }

        int table_id = stateful_table_ids[processor_id];
        // get the hash value
        // caution: stateful table only has one way hash
        auto hash_value = now.hash_values[table_id][0];
        auto flow_cam = now_proc.dirty_cam;
        // the normal pipeline pkt
        if (flow_cam.find(hash_value) == flow_cam.end()) {
            // go to normal match-action,
            // but need to go through the second cycle of PIR without processing
            next.phv = now.phv;
            next.need_to_block = false;
            next.match_table_guider = now.match_table_guider;
            next.gateway_guider = now.gateway_guider;
            next.hash_values = now.hash_values;
        } else {
            next.flow_info = flow_cam[hash_value];
            next.phv = now.phv;
            next.need_to_block = true;
            next.match_table_guider = now.match_table_guider;
            next.gateway_guider = now.gateway_guider;
            next.hash_values = now.hash_values;
        }
    }
};

struct PO : public Logic {
    PO(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {
        const PORegister & poReg = now.processors[processor_id].po;
        GetAddressRegister & getAddrReg = next.processors[processor_id].getAddress;

        const ProcessorState & now_proc = now.proc_states[processor_id];
        ProcessorState & next_proc = next.proc_states[processor_id];

        asyn_schedule(poReg, getAddrReg, now_proc, next_proc);
    }

    void asyn_schedule(const PORegister & now, GetAddressRegister & next, const ProcessorState & now_proc, ProcessorState & next_proc) {
        if (now_proc.normal_pipe_schedule_flag) {
            // schedule the po's register
            next.enable1 = true;
            next.phv = now.phv;
            next.match_table_keys = now.match_table_keys;
            next.gateway_guider = now.gateway_guider;
            next.match_table_guider = now.match_table_guider;
            return;
        }

        if (now_proc.schedule_queue.empty()) {
            next.enable1 = false;
            return;
        }

        flow_info_in_cam & schedule_flow = next_proc.schedule_queue.front();
        if (schedule_flow.lazy) { // ready -> suspend
            next_proc.schedule_queue.pop();
            next.enable1 = false;
        } else {
            next.enable1 = true;
            schedule_flow.left_pkt_num -= 1;
            if (schedule_flow.left_pkt_num == 0) {
                next_proc.schedule_queue.pop();// just delete, not add to the queue tail
            } else {
                next_proc.schedule_queue.pop();
                next_proc.schedule_queue.push(schedule_flow);
            }
            // schedule the pkt to getAddr Module
            auto pkt = now_proc.rp2p[schedule_flow.p2p_first_pkt_idx];
            next.match_table_guider = pkt.match_table_guider;
            next.gateway_guider = pkt.gateway_guider;
            next.phv = pkt.phv;
            // todo: backward pkt will not go through the stateless table of the READ processor
            if (pkt.backward_pkt) {
                next.backward_pkt = true;
                next.hash_value = pkt.hash_value;
            } else {
                next.hash_values = pkt.hash_values;
            }
            schedule_flow.p2p_first_pkt_idx = now_proc.rp2p_pointer[schedule_flow.p2p_first_pkt_idx];
            // todo: set the rp2p to null
        }

    }
};

struct RI : public Logic {
    RI(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {
        //  -----------
        // |    PIR    |    <----- r2p <------|
        //  -----------                       |
        //       |                            |
        //      p2r                           |
        //       |                            |
        //  ----------                   ----------
        // |    RO    | <--- r2r <----- |    RI    |
        //  ----------                   ----------
        const ProcessorState & now_proc = now.proc_states[processor_id];
        ProcessorState & next_proc = next.proc_states[processor_id];
        next_proc = now_proc;

        const RIRegister & riReg = now.processors[processor_id].ri;
        PIRAsynRegister & pi_asynReg = next.processors[processor_id].pi_asyn[0];
        RORegister & roReg = next.processors[processor_id].ro;

        handle_ri_signal(riReg, pi_asynReg, next_proc);
    }

    void handle_ri_signal(const RIRegister & riReg, PIRAsynRegister & pi_asynReg, ProcessorState & next_proc) {
        // get the one-hot code of the processor
        u32 proc_bitmap = 1 << (PROCESSOR_NUM - processor_id - 1);

        // if dst2 == proc_bitmap: dst1-ctrl-dst2-addr-payload to pi_asyn; left dst1 to roReg
        // else :
        // if ctrl == 0b11: get dst1 to pi_asyn, left to ro; ctrl = 0b11
        // else: get dst1 to pi_asyn, pi.ctrl = 0b11; left to ro
        int flag = 0;
        // todo: it is not sure if need to pass enable; -> no
        RP2R_REG it{};
        if (riReg.ringReg.dst2 == proc_bitmap) { // the signal dst is this proc
            pi_asynReg.ringReg = {
                    proc_bitmap & riReg.ringReg.dst1,
                    riReg.ringReg.ctrl,
                    riReg.ringReg.dst2,
                    riReg.ringReg.addr,
                    riReg.ringReg.payload
            };
            flag = 1; // bp, write to pi
            if (riReg.ringReg.dst1 - proc_bitmap) { // other hb left, to ro
                it.rr = {
                        riReg.ringReg.dst1 - proc_bitmap,
                        0b11,
                        0,
                        0
                };
                next_proc.r2r.push(it);
//                roReg.enable1 = true; // no need to enable; just verify the queue
            } else {
//                roReg.enable1 = false;
            }
        } else {
            // if ctrl == 0b11: get dst1 to pi_asyn, left to ro; ctrl = 0b11
            if (riReg.ringReg.ctrl == 0b11) {
                if (riReg.ringReg.dst1 & proc_bitmap) { // have hb to this proc
                    pi_asynReg.ringReg.dst1 = proc_bitmap;
                    pi_asynReg.ringReg.ctrl = 0b11;
                    pi_asynReg.enable1 = true;
                    // to ro's r2r
                    if (riReg.ringReg.dst1 - proc_bitmap) { // have left hb to other procs
                        it.rr = {
                                riReg.ringReg.dst1 - proc_bitmap,
                                0b11,
                                0,
                                0
                        };
                        next_proc.r2r.push(it);
//                        roReg.enable1 = true;
                    } else {
//                        roReg.enable1 = false;
                    }
                } else { // have hb only to other procs
                    it.rr = {
                            riReg.ringReg.dst1,
                            0b11,
                            0,
                            0
                    };
                    next_proc.r2r.push(it);
//                    roReg.enable1 = true;
                }
            } else { // else: get dst1 to pi_asyn, pi.ctrl = 0b11; left to ro
                if (riReg.ringReg.dst1 & proc_bitmap) { // have hb to this proc
                    pi_asynReg.ringReg.dst1 = proc_bitmap;
                    pi_asynReg.ringReg.ctrl = 0b11;
                    pi_asynReg.enable1 = true;

                    it.rr = {
                            riReg.ringReg.dst1 - proc_bitmap,
                            riReg.ringReg.ctrl,
                            riReg.ringReg.dst2,
                            riReg.ringReg.addr,
                            riReg.ringReg.payload
                    };
                } else {
                    it.rr = {
                            riReg.ringReg.dst1,
                            riReg.ringReg.ctrl,
                            riReg.ringReg.dst2,
                            riReg.ringReg.addr,
                            riReg.ringReg.payload
                    };
                }

                if (it.rr.ctrl == 0b01) {
                    it.gateway_guider = riReg.gateway_guider;
                    it.match_table_guider = riReg.match_table_guider;
                    it.hash_value = riReg.hash_value;
                }
                next_proc.r2r.push(it);
            };
        }

        // if backward pkt, receive the guider
        if (flag == 1 && pi_asynReg.ringReg.ctrl == 0b01) {
            pi_asynReg.gateway_guider = riReg.gateway_guider;
            pi_asynReg.match_table_guider = riReg.match_table_guider;
            pi_asynReg.hash_value = riReg.hash_value; // returned hash value, used to search dirty_table; in fact, 'addr' is enough?
            pi_asynReg.enable1 = true;
        }
    }
};

struct RO : public Logic {
    RO(int id) : Logic(id) {}

    // manage p2r and r2r;
    // it is asynchronous logic
    void execute(const PipeLine &now, PipeLine &next) override {
        const ProcessorState & now_proc = now.proc_states[processor_id];
        ProcessorState & next_proc = next.proc_states[processor_id];
        next_proc = now_proc;

//        const RORegister & roReg = now.processors[processor_id].ro;
        // get front processor id to get the front ri
        int front_proc_id = (PROCESSOR_NUM + processor_id - 1) % PROCESSOR_NUM;
        RIRegister & riReg = next.processors[front_proc_id].ri;

        handle_ro_queue(riReg, now_proc, next_proc);
    }

    // READ type and WRITE type
    void handle_ro_queue(RIRegister & riReg, const ProcessorState & now_proc, ProcessorState & next_proc) {
        if (now_proc.p2r.empty() && now_proc.r2r.empty()) { // no signal (i.e., cancel_dirty, hb, write, bp)
            riReg.enable1 = false;
        } else if (now_proc.p2r.empty() && !now_proc.r2r.empty()) {
            auto r2r_reg = now_proc.r2r.front();
            riReg.ringReg = {
                    r2r_reg.rr.dst1,
                    r2r_reg.rr.ctrl,
                    r2r_reg.rr.dst2,
                    r2r_reg.rr.addr,
                    r2r_reg.rr.payload
            };
            if (riReg.ringReg.ctrl == 0b01) {// bp
                riReg.gateway_guider = r2r_reg.gateway_guider;
                riReg.match_table_guider = r2r_reg.match_table_guider;
                riReg.hash_value = r2r_reg.hash_value; // todo: verify if it is needed
            }
            next_proc.r2r.pop();
        } else if (!now_proc.p2r.empty() && now_proc.r2r.empty()) {
            auto p2r_reg = now_proc.p2r.front();
            riReg.ringReg.dst1 = 0; // no hb
            riReg.ringReg.ctrl = p2r_reg.rr.ctrl;
            riReg.ringReg.dst2 = p2r_reg.rr.dst2;
            riReg.ringReg.addr = p2r_reg.rr.addr;
            riReg.ringReg.payload = p2r_reg.rr.payload;
            next_proc.p2r.pop();
        } else {
            auto p2r_reg = now_proc.p2r.front();
            auto r2r_reg = now_proc.r2r.front();



            if (r2r_reg.rr.ctrl == 0b11) { // hb + cancel_dirty
                riReg.ringReg.dst1 = r2r_reg.rr.dst1;
                riReg.ringReg.ctrl = 0b10;
                riReg.ringReg.dst2 = p2r_reg.rr.dst2;
                riReg.ringReg.addr = p2r_reg.rr.addr;
                riReg.ringReg.payload = p2r_reg.rr.payload;
            } else if (r2r_reg.rr.ctrl == 0b10 || r2r_reg.rr.ctrl == 0b00 || r2r_reg.rr.ctrl == 0b01) {
                // cancel_dirty | [cancel_dirty | write | bp]
                if (now_proc.round_robin_flag == 0) {
                    riReg.ringReg = r2r_reg.rr;
                    next_proc.r2r.pop();
                } else {
                    riReg.ringReg = p2r_reg.rr;
                    next_proc.p2r.pop();
                }
                next_proc.round_robin_flag = 1 - now_proc.round_robin_flag;//round robin schedule r2r and p2r
                if (riReg.ringReg.ctrl == 0b01) {
                    riReg.match_table_guider = r2r_reg.match_table_guider;
                    riReg.gateway_guider = r2r_reg.gateway_guider;
                    riReg.hash_value = r2r_reg.hash_value; // todo: verify if it is needed
                }
            }
        }
    }
};

struct PIR_asyn : public Logic {
    PIR_asyn(int id) : Logic(id) {}

    // from ring: store the pkt to stash, and when the dirty_table is free, search it.
    // 1. receive only hb
    // 2. receive write
    // 3. receive bp
    void execute(const PipeLine &now, PipeLine &next) override {

        const ProcessorState & now_proc = now.proc_states[processor_id];
        ProcessorState & next_proc = next.proc_states[processor_id];
        next_proc = now_proc;
        // current cycle's PIAsyn-level pipe
        const PIRAsynRegister & piAsynReg = now.processors[processor_id].pi_asyn[0];
        // next cycle's second PIAsyn-level pipe
        PIRAsynRegister & secondPIAsynReg = next.processors[processor_id].pi_asyn[1];
        // current cycle's second PIAsyn-level pipe
        const PIRAsynRegister & cur_secondPIAsynReg = now.processors[processor_id].pi_asyn[1];

        RORegister & roReg = next.processors[processor_id].ro;
        // next cycle's second PO-level pipe, to receive pkt from second PIAsyn-level
        PORegister & poReg = next.processors[processor_id].po;

        u32 proc_bitmap = 1 << (PROCESSOR_NUM - processor_id - 1);

        pi_asyn_first_cycle(now_proc, next_proc, piAsynReg, poReg, secondPIAsynReg);

        pi_asyn_second_cycle(now_proc, next_proc, cur_secondPIAsynReg);
    }

    void pi_asyn_second_cycle(const ProcessorState & now_proc, ProcessorState & next_proc, const PIRAsynRegister & now) {
        switch (now.cam_search_res) {
            case BP_NOT_FOUND : {
                // write lost
                // set Increase CLK to this addr -> offset = I; I = 0
                // [addr, offset] -> wait_queue (tail)
                // flow_cam[new_index] = addr;
                //          .RAM[new_index].{state, head, tail} = SUSPEND, new_tail, new_tail
                flow_info_in_cam flow_info{};
                flow_info.timer_offset = now_proc.increase_clk;
                next_proc.increase_clk = 0;
                flow_info.r2p_first_pkt_idx = flow_info.r2p_last_pkt_idx =  now_proc.rp2p_tail;
                next_proc.rp2p_tail = (now_proc.rp2p_tail + 1 ) % 128;
                flow_info.flow_addr = now.hash_value;
                flow_info.left_pkt_num = 1;
                flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;

                next_proc.dirty_cam[now.hash_value] = flow_info;

                FlowInfo pkt_info{
                        now.phv,
                        now.match_table_guider,
                        now.gateway_guider,
                        now.hash_values
                };
                next_proc.rp2p[flow_info.r2p_last_pkt_idx] = pkt_info;
                next_proc.rp2p_pointer[flow_info.r2p_last_pkt_idx] = -1;

                // todo: add this flow to wait_queue
                next_proc.wait_queue.push(flow_info);

                break;
            }
            case BP_FOUND : {
                // 1. state to SUSPEND
                // 2. store this pkt to the tail (get the flow's tail)
                // 3. update tail
                // 4. update pointer
                // todo: at
                const flow_info_in_cam & flow_info = now_proc.dirty_cam.at(now.hash_value);
                flow_info_in_cam & next_flow_info = next_proc.dirty_cam[now.hash_value];

                next_flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;
                FlowInfo pkt_info{
                        now.phv,
                        now.match_table_guider,
                        now.gateway_guider,
                        now.hash_values
                };
                next_proc.rp2p[now_proc.rp2p_tail] = pkt_info;
                if (-1 == flow_info.r2p_first_pkt_idx) {
                    next_flow_info.r2p_first_pkt_idx = next_flow_info.r2p_last_pkt_idx = now_proc.rp2p_tail;
                    next_proc.rp2p_pointer[flow_info.r2p_last_pkt_idx] = -1;
                } else {
                    next_proc.rp2p_pointer[flow_info.r2p_last_pkt_idx] = now_proc.rp2p_tail;
                    next_flow_info.r2p_last_pkt_idx = now_proc.rp2p_tail;
                }

                next_flow_info.left_pkt_num += 1;

                // todo: update rp2p_tail; this is not a good update method
                next_proc.rp2p_tail = (now_proc.rp2p_tail + 1) % 128;
                break;
            }
            case WRITE_NOT_FOUND : {
                // 1. first write: RUN -> WAIT; i.e., add this flow to flow_cam
                // 2. add this flow to wait_queue
                // 3. offset <- I; I = 0
                flow_info_in_cam flow_info{};
                flow_info.timer_offset = now_proc.increase_clk;
                next_proc.increase_clk = 0;
                flow_info.r2p_first_pkt_idx = flow_info.r2p_last_pkt_idx = -1;
                flow_info.flow_addr = now.hash_value;
                flow_info.left_pkt_num = 0;
                flow_info.cur_state = flow_info_in_cam::FSMState::WAIT;

                // add to dirty table
                next_proc.dirty_cam[now.hash_value] = flow_info;
                // add to wait_queue
                next_proc.wait_queue.push(flow_info);

                break;
            }
            case WRITE_FOUND : {
                // 1. READY to SUSPEND
                // 2. write state; -> done in first cycle
                // 3. set it from Schedule Queue to Wait Queue; set lazy tag, for circular scheduling
                // 4. flow.offset=I, I=0;
                // 5. set packet buffer.

                const flow_info_in_cam & flow_info = now_proc.dirty_cam.at(now.hash_value);
                flow_info_in_cam & next_flow_info = next_proc.dirty_cam[now.hash_value];

                if (flow_info.left_pkt_num == 0) {
                    next_flow_info.cur_state = flow_info_in_cam::FSMState::WAIT;
                } else {
                    next_flow_info.cur_state = flow_info_in_cam::FSMState::SUSPEND;
                }
                next_flow_info.timer_offset = now_proc.increase_clk;
                next_flow_info.r2p_first_pkt_idx = next_flow_info.r2p_last_pkt_idx = -1;

                next_proc.increase_clk = 0;

                // todo: how to set lazy tag in schedule queue
                next_flow_info.lazy = true;

                next_proc.wait_queue.push(flow_info);

                break;
            }
            default : break;
        }
    }

    void handle_write(const ProcessorState & now_proc, ProcessorState & next_proc, PIRAsynRegister & next) {
        auto write = now_proc.write_stash.front();
        u64 hash_value = write.addr;
        auto state = write.state;

        // Write State to the State table
        int table_id = stateful_table_ids[processor_id];
        auto match_table = matchTableConfigs[processor_id].matchTables[table_id];
        u32 start_index = hash_value >> 10;
        u64 on_chip_addr = hash_value << 54 >> 54;
        for(int i = 0; i < match_table.value_width; i++) {
            SRAMs[processor_id][match_table.value_sram_index_per_hash_way[0][start_index + i]]
                    .set(int(on_chip_addr), {state[i * 4], state[i * 4 + 1], state[i * 4 + 2], state[i * 4 + 3]});
        }

        auto flow_cam = now_proc.dirty_cam;
        if (flow_cam.find(hash_value) == flow_cam.end()) {
            //
            next.cam_search_res = WRITE_NOT_FOUND;
            next.hash_value = hash_value;
//            next.hash_values = now.hash_values;
        } else {
            next.cam_search_res = WRITE_FOUND;
            next.hash_value = hash_value;
//            next.hash_values = now.hash_values;
        }

        next_proc.write_stash.pop();
    }

    void pi_asyn_first_cycle(const ProcessorState & now_proc, ProcessorState & next_proc,
                                    const PIRAsynRegister & now, PORegister & poReg, PIRAsynRegister & next) {
        // the PIAsyn should be triggerred by RI's input(enable1, i.e., pkt, hb or write)
        // or not-empty write_stash(state), r2p_stash(pkt)
        if (!now.enable1) { // no input from RI, so check two stashes, and write_stash has higher priority
            // but if there is normal pkt from pipeline, do it first(search in flow_cam).
            // because flow_cam can only be accessed by one.
            if (now_proc.normal_pipe_pkt) {
                return;
            }
            if (!now_proc.write_stash.empty()) {
                // get the write information, call handle write to write state to stateful SRAM
                handle_write(now_proc, next_proc, next);
                return;
            } else if (!now_proc.r2p_stash.empty()) {
                auto pkt = now_proc.r2p_stash.front();
                next_proc.r2p_stash.pop();
                // search
                int table_id = stateful_table_ids[processor_id];
                // get the hash value
                // caution: stateful table only has one way hash
                auto hash_value = now.hash_values[table_id][0];
                auto flow_cam = now_proc.dirty_cam;
                if (flow_cam.find(hash_value) == flow_cam.end()) {
                    next.cam_search_res = BP_NOT_FOUND; // write lost. create timer of this flow
                    next.hash_values = pkt.hash_values;
                    next.gateway_guider = pkt.gateway_guider;
                    next.match_table_guider = pkt.match_table_guider;
                    next.phv = pkt.phv;
                } else {
                    next.cam_search_res = BP_FOUND; // set state to SUSPEND
                    next.hash_values = pkt.hash_values;
                    next.gateway_guider = pkt.gateway_guider;
                    next.match_table_guider = pkt.match_table_guider;
                    next.phv = pkt.phv;
                }
                return;
            } else {
                return;
            }
        }

        // have RI's input
        if (now.ringReg.ctrl == 0b11) {// only hb
            handle_heartbeat(poReg, now_proc, next_proc);
        } else if (now.ringReg.ctrl == 0b01) { // bp with hb
            handle_heartbeat(poReg, now_proc, next_proc);
            // search pir_dirty_cam, record the result to next cycle;
            if (now_proc.normal_pipe_pkt) { // normal pipe has pkt
                FlowInfo it {
                        transfer_from_payload_to_phv(now.ringReg.payload),
                        now.gateway_guider,
                        now.match_table_guider
                };
                next_proc.r2p_stash.push(it); // set it to the r2p stash
            } else if (now_proc.write_stash.empty()){ // normal pipe don't have pkt & don't have pending write
                FlowInfo it {};
                it.phv = transfer_from_payload_to_phv(now.ringReg.payload);
                it.gateway_guider = now.gateway_guider;
                it.match_table_guider = now.match_table_guider;
                next_proc.r2p_stash.push(it);

                auto pkt = now_proc.r2p_stash.front();
                next_proc.r2p_stash.pop();
                // search
                int table_id = stateful_table_ids[processor_id];
                // get the hash value
                // caution: stateful table only has one way hash
                auto hash_value = now.hash_values[table_id][0];
                auto flow_cam = now_proc.dirty_cam;
                if (flow_cam.find(hash_value) == flow_cam.end()) {
                    next.cam_search_res = BP_NOT_FOUND; // write lost. create timer of this flow
                    next.hash_values = pkt.hash_values;
                    next.gateway_guider = pkt.gateway_guider;
                    next.match_table_guider = pkt.match_table_guider;
                    next.phv = pkt.phv;
                } else {
                    next.cam_search_res = BP_FOUND; // set state to SUSPEND
                    next.hash_values = pkt.hash_values;
                    next.gateway_guider = pkt.gateway_guider;
                    next.match_table_guider = pkt.match_table_guider;
                    next.phv = pkt.phv;
                }
            } else if (!now_proc.write_stash.empty()) {
                // handle write
                handle_write(now_proc, next_proc, next);
            } else {
                // nothing
            }
        } else if (now.ringReg.ctrl == 0b00) { // write with hb
            handle_heartbeat(poReg, now_proc, next_proc);
            // addr & payload (state)
            if (now_proc.normal_pipe_pkt) {
                WriteInfo it {};
                it.addr = now.ringReg.addr;
                it.state = now.ringReg.payload;
                next_proc.write_stash.push(it);
            } else {
                WriteInfo it {};
                it.addr = now.ringReg.addr;
                it.state = now.ringReg.payload;
                next_proc.write_stash.push(it);

                handle_write(now_proc, next_proc, next);
            }
        } else {
            // nothing
        }
    }

    PHV transfer_from_payload_to_phv(std::array<u32, 128> payload) {
        PHV phv;
        // 64 * 8 | 96 * 16 | 64 * 32
        for(int i = 0; i < 16; i++) {
            phv[i * 4] = payload[i] >> 24;
            phv[i * 4 + 1] = payload[i] << 8 >> 24;
            phv[i * 4 + 2] = payload[i] << 16 >> 24;
            phv[i * 4 + 3] = payload[i] << 24 >> 24;
        }

        for(int i = 0; i < 48; i++) {
            phv[64 + i * 2] = payload[16 + i] >> 16;
            phv[64 + i * 2 + 1] = payload[16 + i] << 16 >> 16;
        }

        for(int i = 0; i < 64; i++) {
            phv[160 + i] = payload[64 + i];
        }
        return phv;
    }

    // todo: check all of the idx
    void handle_heartbeat(PORegister & poReg, const ProcessorState & now_proc, ProcessorState & next_proc) {
        if (now_proc.decrease_clk - 1 == 0) {
            if (!now_proc.wait_queue.empty() || now_proc.wait_queue_head_flag) { // wait_queue has flow
                // if not empty, flag is true; empty, possible true
                const flow_info_in_cam & wait_flow = now_proc.wait_queue_head;
                flow_info_in_cam next_schedule_flow = next_proc.wait_queue_head;

                // caution: send cancel_dirty
                u32 flow_addr = wait_flow.flow_addr;
                RP2R_REG it = {
                        0,
                        0b10,
                        u32(1 << (PROCESSOR_NUM - now_proc.write_proc_id - 1)),
                        flow_addr
                };
                next_proc.p2r.push(it);

                if (wait_flow.cur_state == flow_info_in_cam::FSMState::WAIT) {
                    // buffer is empty
                    // 1. delete the flow in dirty_cam
                    // 2. delete in the wait_queue
                    if (!now_proc.wait_queue.empty()) {
                        next_proc.wait_queue_head = now_proc.wait_queue.front();
                        next_proc.wait_queue.pop();
                    } else {
                        next_proc.wait_queue_head_flag = false;
                        next_proc.wait_queue_head = {};
                    }
                    next_proc.dirty_cam.erase(wait_flow.hash_value);
                } else if (wait_flow.cur_state == flow_info_in_cam::FSMState::SUSPEND){
                    if (now_proc.normal_pipe_schedule_flag) { // normal pipe has pkt, push it to schedule queue directly
                        next_proc.schedule_queue.push(next_schedule_flow);
                        next_schedule_flow.cur_state = flow_info_in_cam::FSMState::READY; // SUSPEND->READY : wait->schedule

                        // merge queue
                        if (wait_flow.r2p_first_pkt_idx == -1) { // only p2p buffered
                            // no need to merge
                        } else if (wait_flow.p2p_first_pkt_idx == -1) { // only r2p buffered
                            next_schedule_flow.p2p_first_pkt_idx = wait_flow.r2p_first_pkt_idx;
                            next_schedule_flow.p2p_last_pkt_idx = wait_flow.r2p_last_pkt_idx;
                            next_schedule_flow.r2p_first_pkt_idx = next_schedule_flow.r2p_last_pkt_idx = -1;
                        } else { // r2p and p2p buffered
                            next_schedule_flow.p2p_first_pkt_idx = wait_flow.r2p_first_pkt_idx;
                            next_proc.rp2p_pointer[wait_flow.r2p_last_pkt_idx] = wait_flow.p2p_first_pkt_idx;
                            next_schedule_flow.r2p_first_pkt_idx = next_schedule_flow.r2p_last_pkt_idx = -1;
                        }

                        // delete the flow in the wait_queue
                        if (!now_proc.wait_queue.empty()) {
                            next_proc.wait_queue_head = now_proc.wait_queue.front();
                            next_proc.wait_queue.pop();
                        } else {
                            next_proc.wait_queue_head_flag = false;
                            next_proc.wait_queue_head = {};
                        }
                    } else {// normal pipe doesn't have pkt, directly to PO
                        // todo: verify if it needs to go through the second cycle?
                        poReg.enable1 = true;

                        if (wait_flow.left_pkt_num <= 1) {
                            // delete it in dirty table & wait_queue
                            next_proc.dirty_cam.erase(wait_flow.hash_value);
                            if (!now_proc.wait_queue.empty()) {
                                next_proc.wait_queue_head = now_proc.wait_queue.front();
                                next_proc.wait_queue.pop();
                            } else {
                                next_proc.wait_queue_head_flag = false;
                                next_proc.wait_queue_head = {};
                            }
                        } else {
                            next_schedule_flow.left_pkt_num -= 1;
                            next_schedule_flow.cur_state = flow_info_in_cam::FSMState::READY;
                            // merge queue
                            if (wait_flow.r2p_first_pkt_idx == -1) { // only p2p buffered
                                // no need to merge
                            } else if (wait_flow.p2p_first_pkt_idx == -1) { // only r2p buffered
                                next_schedule_flow.p2p_first_pkt_idx = wait_flow.r2p_first_pkt_idx;
                                next_schedule_flow.p2p_last_pkt_idx = wait_flow.r2p_last_pkt_idx;
                                next_schedule_flow.r2p_first_pkt_idx = next_schedule_flow.r2p_last_pkt_idx = -1;
                            } else { // r2p and p2p buffered
                                int tmp_first = wait_flow.p2p_first_pkt_idx;
                                next_schedule_flow.p2p_first_pkt_idx = wait_flow.r2p_first_pkt_idx;
                                next_proc.rp2p_pointer[wait_flow.r2p_last_pkt_idx] = tmp_first;
                                next_schedule_flow.r2p_first_pkt_idx = next_schedule_flow.r2p_last_pkt_idx = -1;
                            }
                            // add to schedule_queue & delete it in wait_queue
                            if (!now_proc.wait_queue.empty()) {
                                next_proc.wait_queue_head = now_proc.wait_queue.front();
                                next_proc.wait_queue.pop();
                            } else {
                                next_proc.wait_queue_head_flag = false;
                                next_proc.wait_queue_head = {};
                            }
                            next_proc.schedule_queue.push(next_schedule_flow);
                        }
                        // schedule
                        poReg.match_table_guider = now_proc.rp2p[wait_flow.p2p_first_pkt_idx].match_table_guider;
                        poReg.gateway_guider = now_proc.rp2p[wait_flow.p2p_first_pkt_idx].gateway_guider;
                        poReg.phv = now_proc.rp2p[wait_flow.p2p_first_pkt_idx].phv;
                        poReg.hash_values = now_proc.rp2p[wait_flow.p2p_first_pkt_idx].hash_values;
                        // todo: to verify
                        poReg.hash_value = now_proc.rp2p[wait_flow.p2p_first_pkt_idx].hash_value;
                    }
                } else {
                    // nothing
                }
            } else {
                // wait queue empty
                // no op
                // todo: set D and I; czk's hardware code
//                proc.decrease_clk
            }
        } else {
            next_proc.decrease_clk -= 1;
            next_proc.increase_clk += 1;
        }
    }
};

#endif //RPISA_SW_SCHEDULE_H
