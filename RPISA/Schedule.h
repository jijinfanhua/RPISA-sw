//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_SCHEDULE_H
#define RPISA_SW_SCHEDULE_H

#include "../PipeLine.h"


struct Schedule : public Logic {


    bool isRead;

    DirtyTable dirty;
    bool isDirty(int label) const {
        return dirty.find(label) != dirty.end() && dirty.at(label);
    }

    map<int, int> timeOut;
    int TIME_OUT;

    enum FlowState {
        READY, WAIT, SUSPEND, RUN
    };




    bool roRoundRobin;

    void execute(const PipeLine &now, PipeLine &next) override {

        const ProcessorRegister &p = now.processors[processor_id];
        ProcessorRegister &n = next.processors[processor_id];

        auto ROExecute = [&] () {
            if (roRoundRobin) {
                if (!p.r2r.empty()) {
                    n.ro.output = p.r2r.front();
                    n.r2r.pop();
                    roRoundRobin = false;
                } else if (!p.p2r.empty()) {
                    n.ro.output = p.p2r.front();
                    n.p2r.pop();
                }
            } else {
                if (!p.p2r.empty()) {
                    n.ro.output = p.p2r.front();
                    n.p2r.pop();
                    roRoundRobin = true;
                } else if (!p.r2r.empty()) {
                    n.ro.output = p.r2r.front();
                    n.r2r.pop();
                }
            }

            next.processors[(processor_id + 1) % PROC_NUM].ri.input = n.ro.output;
        };

        RIExecute(p, n);
        if (isRead) PIRExecute(p, n);
        else PIWExecute(p, n);
        ROExecute();
        POExecute(p, n);


    }

    void RIExecute(const ProcessorRegister &p, ProcessorRegister &n) {

        if (p.ri.input.destProcessor == processor_id) {
            if (p.ri.input.type == PacketInfo::BP) {
                n.r2p.push(p.ri.input.label, p.ri.input);
            }
        } else {
            n.r2r.push(p.ri.input);
        }

    }

    FlowState state(int key, const ProcessorRegister& p) {
        if (timeOut.find(key) == timeOut.end() || timeOut[key] == 0) {
            if (p.p2p.contain(key)) {
                return READY;
            } else {
                return RUN;
            }
        } else if (p.p2p.contain(key)) {
            return SUSPEND;
        } else {
            return WAIT;
        }
    }

    void PIRExecute(const ProcessorRegister &p, ProcessorRegister &n) {

        PHV phv  = p.getKey.phv.back();
        array<Key,  READ_TABLE_NUM>        keys = p.getKey.keys      .back();
        array<u32,  READ_TABLE_NUM>   hashValue = p.getKey.hashValue .back();
        array<bool, READ_TABLE_NUM>  readEnable = p.getKey.readEnable.back();
        auto packetFromPipeLine = PacketInfo(processor_id, phv, keys, hashValue, readEnable);

        auto writeSetTimeOut = [&] () {
            if (p.ri.input.type == PacketInfo::WRITE) {
                if (state(p.ri.input.label, p) == WAIT)
                    throw TimeOutSettingErrorException;
                else {
                    dirty[p.ri.input.label] = true;
                    timeOut[p.ri.input.label] = TIME_OUT;
                }
            }
        };

        auto heartBeatActivate = [&] () {
            if (p.ri.input.heartBeat[processor_id]) {
                for (auto& t : timeOut) {
                    if (t.second > 0) {
                        t.second --;
                        FlowState s = state(t.first, p);
                        if (s == READY || s == RUN) {
                            dirty[t.first] = false;
                            n.p2r.push(PacketInfo(processor_id, t.first, PacketInfo::CANCEL_DIRTY));
                        }
                    }
                }
            }
        };

        auto pipeLineScheduling = [&] () {
            if (p.p2p.contain(packetFromPipeLine.label)) {
                n.pi.priority = false;
                n.p2p.push(packetFromPipeLine.label, packetFromPipeLine);
            } else {
                n.pi.priority = true;
                n.pi.output = packetFromPipeLine;
            }
        };


        auto combineR2PAndP2P = [&] () {
            for (const auto& d : dirty) {
                if (state(d.first, p) == READY) {
                    queue<PacketInfo> q;
                    if (p.p2p.q.find(d.first) != p.p2p.q.end()) {
                        q = p.p2p.q.at(d.first);
                    } else {
                        q = {};
                    }
                    n.p2p.q[d.first] = n.r2p.q[d.first];
                    while (!q.empty()){
                        n.p2p.q[d.first].push(q.front());
                        q.pop();
                    } n.r2p.q[d.first] = {};
                }
            }
        };

        writeSetTimeOut();
        heartBeatActivate();
        combineR2PAndP2P();
        pipeLineScheduling();

    };





    void PIWExecute(const ProcessorRegister &now, ProcessorRegister &next) {

    }

    int roundRobin;
    void POExecute(const ProcessorRegister &p, ProcessorRegister &n) {

        PacketInfo matcherInput;
        if (p.pi.priority) {
            matcherInput = p.pi.output;
        } else {
            if (roundRobin >= p.p2p.q.rbegin()->first) roundRobin = 0;
            for (const auto& flow : p.p2p.q) {
                if (p.p2p.contain(flow.first)) {
                    if (roundRobin < flow.first) {
                        roundRobin = flow.first;
                        matcherInput = n.p2p.pop(flow.first);
                        break;
                    }
                }
            }
        }

        n.matcher.hashValue = matcherInput.hashValue;
        n.matcher.readEnableCycleMatch = matcherInput.readEnable;
        n.matcher.phv[0] = matcherInput.phv;
        n.matcher.keyCycleMatch = matcherInput.key;


    }


    void PIWExecute(const ProcessorRegister &now, ProcessorRegister &next) {

    }




};

#endif //RPISA_SW_SCHEDULE_H
