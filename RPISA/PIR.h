//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_PIR_H
#define RPISA_SW_PIR_H

#include "../PipeLine.h"


struct PIR : public Logic {

    void execute(const PipeLine &now, PipeLine &next) override {

        const ProcessorRegister& p = now.processors[processor_id];
        ProcessorRegister& n = next.processors[processor_id];

        PHV phv  = p.getKey.phv.back();
        array<Key,  READ_TABLE_NUM>        keys = p.getKey.keys      .back();
        array<u32,  READ_TABLE_NUM>   hashValue = p.getKey.hashValue .back();
        array<bool, READ_TABLE_NUM>  readEnable = p.getKey.readEnable.back();
        auto x = GetKeyResult(phv, keys, hashValue, readEnable);

        int key  = hashValue[0];
        if (p.p2p.contain(key)) {
            n.pir.priority = false;
            n.p2p.push(key, x);
        } else {
            n.pir.priority = true;
            n.pir.output = x;
        }

    }

};

#endif //RPISA_SW_PIR_H
