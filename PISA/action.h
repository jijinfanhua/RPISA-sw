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

        auto matchTableConfig = matchTableConfigs[processor_id];
        std::array<bool, MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM> vliw_enabler = {false};
        int action_data_id_base = 0;

        for(int i = 0; i < matchTableConfig.match_table_num; i++) {

            if(matchTableConfig.matchTables[i].type == 1){
                continue;
            }

            if (!now.final_values[i].second) continue;
            // 0 ... 16 ... 32 ... 48 ... 64 ... 80 ... 96 ... 112 .. 128
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            // | action_id |    data1     |    data2     |    data3     |
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            u32 action_id;
            auto final_value = now.final_values[i].first;
            // 判断表中是否有内容
            if(matchTableConfig.matchTables[i].default_action_id == -1) {
                action_id = final_value[0][0] >> 16;
            }
            else {
                action_id = matchTableConfig.matchTables[i].default_action_id;
            }

            // config alus using action_id
            for(int i = 0; i < MAX_PHV_CONTAINER_NUM; i++){
                if(actionConfigs[processor_id].actions[action_id].vliw_enabler[i]){
                    auto alu_config = actionConfigs[processor_id].actions[action_id].alu_configs[i];
                    ALUs[processor_id][i].op = alu_config.op;
                    ALUs[processor_id][i].operand1 = alu_config.operand1;
                    ALUs[processor_id][i].operand2 = alu_config.operand2;
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
        next.hash_values = now.hash_values;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
    }
};

struct ExecuteAction : public Logic {
    ExecuteAction(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override {
        const ExecuteActionRegister & executeActionReg = now->processors[processor_id].executeAction;
        VerifyStateChangeRegister & verifyReg = next->processors[processor_id].verifyState;

        verifyReg.enable1 = executeActionReg.enable1;
        if (!executeActionReg.enable1) {
            return;
        }

        execute_action(executeActionReg, verifyReg);

        // no need to copy phv here !!!!!
//        verifyReg.phv = executeActionReg.phv;
        verifyReg.gateway_guider = executeActionReg.gateway_guider;
        verifyReg.match_table_guider = executeActionReg.match_table_guider;

    }

    void execute_action(const ExecuteActionRegister &now, VerifyStateChangeRegister &next) {
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
                     const ExecuteActionRegister &now, VerifyStateChangeRegister &next) {
        u32 param1 = get_operand_value(operand1, now);
        switch (op) {
            case ALUnit::SET: {
                next.phv[alu_idx] = param1;
                if (now.phv[alu_idx] != next.phv[alu_idx]) {
                    next.phv_changed_tags[alu_idx] = true;
                }
                break;
            }
            case ALUnit::PLUS: {
                u32 param2 = get_operand_value(operand2, now);
                next.phv[alu_idx] = param1 + param2;
                if (now.phv[alu_idx] != next.phv[alu_idx]) {
                    next.phv_changed_tags[alu_idx] = true;
                }
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
            default: break;
        }
        return 0;
    }
};
#endif //RPISA_SW_ACTION_H
