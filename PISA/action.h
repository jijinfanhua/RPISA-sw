//
// Created by root on 1/27/23.
//

#ifndef RPISA_SW_ACTION_H
#define RPISA_SW_ACTION_H

#include "../PipeLine.h"
#include "../dataplane_config.h"

struct GetAction : public Logic {
    GetAction(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {
        const GetActionRegister & getActionReg = now.processors[processor_id].getAction;
        ExecuteActionRegister & executeActionReg = next.processors[processor_id].executeAction;

        get_alus(getActionReg, executeActionReg);
    }

    void get_alus(const GetActionRegister & now, ExecuteActionRegister & next) {
        next.enable1 = now.enable1;
        if (!now.enable1) {
            return;
        }

        auto matchTableConfig = matchTableConfigs[processor_id];
        std::array<bool, MAX_PHV_CONTAINER_NUM> vliw_enabler = {false};
        int action_data_id_base = 0;
        for(int i = 0; i < matchTableConfig.match_table_num; i++) {
            if (!now.final_values[i].second) continue;
            // 0 ... 16 ... 32 ... 48 ... 64 ... 80 ... 96 ... 112 .. 128
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            // | action_id |    data1     |    data2     |    data3     |
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            auto final_value = now.final_values[i].first;
            u32 action_id = final_value[0][0] >> 16;
            int para_num = actionConfigs[processor_id].actions[action_id].action_data_num;
            // 0-0.1  1-0.2  2-0.3  3-1.0  4-1.1  5-1.2  6-1.3  7-2.0   8-2.1  9-2.2  10-2.3  11-3.0
            // 0-0    3-1    7-2   11-3
            for(int j = 0; j < para_num; j++) {
                if (j <= 2) {
                    actionConfigs[processor_id].actions[action_id].actionDatas[j] = {
                            action_data_id_base + j, 4, 32, final_value[0][j+1]
                    };
                } else {
                    actionConfigs[processor_id].actions[action_id].actionDatas[j] = {
                            action_data_id_base + j, 4, 32, final_value[(para_num - 3) / 4 + 1][(para_num - 3) % 4]
                    };
                }
            }
            action_data_id_base += para_num;

            // "or" all of the actions' ALU enabler
            for(int k = 0 ; k < MAX_PHV_CONTAINER_NUM; k++) {
                vliw_enabler[k] = vliw_enabler[k] | actionConfigs[processor_id].actions[action_id].vliw_enabler[k];
            }
        }
        next.vliw_enabler = vliw_enabler;
        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
    }
};

struct ExecuteAction : public Logic {
    ExecuteAction(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {

    }
};

#endif //RPISA_SW_ACTION_H
