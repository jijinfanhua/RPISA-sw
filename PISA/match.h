//
// Created by root on 1/23/23.
//

#ifndef RPISA_SW_MATCH_H
#define RPISA_SW_MATCH_H

#include "../pipeline.h"
#include "../dataplane_config.h"

struct GetKey : public Logic
{
    GetKey(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const GetKeysRegister &getKey = now->processors[processor_id].getKeys;
        GatewayRegister &nextGateway = next->processors[processor_id].gateway[0];

        getKeyFromPHV(getKey, nextGateway);
    }

    void getKeyFromPHV(const GetKeysRegister &now, GatewayRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }
        GetKeyConfig getKeyConfig = getKeyConfigs[processor_id];
        for (int i = 0; i < getKeyConfig.used_container_num; i++)
        {
            GetKeyConfig::UsedContainer2MatchFieldByte it = getKeyConfig.used_container_2_match_field_byte[i];
            int id = it.used_container_id;
            if (it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U8)
            {
                next.key[it.match_field_byte_ids[0]] = now.phv[id];
            }
            else if (it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U16)
            {
                next.key[it.match_field_byte_ids[0]] = now.phv[id] << 16 >> 24;
                next.key[it.match_field_byte_ids[1]] = now.phv[id] << 24 >> 24;
            }
            else
            {
                next.key[it.match_field_byte_ids[0]] = now.phv[id] >> 24;
                next.key[it.match_field_byte_ids[1]] = now.phv[id] << 8 >> 24;
                next.key[it.match_field_byte_ids[2]] = now.phv[id] << 16 >> 24;
                next.key[it.match_field_byte_ids[3]] = now.phv[id] << 24 >> 24;
            }
        }

        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
    }
};

struct Gateway : public Logic
{
    Gateway(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const GatewayRegister &gatewayReg = now->processors[processor_id].gateway[0];
        GatewayRegister &nextPipeGatewayReg = next->processors[processor_id].gateway[1];
        const GatewayRegister &nextGatewayReg = now->processors[processor_id].gateway[1];
        HashRegister &hashReg = next->processors[processor_id].hashes[0];

        gateway_first_cycle(gatewayReg, nextPipeGatewayReg);
        gateway_second_cycle(nextGatewayReg, hashReg);
    }

    void gateway_first_cycle(const GatewayRegister &now, GatewayRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }

        array<bool, MAX_GATEWAY_NUM> gate_res = {false};
        // copy the gateway enabler from bus: enable which gates of 16.
        for (int i = 0; i < MAX_GATEWAY_NUM; i++)
        {
            if (now.gateway_guider[processor_id * 16 + i])
            {
                gate_res[i] = get_gate_res(now, gatewaysConfigs[processor_id].gates[i]);
            }
            next.gate_res[i] = gate_res[i];
        }

        next.phv = now.phv;
        next.key = now.key;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
    }

    void gateway_second_cycle(const GatewayRegister &now, HashRegister &next)
    {
        next.enable1 = now.enable1;
        if(!now.enable1){
            return;
        }
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;

        auto gatewayConfig = gatewaysConfigs[processor_id];
        for(auto mask: gatewayConfig.masks){
            std::array<bool, 16> after_mask = {false};
            for(int i = 0; i < MAX_PARALLEL_MATCH_NUM; i++){
                after_mask[i] = mask[i] & now.gate_res[i];
            }
            u32 res = bool_array_2_u32(after_mask);
            if(gatewayConfig.gateway_res_2_match_tables.find(res) != gatewayConfig.gateway_res_2_match_tables.end()){
                auto match_bitmap = gatewayConfig.gateway_res_2_match_tables[res];
                auto gateway_bitmap = gatewayConfig.gateway_res_2_gates[res];
                // get the newest match table guider and gateway guider
                for (int i = processor_id * 16; i < MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM; i++)
                {
                    next.match_table_guider[i] = now.match_table_guider[i] | match_bitmap[i];
                    next.gateway_guider[i] = now.gateway_guider[i] | gateway_bitmap[i];
                }
            }
        }
        next.key = now.key;
        next.phv = now.phv;
    }

    static u32 bool_array_2_u32(const array<bool, 16> bool_array)
    {
        u32 sum = 0;
        for (int i = 0; i < 16; i++)
        {
            if(bool_array[i]) {
                sum += (1 << (15 - i));
            }
        }
        return sum;
    }

    static bool get_gate_res(const GatewayRegister &now, GatewaysConfig::Gate gate)
    {
        auto op = gate.op;
        auto operand1 = get_operand_value(now, gate.operand1);
        auto operand2 = get_operand_value(now, gate.operand2);
        switch (op)
        {
        case GatewaysConfig::Gate::OP::EQ: {
            return operand1 == operand2;
        }
        case GatewaysConfig::Gate::OP::GT: {
            return operand1 > operand2;
        }
        case GatewaysConfig::Gate::OP::LT: {
            return operand1 < operand2;
        }
        case GatewaysConfig::Gate::OP::GTE:{
            return operand1 >= operand2;
        }
        case GatewaysConfig::Gate::OP::LTE: {
            return operand1 <= operand2;
        }
        case GatewaysConfig::Gate::OP::NEQ: {
            return operand1 != operand2;
        }
        default:
            break;
        }
        return false;
    }

    static u32 get_operand_value(const GatewayRegister &now, GatewaysConfig::Gate::Parameter operand)
    {
        switch (operand.type)
        {
        case GatewaysConfig::Gate::Parameter::CONST:
            return operand.content.value;
        case GatewaysConfig::Gate::Parameter::HEADER:
        {
            u32 tmp = 0;
            int len = operand.content.operand_match_field_byte.len;
            // get byte from match_field and compute the result
            for (int i = 0; i < len; i++)
            {
                tmp += now.key[operand.content.operand_match_field_byte.match_field_byte_ids[i]] << (len - 1 - i) * 8;
            }
            return tmp;
        }
        default:
            return 0;
            break;
        }
    }
};

struct GetHash : public Logic
{
    GetHash(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const HashRegister &hashReg = now->processors[processor_id].hashes[0];
        HashRegister &nextHashReg = next->processors[processor_id].hashes[1];
        PIRegister &piRegister = next->processors[processor_id].pi[0];

        get_hash(hashReg, nextHashReg);
        for (int i = 1; i < HASH_CYCLE - 1; i++)
        {
            get_left_hash(now->processors[processor_id].hashes[i], next->processors[processor_id].hashes[i + 1]);
        }

        const HashRegister &lastHashReg = now->processors[processor_id].hashes[HASH_CYCLE - 1];

        piRegister.enable1 = lastHashReg.enable1;
        if (!lastHashReg.enable1)
        {
            return;
        }
        piRegister.phv = lastHashReg.phv;
        //        getAddressRegister.key = lastHashReg.key;
        piRegister.hash_values = lastHashReg.hash_values;
        piRegister.match_table_guider = lastHashReg.match_table_guider;
        piRegister.gateway_guider = lastHashReg.gateway_guider;
        piRegister.match_table_keys = lastHashReg.match_table_keys;
    }

    static void get_left_hash(const HashRegister &now, HashRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }

        next.key = now.key;
        next.phv = now.phv;
        next.match_table_keys = now.match_table_keys;
        next.hash_values = now.hash_values;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
    }

    void get_hash(const HashRegister &now, HashRegister &next)
    {
        auto phv = now.phv;
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }
        auto matchTableConfig = matchTableConfigs[processor_id];
        for (int i = 0; i < matchTableConfig.match_table_num; i++)
        {
            // operate stateless table
            auto match_table = matchTableConfig.matchTables[i];
            std::array<u32, 32> match_table_key{};
            for (int j = 0; j < match_table.match_field_byte_len; j++)
            {
                match_table_key[j] = now.key[match_table.match_field_byte_ids[j]];
            }
            if (match_table.type == 0)
            {
                cal_hash(i, match_table_key, match_table.match_field_byte_len, match_table.number_of_hash_ways,
                         match_table.hash_bit_per_way, match_table.hash_bit_sum, next);
                next.match_table_keys[i] = match_table_key;
            }
            else
            {
                continue;
            }
        }

        for (int i = 0; i < num_of_stateful_tables[processor_id]; i++)
        {
            // operate stateful table
            auto match_table = matchTableConfig.matchTables[stateful_table_ids[processor_id][i]];
            std::array<u32, 32> match_table_key{};
            for (int j = 0; j < match_table.match_field_byte_len; j++)
            {
                match_table_key[j] = now.key[match_table.match_field_byte_ids[j]];
            }
            // done: normal hash, but save hash value in particular position into phv
            // done: translate 128 bit hash value into 64 bit, low 16 bit per 32 bit, save it into two particular phvs
            cal_hash(i, match_table_key, match_table.match_field_byte_len, match_table.number_of_hash_ways,
                     match_table.hash_bit_per_way, match_table.hash_bit_sum, next);
            b128 hash_values = next.hash_values[i];
            u64 hash_value = u16_array_to_u64(hash_values);

            next.match_table_keys[i] = match_table_key;
            // high 32 bit
            phv[phv_id_to_save_hash_value[processor_id][i][0]] = hash_value >> 32;
            // low 32 bit
            phv[phv_id_to_save_hash_value[processor_id][i][1]] = hash_value << 32 >> 32;
        }

        next.key = now.key;
        next.phv = phv;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
    }

    static void cal_hash(int table_id, const std::array<u32, 32> &match_table_key, int match_field_byte_len,
                         int number_of_hash_ways, std::array<int, 4> hash_bit_per_way, int hash_bit_sum, HashRegister &next)
    {
        u64 hash_value = 0;
        for (int i = 0; i < match_field_byte_len; i++)
        {
            hash_value += ((u64)match_table_key[i] << 32) + match_table_key[i];
        }

        // get up to 4 ways hash value to register
        for (int i = 0; i < number_of_hash_ways; i++)
        {
            int left_shift = 0;
            int right_shift = 0;
            u32 high_bit = 0;
            if (hash_bit_per_way[i] > 10)
            {
                if (i != 0)
                {
                    for (int j = 0; j < i; j++)
                    {
                        left_shift += (hash_bit_per_way[j] - 10);
                    }
                    left_shift += (64 - hash_bit_sum);
                }
                else
                {
                    left_shift += (64 - hash_bit_sum);
                }
                right_shift = 64 - left_shift - (hash_bit_per_way[i] - 10);
                high_bit = u32(hash_value << left_shift >> left_shift >> right_shift << 10);
            }
            else
            {
                high_bit = 0;
            }

            right_shift = (number_of_hash_ways - 1 - i) * 10;
            left_shift = 64 - right_shift - 10;
            u32 low_bit = (hash_value << left_shift >> (left_shift + right_shift));

            // set hash value to next cycle's register
            next.hash_values[table_id][i] = high_bit + low_bit;
        }
    }
};

struct GetAddress : public Logic
{
    GetAddress(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const GetAddressRegister &getAddressRegister = now->processors[processor_id].getAddress;
        MatchRegister &matchRegister = next->processors[processor_id].match;

        get_sram_columns(getAddressRegister, matchRegister);
    }

    void get_sram_columns(const GetAddressRegister &now, MatchRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }
        auto matchTableConfig = matchTableConfigs[processor_id];

        if (now.backward_pkt)
        {
            // done: only lookup the stateful table using hash_value
            for (int i = 0; i < num_of_stateful_tables[processor_id]; i++)
            {

                int stateful_table_id = stateful_table_ids[processor_id][i];
                auto match_table = matchTableConfig.matchTables[stateful_table_id];
                // done: hash value is 64 bit, translate it into b128, 16 in per 32 bit
                u64 hash_value = u32_to_u64(phv_id_to_save_hash_value[processor_id][i][0], phv_id_to_save_hash_value[processor_id][i][1]);
                b128 hash_values = u64_to_u16_array(hash_value);

                for (int j = 0; j < match_table.number_of_hash_ways; j++)
                {
                    // done: up to four ways
                    u32 start_index = (hash_values[j] >> 10);
                    next.on_chip_addrs[stateful_table_id][j] = (hash_values[j] << 22 >> 22); // u64
                    for (int k = 0; k < match_table.key_width; k++)
                    {
                        next.key_sram_columns[stateful_table_id][j][k] = match_table.key_sram_index_per_hash_way[j][start_index + k];
                    }
                    for (int k = 0; k < match_table.value_width; k++)
                    {
                        next.value_sram_columns[stateful_table_id][j][k] = match_table.value_sram_index_per_hash_way[j][start_index + k];
                    }
                }
            }

            next.phv = now.phv;
            next.match_table_keys = now.match_table_keys;
            next.match_table_guider = now.match_table_guider;
            next.gateway_guider = now.gateway_guider;
            next.backward_pkt = true;

            return;
        }

        // get the sram indexes per way for each match table
        for (int i = 0; i < matchTableConfig.match_table_num; i++)
        {
            auto hash_values = now.hash_values[i];
            auto match_table = matchTableConfig.matchTables[i];
            for (int j = 0; j < match_table.number_of_hash_ways; j++)
            {
                u32 start_key_index = (hash_values[j] >> 10) * match_table.key_width;
                u32 start_value_index = (hash_values[j] >> 10) * match_table.value_width;
                next.on_chip_addrs[i][j] = (hash_values[j] << 22 >> 22);
                for (int k = 0; k < match_table.key_width; k++)
                {
                    next.key_sram_columns[i][j][k] = match_table.key_sram_index_per_hash_way[j][start_key_index + k];
                }
                for (int k = 0; k < match_table.value_width; k++)
                {
                    next.value_sram_columns[i][j][k] = match_table.value_sram_index_per_hash_way[j][start_value_index + k];
                }
            }
        }
        next.phv = now.phv;
        next.hash_values = now.hash_values;
        next.match_table_keys = now.match_table_keys;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
    }
};

struct Matches : public Logic
{
    Matches(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const MatchRegister &matchRegister = now->processors[processor_id].match;
        CompareRegister &compareRegister = next->processors[processor_id].compare;

        get_value_to_compare(matchRegister, compareRegister);
    }

    void get_value_to_compare(const MatchRegister &now, CompareRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }

        if (now.backward_pkt)
        {
            for (int i = 0; i < num_of_stateful_tables[processor_id]; i++)
            {

                int stateful_table_id = stateful_table_ids[processor_id][i];
                auto match_table = matchTableConfigs[processor_id].matchTables[stateful_table_id];
                for (int j = 0; j < match_table.number_of_hash_ways; j++)
                {
                    // done: four ways
                    for (int k = 0; k < match_table.value_width; k++)
                    {
                        next.obtained_values[stateful_table_id][j][k] = SRAMs[processor_id][now.value_sram_columns[stateful_table_id][j][k]]
                                                                            .get(now.on_chip_addrs[stateful_table_id][j]);
                    }
                }
            }

            next.phv = now.phv;
            next.match_table_guider = now.match_table_guider;
            next.gateway_guider = now.gateway_guider;
            next.backward_pkt = true;
            return;
        }

        auto matchTableConfig = matchTableConfigs[processor_id];
        for (int i = 0; i < matchTableConfig.match_table_num; i++)
        {
            // follow match table guider
            if (!now.match_table_guider[processor_id * 16 + i])
                continue;
            auto match_table = matchTableConfig.matchTables[i];
            for (int j = 0; j < match_table.number_of_hash_ways; j++)
            {
                for (int k = 0; k < match_table.key_width; k++)
                {
                    next.obtained_keys[i][j][k] = SRAMs[processor_id][now.key_sram_columns[i][j][k]].get(now.on_chip_addrs[i][j]);
                }
                for (int k = 0; k < match_table.value_width; k++)
                {
                    next.obtained_values[i][j][k] = SRAMs[processor_id][now.value_sram_columns[i][j][k]].get(now.on_chip_addrs[i][j]);
                }
            }
        }

        next.phv = now.phv;
        next.hash_values = now.hash_values;
        next.match_table_keys = now.match_table_keys;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
    }
};

struct Compare : public Logic
{
    Compare(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const CompareRegister &compareReg = now->processors[processor_id].compare;
        GetActionRegister &getActionReg = next->processors[processor_id].getAction;

        get_action(compareReg, getActionReg);
    }

    void get_action(const CompareRegister &now, GetActionRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }
        auto matchTableConfig = matchTableConfigs[processor_id];

        for (int i = 0; i < matchTableConfig.match_table_num; i++)
        {
            if (!now.match_table_guider[processor_id * 16 + i])
            {
                next.final_values[i].second = false;
                continue;
            }
            auto match_table = matchTableConfig.matchTables[i];
            if (match_table.type == 1)
            {
                // done: four ways; fit the salu
                int found_flag = 0;
                b128 key_to_compare = u64_to_u16_array(u32_to_u64(now.phv[flow_id_in_phv[0]], now.phv[flow_id_in_phv[1]]));
                for (int j = 0; j < match_table.number_of_hash_ways; j++)
                {
                    // key is only 128 bit long
                    if (key_to_compare[0] == now.obtained_keys[i][j][0][0] && key_to_compare[1] == now.obtained_keys[i][j][0][1] && key_to_compare[2] == now.obtained_keys[i][j][0][2] && key_to_compare[3] == now.obtained_keys[i][j][0][3])
                    {
                        found_flag = 1;
                    }

                    if (found_flag == 1)
                    {
                        next.final_values[i] = std::make_pair(now.obtained_values[i][j], true);
                        // 将匹配的 hash way 保存
                        next.stateful_matched_hash_way[i] = j;
                        break;
                    }
                }
                if (found_flag == 0)
                {
                    next.final_values[i].second = false;
                    next.stateful_matched_hash_way[i] = -1;
                }
                continue;
            }
            int found_flag = 0;
            for (int j = 0; j < match_table.number_of_hash_ways; j++)
            {
                // compare obtained key with original key
                for (int k = 0; k < match_table.key_width; k++)
                {
                    if (now.match_table_keys[i][k * 4 + 0] == now.obtained_keys[i][j][k][0] && now.match_table_keys[i][k * 4 + 1] == now.obtained_keys[i][j][k][1] && now.match_table_keys[i][k * 4 + 2] == now.obtained_keys[i][j][k][2] && now.match_table_keys[i][k * 4 + 3] == now.obtained_keys[i][j][k][3])
                    {
                        found_flag = 1;
                    }
                }
                if (found_flag == 1)
                {
                    // get the value of this way
                    next.final_values[i] = std::make_pair(now.obtained_values[i][j], true);
                    break; // handle next table
                }
            }
            if(match_table.default_action_id != -1) {
                next.final_values[i].second = true;
            }
            else{
                next.final_values[i].second = false;
            }
        }

        next.phv = now.phv;
        next.hash_values = now.hash_values;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
        // newly added
    }
};

#endif // RPISA_SW_MATCH_H
