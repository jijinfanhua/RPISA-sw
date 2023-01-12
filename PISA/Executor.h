//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_EXECUTOR_H
#define RPISA_SW_EXECUTOR_H
#include "../defs.h"


const int INSTRUCTION_NUM = 8;
//const int ALUWidth = 128;
//const int size = ALUWidth / 32;
//using ALUInt = array<u32, size>;
using ALUInt = u32;
typedef ALUInt (*ALU)(ALUInt, ALUInt);
struct Instruction {

    ALU op;

    struct Parameter {
        enum Type {
            CONST, HEADER
        } type;
        union {
            ALUInt value;
            u32 id;
        } content ;
    } a, b;

    u32 id;



};
using VLIW = array<Instruction, INSTRUCTION_NUM>;


struct PipeLine::ExecutorRegister {

    PHV phvIF;
    array<bool, READ_TABLE_NUM> readEnable;
    array<bool, READ_TABLE_NUM> compare;
    array<Key , READ_TABLE_NUM> value;
    array<VLIW, READ_TABLE_NUM> vliw;


    PHV phvID;
    array<array<ALU, HEADER_NUM> , READ_TABLE_NUM> alu;
    array<array<pair<ALUInt, ALUInt>, HEADER_NUM>, READ_TABLE_NUM> parameter;
    array<array<bool, HEADER_NUM>, READ_TABLE_NUM> writeEnableEX;

    PHV phvEX;
    array<array<ALUInt , HEADER_NUM>, READ_TABLE_NUM> res;
    array<array<bool, HEADER_NUM>, READ_TABLE_NUM> writeEnableWB;

    PHV phvWB;


};


struct ExecutorConfig {
    map<int, VLIW> vliwMap;
    Key defaultActionVLIW;
};


struct Executor : public Logic {

    array<ExecutorConfig, READ_TABLE_NUM> executorConfig;

    void execute(const PipeLine &now, PipeLine &next) override {
        PipeLine::ExecutorRegister& nextExecutor  = next.processors[processor_id].executor;
        const PipeLine::ExecutorRegister& executor = now.processors[processor_id].executor;
        nextExecutor.writeEnableWB = executor.writeEnableEX;
    }

    void instructionFetch(const PipeLine::ExecutorRegister& now, PipeLine::ExecutorRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            int key = now.value[i][0] & 0x1ff;
            next.vliw[i] = executorConfig[i].vliwMap[key];
        }
    }

    void instructionDecode(const PipeLine::ExecutorRegister& now, PipeLine::ExecutorRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            for (int j = 0; j < INSTRUCTION_NUM; j++) {
                Instruction instruction = now.vliw[i][j];
                next.writeEnableEX[i][instruction.id] = true;
                next.parameter[i][instruction.id] = {
                        getParameter(instruction.a, now.phvID),
                        getParameter(instruction.b, now.phvID)
                };
                next.alu[i][instruction.id] = instruction.op;
            }
        }
    }

    ALUInt getParameter(const Instruction::Parameter& parameter, const PHV& phv) {
        switch (parameter.type) {
            case Instruction::Parameter::CONST:
                return parameter.content.value;
            case Instruction::Parameter::HEADER:
                return phv[parameter.content.id];
        }
    }

    void instructionExecute(const PipeLine::ExecutorRegister& now, PipeLine::ExecutorRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            for (int j = 0; j < HEADER_NUM; j++) {
                if (now.writeEnableEX[i][j]) {
                    next.res[i][j] = now.alu[i][j](now.parameter[i][j].first, now.parameter[i][j].second);
                }
            }
        }
    }

    void writeBack(const PipeLine::ExecutorRegister& now, PipeLine::ExecutorRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            for (int j = 0; j < HEADER_NUM; j++) {
                if (now.writeEnableWB[i][j]) {
                    next.phvWB[j] = now.res[i][j];
                }
            }
        }
    }

};


#endif //RPISA_SW_EXECUTOR_H
