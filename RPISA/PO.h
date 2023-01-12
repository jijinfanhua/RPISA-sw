//
// Created by ASUS on 2023/1/12.
//

#ifndef RPISA_SW_PO_H
#define RPISA_SW_PO_H



#include "../PipeLine.h"


struct PO : public Logic {
    void execute(const PipeLine &now, PipeLine &next) override {
        const ProcessorRegister& p = now.processors[processor_id];
        ProcessorRegister& n = next.processors[processor_id];

        if (p.pir.priority) {
            n.po.ready.push(p.pir.output);
        } else {
            for (const auto& time : p.pir.timeOut) {
                if (time.second == 0 && p.p2p.contain(time.first)) {
                    n.po.ready.push(n.p2p.pop(time.first));
                    break;
                }
            }
        }

        if (!n.po.ready.empty()) {
            n.matcher.hashValue = n.po.ready.front().hashValue;
            n.matcher.readEnableCycleMatch = n.po.ready.front().readEnable;
            n.matcher.phv[0] = n.po.ready.front().phv;
            n.matcher.keyCycleMatch = n.po.ready.front().key;
            n.po.ready.pop();
        } else {
            n.matcher.readEnableCycleMatch = {false};
        }

    }
};

#endif //RPISA_SW_PO_H
