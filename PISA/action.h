//
// Created by root on 1/27/23.
//

#ifndef RPISA_SW_ACTION_H
#define RPISA_SW_ACTION_H

#include "../pipeline.h"
#include "../dataplane_config.h"

struct GetAction : public Logic {
    GetAction(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override {
        const GetActionRegister & getActionReg = now->processors[processor_id].getAction;
        ExecuteActionRegister & executeActionReg = next->processors[processor_id].executeAction;

        get_alus(getActionReg, executeActionReg);
    }

    void get_alus(const GetActionRegister & now, ExecuteActionRegister & next) {
        next.enable1 = now.enable1;
        if (!now.enable1) {
            return;
        }

        auto efsmTableConfig = efsmTableConfigs[processor_id];
        std::array<bool, MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM> vliw_enabler = {false};
        int action_data_id_base = 0;

        for(int i = 0; i < efsmTableConfig.efsm_table_num; i++) {
            if (!now.final_values[i].second) continue;
            // 0 ... 16 ... 32 ... 48 ... 64 ... 80 ... 96 ... 112 .. 128
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            // | action_id |    data1     |    data2     |    data3     |
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            auto final_value = now.final_values[i].first;
            u32 action_id = final_value[0][0];
            // 判断表中是否有内容

            // config alus using action_id
            for(int k = 0; k < MAX_PHV_CONTAINER_NUM; k++){
                if(actionConfigs[processor_id].actions[action_id].vliw_enabler[k]){
                    auto alu_config = actionConfigs[processor_id].actions[action_id].alu_configs[k];
                    ALUs[processor_id][k].op = alu_config.op;
                    ALUs[processor_id][k].operand1 = alu_config.operand1;
                    ALUs[processor_id][k].operand2 = alu_config.operand2;
                }
            }

            int para_num = actionConfigs[processor_id].actions[action_id].action_data_num;
            for(int j = 0; j < para_num; j++) {
                if (j <= 2) {
                    next.action_data_set[action_data_id_base + j] =
                    {
                            action_data_id_base + j, 4, 32, final_value[0][j+1]
                    };
                } else {
                    next.action_data_set[action_data_id_base + j] =
                    {
                            action_data_id_base + j, 4, 32, final_value[(para_num - 3) / 4 + 1][(para_num - 3) % 4]
                    };
                }
            }
            action_data_id_base += para_num;

            // "or" all the actions' ALU enabler
            for(int k = 0 ; k < MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM; k++) {
                vliw_enabler[k] = vliw_enabler[k] | actionConfigs[processor_id].actions[action_id].vliw_enabler[k];
            }
        }
        next.vliw_enabler = vliw_enabler;
        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
        next.states = now.states;
        next.registers = now.registers;
    }
};

struct ExecuteAction : public Logic {
    ExecuteAction(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override {
        const ExecuteActionRegister & executeActionReg = now->processors[processor_id].executeAction;
        UpdateStateRegister& updateStateReg = next->processors[processor_id].update;

        execute_action(executeActionReg, updateStateReg);


    }

    void execute_action(const ExecuteActionRegister &now, UpdateStateRegister &next) {
        next.enable1 = now.enable1;
        if (!now.enable1) {
            return;
        }

        // caution: should copy the phv before action!
        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;

        // execute stateless action
        for(int i = 0 ; i < MAX_PHV_CONTAINER_NUM; i++) {
            if (!now.vliw_enabler[i]) {
                continue;
            }
            auto alu = ALUs[processor_id][i];
            auto op = alu.op;
            auto operand1 = alu.operand1;
            auto operand2 = alu.operand2;
            execute_alu(i, op, operand1, operand2, now, next);
        }
    }



    static void execute_alu(int alu_idx, ALUnit::OP op, ALUnit::Parameter operand1, ALUnit::Parameter operand2,
                     const ExecuteActionRegister &now, UpdateStateRegister &next) {
        u32 param1 = get_operand_value(operand1, now);
        switch (op) {
            case ALUnit::SET: {
                next.phv[alu_idx] = param1;
                break;
            }
            case ALUnit::PLUS: {
                u32 param2 = get_operand_value(operand2, now);
                next.phv[alu_idx] = param1 + param2;
                break;
            }
            case ALUnit::NONE:
                break;
        }
    }

    static u32 get_operand_value(ALUnit::Parameter operand, const ExecuteActionRegister& now) {
        switch (operand.type) {
            case ALUnit::Parameter::Type::CONST: return operand.content.value;
            case ALUnit::Parameter::Type::HEADER: return now.phv[operand.content.phv_id];
            case ALUnit::Parameter::Type::ACTION_DATA: return now.action_data_set[operand.content.action_data_id].value;
            case ALUnit::Parameter::Type::STATE: return now.states[operand.content.table_id];
            case ALUnit::Parameter::Type::REGISTER: return now.registers[operand.content.table_register_id.table_id][operand.content.table_register_id.register_id];
            default: break;
        }
        return 0;
    }
};

struct UpdateState: public Logic{
    UpdateState(int id): Logic(id){}

    void execute(const PipeLine* now, PipeLine* next) override {
        const UpdateStateRegister &updateReg = now->processors[processor_id].update;

        if (processor_id == PROCESSOR_NUM - 1) {
            // empty cycle
            return;
        } else if (processor_id != PROCESSOR_NUM - 1) {
            // not the last -> only one cycle
            if (updateReg.enable1) {
                GetKeysRegister &getKeysReg = next->processors[processor_id + 1].getKeys;
                getKeysReg.enable1 = updateReg.enable1;
                getKeysReg.match_table_guider = updateReg.match_table_guider;
                getKeysReg.gateway_guider = updateReg.gateway_guider;
                getKeysReg.phv = updateReg.phv;
            }
        }

        update_state(updateReg);
    }

    void update_state(const UpdateStateRegister& now){

    }
};
#endif //RPISA_SW_ACTION_H
