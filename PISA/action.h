//
// Created by root on 1/27/23.
//

#ifndef RPISA_SW_ACTION_H
#define RPISA_SW_ACTION_H

#include "../pipeline.h"
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
        std::array<bool, MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM> vliw_enabler = {false};
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
            // stateful (128bit) : set | value1 | value2 | value3
//            set() {
//                phva = value1;
//                phvb = value2;
//                phvc = value3;
//            }
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

            // "or" all of the actions' ALU enabler
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

    void execute(const PipeLine &now, PipeLine &next) override {
        const ExecuteActionRegister & executeActionReg = now.processors[processor_id].executeAction;
        VerifyStateChangeRegister & verifyReg = next.processors[processor_id].verifyState;

        verifyReg.enable1 = executeActionReg.enable1;
        if (!executeActionReg.enable1) {
            return;
        }

        execute_action(executeActionReg, verifyReg);

        verifyReg.phv = executeActionReg.phv;
        verifyReg.gateway_guider = executeActionReg.gateway_guider;
        verifyReg.match_table_guider = executeActionReg.match_table_guider;
        verifyReg.hash_values = executeActionReg.hash_values;
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

        // todo: execute stateful action
        for(int i = MAX_PHV_CONTAINER_NUM ; i < MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM; i++) {
            if (!now.vliw_enabler[i]) {
                continue;
            }
            auto salu = SALUs[processor_id][i - MAX_PHV_CONTAINER_NUM];
            auto left_value = salu.left_value;
            auto operand1 = salu.operand1;
            auto operand2 = salu.operand2;
            std::array<int, 48> sram_ids = salu.sram_ids;
            execute_salu(salu, now, next);
        }
    }

    // todo: wait to finish    SALUnit::Parameter &left_value, SALUnit::Parameter &operand1, SALUnit::Parameter &operand2
    void execute_salu(SALUnit & salu, const ExecuteActionRegister &now, VerifyStateChangeRegister & next) {
        switch (salu.op) {
            case SALUnit::READ: {
                auto left_value = salu.left_value;
                auto operand1 = salu.operand1;
                // the stateful table index; >> to get the "cs"
                // | processor_id | -- | cs | -- | on-chip address |
                auto return_value = SRAMs[processor_id][salu.sram_ids[(now.hash_values[operand1.content.table_idx][0] >> 10)]]
                        .get(now.hash_values[operand1.content.table_idx][0] << 22 >> 22);
                // 0 ... 16 ... 32 ... 48 ... 64 ... 80 ... 96 ... 112 .. 128
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |             |             |             |  reg_value  |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //                                           high           low
                u32 reg_value = return_value[3];
                next.phv[left_value.content.phv_id] = reg_value;
                if (now.phv[left_value.content.phv_id] != next.phv[left_value.content.phv_id]) {
                    next.phv_changed_tags[left_value.content.phv_id] = true;
                }
                break;
            }
            case SALUnit::WRITE: {
                auto left_value = salu.left_value;
                u32 return_value = get_param_value(salu, left_value, now);
                auto operand1 = salu.operand1;
                SRAMs[processor_id][salu.sram_ids[(now.hash_values[operand1.content.table_idx][0] >> 10)]]
                    .set(now.hash_values[operand1.content.table_idx][0] << 22 >> 22, {0, 0, 0, return_value});
                break;
            }
            case SALUnit::RAW: {
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(salu, left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value = get_param_value(salu, operand1, now);
                SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                        .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value});
                break;
            }
            case SALUnit::PRAW:{
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(salu, left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value1 = get_param_value(salu, operand1, now);
                auto operand2 = salu.operand2;
                // header, metadata or constant value
                u32 return_value2 = get_param_value(salu, operand2, now);
                switch (left_value.if_type) {
                    case SALUnit::Parameter::EQ: {
                        if (left_return_value == return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::NEQ: {
                        if (left_return_value != return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::GT: {
                        if (left_return_value > return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::LT: {
                        if (left_return_value < return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::GTE: {
                        if (left_return_value >= return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::LTE: {
                        if (left_return_value <= return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        }
                        break;
                    }
                    default: break;
                }
                break;
            }
            case SALUnit::SUB: {
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(salu, left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value1 = get_param_value(salu, operand1, now);
                auto operand2 = salu.operand2;
                // header, metadata or constant value
                u32 return_value2 = get_param_value(salu, operand2, now);
                switch (left_value.if_type) {
                    case SALUnit::Parameter::EQ: {
                        if (left_return_value == return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value - return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::NEQ: {
                        if (left_return_value != return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value - return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::GT: {
                        if (left_return_value > return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value - return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::LT: {
                        if (left_return_value < return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value - return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::GTE: {
                        if (left_return_value >= return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value - return_value2});
                        }
                        break;
                    }
                    case SALUnit::Parameter::LTE: {
                        if (left_return_value <= return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(int(now.hash_values[left_value.content.table_idx][0] << 22 >> 22), {0, 0, 0, left_return_value - return_value2});
                        }
                        break;
                    }
                    default: break;
                }
                break;
            }
            case SALUnit::IfElseRAW: {
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(salu, left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value1 = get_param_value(salu, operand1, now);
                auto operand2 = salu.operand2;
                // header, metadata or constant value
                u32 return_value2 = get_param_value(salu, operand2, now);
                auto operand3 = salu.operand3;
                // header, metadata or constant value
                u32 return_value3 = get_param_value(salu, operand3, now);
                switch (left_value.if_type) {
                    case SALUnit::Parameter::EQ: {
                        if (left_return_value == return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        } else {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value3});
                        }
                        break;
                    }
                    case SALUnit::Parameter::NEQ: {
                        if (left_return_value != return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        } else {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value3});
                        }
                        break;
                    }
                    case SALUnit::Parameter::GT: {
                        if (left_return_value > return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        } else {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value3});
                        }
                        break;
                    }
                    case SALUnit::Parameter::LT: {
                        if (left_return_value < return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        } else {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value3});
                        }
                        break;
                    }
                    case SALUnit::Parameter::GTE: {
                        if (left_return_value >= return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        } else {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value3});
                        }
                        break;
                    }
                    case SALUnit::Parameter::LTE: {
                        if (left_return_value <= return_value1) {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value2});
                        } else {
                            SRAMs[processor_id][salu.sram_ids[(now.hash_values[left_value.content.table_idx][0] >> 10)]]
                                    .set(now.hash_values[left_value.content.table_idx][0] << 22 >> 22, {0, 0, 0, left_return_value + return_value3});
                        }
                        break;
                    }
                    default: break;
                }
                break;
            }
            case SALUnit::NestedIf:
                break;
        }
    }

    u32 get_param_value(SALUnit & salu, SALUnit::Parameter & param, const ExecuteActionRegister &now) {
        switch (param.type) {
            case SALUnit::Parameter::CONST: {
                return param.content.value;
                break;
            }
            case SALUnit::Parameter::HEADER: {
                return now.phv[param.content.phv_id];
                break;
            }
            case SALUnit::Parameter::ACTION_DATA: {
                return now.action_data_set[param.content.action_data_id].value;
                break;
            }
            case SALUnit::Parameter::REG: {
                return SRAMs[processor_id][salu.sram_ids[(now.hash_values[param.content.table_idx][0] >> 10)]]
                        .get(now.hash_values[param.content.table_idx][0] << 22 >> 22)[3];
            }
            default: break;
        }
        return 0;
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
