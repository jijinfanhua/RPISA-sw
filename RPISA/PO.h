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



    }
};

#endif //RPISA_SW_PO_H
