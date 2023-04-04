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
        for(int i = 0; i < MAX_SALU_NUM; i++){
            if(salu_id[processor_id][i] != 0){
                vliw_enabler[salu_id[processor_id][i]] = true;
            }
        }
        for(int i = 0; i < num_of_stateful_tables[processor_id]; i++){
            // 处理带状态表
            auto match_table_id = stateful_table_ids[processor_id][i];
            if(now.final_values[match_table_id].second){
                next.salu_value_data_set[i] = now.final_values[match_table_id].first[0];
            }
            next.salu_compare_result[salu_id[processor_id][match_table_id]] = now.final_values[match_table_id].second;
        }

        for(int i = 0; i < matchTableConfig.match_table_num; i++) {

            if(matchTableConfig.matchTables[i].type == 1){
                continue;
            }

            if (!now.final_values[i].second) continue;
            // 0 ... 16 ... 32 ... 48 ... 64 ... 80 ... 96 ... 112 .. 128
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            // | action_id |    data1     |    data2     |    data3     |
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            // todo: debug
            if(processor_id == 2){
                cout << "debug" << endl;
            }

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
            for(int j = 0; j < MAX_PHV_CONTAINER_NUM; j++){
                if(actionConfigs[processor_id].actions[action_id].vliw_enabler[j]){
                    auto alu_config = actionConfigs[processor_id].actions[action_id].alu_configs[j];
                    ALUs[processor_id][j].op = alu_config.op;
                    ALUs[processor_id][j].operand1 = alu_config.operand1;
                    ALUs[processor_id][j].operand2 = alu_config.operand2;
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
        next.stateful_matched_hash_way = now.stateful_matched_hash_way;
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

        // done: execute stateful action
        for(int i = MAX_PHV_CONTAINER_NUM ; i < MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM; i++) {
            if (!now.vliw_enabler[i]) {
                continue;
            }
            auto salu = SALUs[processor_id][i - MAX_PHV_CONTAINER_NUM];
            auto return_value = execute_salu(salu, now, next);
            salu_write(return_value, salu.return_value, processor_id, now, next);
        }
    }

    void salu_write(u32 value_to_write, SALUnit::Parameter to_write, int processor_id, const ExecuteActionRegister& now, VerifyStateChangeRegister& next){
        if(to_write.type == SALUnit::Parameter::Type::REG){
            auto table_idx = to_write.content.table_idx;
            auto table_id = stateful_table_ids[processor_id][table_idx];
            auto match_table = matchTableConfigs[processor_id].matchTables[table_id];
            // 如果 compare 成功，则写回成功的位置；如失败，则选择一路可写的写回
            int way_to_write = now.stateful_matched_hash_way[table_id];
            if(way_to_write == -1){
                // compare failed
                for(int i = 0; i < match_table.number_of_hash_ways; i++){
                    auto hash_value = now.hash_values[table_id][i];
                    auto key_sram_id = match_table.key_sram_index_per_hash_way[i][hash_value >> 10];
                    b128 key = SRAMs[processor_id][key_sram_id].get(hash_value << 22 >> 22);
                    if(key[0] == 0 && key[1] == 0 && key[2] == 0 && key[3] == 0){
                        // using all zero to verify empty entry
                        way_to_write = i;
                        break;
                    }
                }
                // assume there is empty entry to write; otherwise write fail
            }
            auto hash_value = now.hash_values[table_id][way_to_write];
            auto key_sram_id = match_table.key_sram_index_per_hash_way[way_to_write][hash_value >> 10];
            auto value_sram_id = match_table.value_sram_index_per_hash_way[way_to_write][hash_value >> 10];

            // no matter compare success / failed, always write key back. (if success, new value and old value are the same)
            b128 key = u64_to_u16_array(u32_to_u64(now.phv[flow_id_in_phv[0]], now.phv[flow_id_in_phv[1]]));
            SRAMs[processor_id][key_sram_id].set(hash_value << 22 >> 22, key);

            b128 value = SRAMs[processor_id][value_sram_id].get(hash_value << 22 >> 22);
            value[to_write.value_idx] = value_to_write;
            SRAMs[processor_id][value_sram_id].set(hash_value << 22 >> 22, value);
        }
        else if(to_write.type == SALUnit::Parameter::Type::HEADER){
            next.phv[to_write.content.phv_id] = value_to_write;
            if(now.phv[to_write.content.phv_id] != next.phv[to_write.content.phv_id]){
                next.phv_changed_tags[to_write.content.phv_id] = true;
            }
        }
    }

    // todo: wait to finish    SALUnit::Parameter &left_value, SALUnit::Parameter &operand1, SALUnit::Parameter &operand2
    u32 execute_salu(SALUnit & salu, const ExecuteActionRegister &now, VerifyStateChangeRegister & next) {
        switch (salu.op) {
            case SALUnit::READ: {
                auto left_value = salu.left_value;
                auto operand1 = salu.operand1;
                // the stateful table index; >> to get the "cs"
                // | processor_id | -- | cs | -- | on-chip address |
                u32 read_value = now.salu_value_data_set[operand1.content.table_idx][operand1.value_idx];

                next.phv[left_value.content.phv_id] = read_value;
                if (now.phv[left_value.content.phv_id] != next.phv[left_value.content.phv_id]) {
                    next.phv_changed_tags[left_value.content.phv_id] = true;
                }
                return get_return_value(salu, salu.return_value_from, true, now);
            }
            case SALUnit::WRITE: {
                auto left_value = salu.left_value;
                u32 write_value = get_param_value(left_value, now);
                auto operand1 = salu.operand1;
                // todo: 传hash way进来，先读在修改最后写
                salu_write(write_value, operand1, processor_id, now, next);
                return get_return_value(salu, salu.return_value_from, true, now);
            }
            case SALUnit::RAW: {
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value = get_param_value(operand1, now);

                salu_write(left_return_value + return_value, left_value, processor_id, now, next);
                return get_return_value(salu, salu.return_value_from, true, now);
            }
            case SALUnit::PRAW:{
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value1 = get_param_value(operand1, now);
                auto operand2 = salu.operand2;
                // header, metadata or constant value
                u32 return_value2 = get_param_value(operand2, now);
                switch (left_value.if_type) {
                    case SALUnit::Parameter::COMPARE_EQ: {
                        if(now.salu_compare_result[salu.salu_id]){
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }

                    case SALUnit::Parameter::EQ: {
                        if (left_return_value == return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::NEQ: {
                        if (left_return_value != return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::GT: {
                        if (left_return_value > return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::LT: {
                        if (left_return_value < return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::GTE: {
                        if (left_return_value >= return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::LTE: {
                        if (left_return_value <= return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    default: break;
                }
                return get_return_value(salu, salu.return_value_from, true, now);
            }
            case SALUnit::SUB: {
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value1 = get_param_value(operand1, now);
                auto operand2 = salu.operand2;
                // header, metadata or constant value
                u32 return_value2 = get_param_value(operand2, now);
                switch (left_value.if_type) {
                    case SALUnit::Parameter::COMPARE_EQ: {
                        if(now.salu_compare_result[salu.salu_id]){
                            salu_write(left_return_value - return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }

                    case SALUnit::Parameter::EQ: {
                        if (left_return_value == return_value1) {
                            salu_write(left_return_value - return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::NEQ: {
                        if (left_return_value != return_value1) {
                            salu_write(left_return_value - return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::GT: {
                        if (left_return_value > return_value1) {
                            salu_write(left_return_value - return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::LT: {
                        if (left_return_value < return_value1) {
                            salu_write(left_return_value - return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::GTE: {
                        if (left_return_value >= return_value1) {
                            salu_write(left_return_value - return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::LTE: {
                        if (left_return_value <= return_value1) {
                            salu_write(left_return_value - return_value2, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    default: break;
                }
                return get_return_value(salu, salu.return_value_from, true, now);
            }
            case SALUnit::IfElseRAW: {
                auto left_value = salu.left_value;
                // reg value
                u32 left_return_value = get_param_value(left_value, now);
                auto operand1 = salu.operand1;
                // header, metadata or constant value
                u32 return_value1 = get_param_value(operand1, now);
                auto operand2 = salu.operand2;
                // header, metadata or constant value
                u32 return_value2 = get_param_value(operand2, now);
                auto operand3 = salu.operand3;
                // header, metadata or constant value
                u32 return_value3 = get_param_value(operand3, now);
                switch (left_value.if_type) {
                    case SALUnit::Parameter::COMPARE_EQ: {
                        if (now.salu_compare_result[salu.salu_id]){
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, true, now);
                        } else {
                            salu_write(left_return_value + return_value3, left_value, processor_id, now, next);
                        }
                        break;
                    }

                    case SALUnit::Parameter::EQ: {
                        if (left_return_value == return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, true, now);
                        } else {
                            salu_write(left_return_value + return_value3, left_value, processor_id, now, next);
                        }
                        break;
                    }
                    case SALUnit::Parameter::NEQ: {
                        if (left_return_value != return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, true, now);
                        } else {
                            salu_write(left_return_value + return_value3, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, false, now);
                        }
                        break;
                    }
                    case SALUnit::Parameter::GT: {
                        if (left_return_value > return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, true, now);
                        } else {
                            salu_write(left_return_value + return_value3, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, false, now);
                        }
                        break;
                    }
                    case SALUnit::Parameter::LT: {
                        if (left_return_value < return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, true, now);
                        } else {
                            salu_write(left_return_value + return_value3, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, false, now);
                        }
                        break;
                    }
                    case SALUnit::Parameter::GTE: {
                        if (left_return_value >= return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, true, now);
                        } else {
                            salu_write(left_return_value + return_value3, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, false, now);
                        }
                        break;
                    }
                    case SALUnit::Parameter::LTE: {
                        if (left_return_value <= return_value1) {
                            salu_write(left_return_value + return_value2, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, true, now);
                        } else {
                            salu_write(left_return_value + return_value3, left_value, processor_id, now, next);
                            return get_return_value(salu, salu.return_value_from, false, now);
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

    u32 get_return_value(SALUnit& salu, SALUnit::ReturnValueFrom value_from, bool compare_result, const ExecuteActionRegister &now){
        if(compare_result){
            switch (value_from.type) {
                case SALUnit::ReturnValueFrom::Type::CONST: {
                    return value_from.content.value;
                }
                case SALUnit::ReturnValueFrom::Type::HEADER: {
                    return now.phv[value_from.content.phv_id];
                }
                case SALUnit::ReturnValueFrom::Type::REG: {
                    return now.salu_value_data_set[value_from.content.table_idx][value_from.value_idx];
                }
                case SALUnit::ReturnValueFrom::Type::ACTION_DATA: {
                    return now.action_data_set[value_from.content.action_data_id].value;
                }
                case SALUnit::ReturnValueFrom::Type::OP1: {
                    return get_param_value(salu.operand1, now);
                }
                case SALUnit::ReturnValueFrom::Type::OP2: {
                    return get_param_value(salu.operand2, now);
                }
                case SALUnit::ReturnValueFrom::Type::OP3: {
                    return get_param_value(salu.operand3, now);
                }
                case SALUnit::ReturnValueFrom::Type::LEFT: {
                    return get_param_value(salu.left_value, now);
                }
            }
        }
        else {
            switch (value_from.false_type) {
                case SALUnit::ReturnValueFrom::Type::CONST: {
                    return value_from.false_content.value;
                }
                case SALUnit::ReturnValueFrom::Type::HEADER: {
                    return now.phv[value_from.false_content.phv_id];
                }
                case SALUnit::ReturnValueFrom::Type::REG: {
                    return now.salu_value_data_set[value_from.false_content.table_idx][value_from.false_value_idx];
                }
                case SALUnit::ReturnValueFrom::Type::ACTION_DATA: {
                    return now.action_data_set[value_from.false_content.action_data_id].value;
                }
                case SALUnit::ReturnValueFrom::Type::OP1: {
                    return get_param_value(salu.operand1, now);
                }
                case SALUnit::ReturnValueFrom::Type::OP2: {
                    return get_param_value(salu.operand2, now);
                }
                case SALUnit::ReturnValueFrom::Type::OP3: {
                    return get_param_value(salu.operand3, now);
                }
                case SALUnit::ReturnValueFrom::Type::LEFT: {
                    return get_param_value(salu.left_value, now);
                }
            }
        }
    }

    u32 get_param_value(SALUnit::Parameter & param, const ExecuteActionRegister &now) {
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
                return now.salu_value_data_set[param.content.table_idx][param.value_idx];
                break;
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
