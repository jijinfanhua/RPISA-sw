//
// Created by root on 1/23/23.
//

#ifndef RPISA_SW_MATCH_H
#define RPISA_SW_MATCH_H

#include "../pipeline.h"
#include "../dataplane_config.h"

struct ArrayHash {
    size_t operator()(const std::array<u32, 128>& arr) const {
        std::hash<u32> hasher;
        size_t hash_val = 0;
        for (int i = 0; i < 128; i++) {
            hash_val ^= hasher(arr[i]) + 0x9e3779b9 + (hash_val<<6) + (hash_val>>2);
        }
        return hash_val & ((1ull << 52) - 1);
    }
};

u64 get_flow_id(PHV phv)
{
    // done: fetch flow id from two particular positions of phv
    return u32_to_u64(phv[flow_id_in_phv[0]], phv[flow_id_in_phv[1]]);
}

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
        DispatcherRegister &dpRegister = next->processors[processor_id].dp;

        get_hash(hashReg, nextHashReg);
        for (int i = 1; i < HASH_CYCLE - 1; i++)
        {
            get_left_hash(now->processors[processor_id].hashes[i], next->processors[processor_id].hashes[i + 1]);
        }

        const HashRegister &lastHashReg = now->processors[processor_id].hashes[HASH_CYCLE - 1];

        dpRegister.enable1 = lastHashReg.enable1;
        if (!lastHashReg.enable1)
        {
            return;
        }
        dpRegister.dq_item.phv = lastHashReg.phv;
        //        getAddressRegister.key = lastHashReg.key;
        dpRegister.dq_item.hash_values = lastHashReg.hash_values;
        dpRegister.dq_item.match_table_guider = lastHashReg.match_table_guider;
        dpRegister.dq_item.gateway_guider = lastHashReg.gateway_guider;
        dpRegister.dq_item.match_table_keys = lastHashReg.match_table_keys;
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
        // hash for match-action table
        for (int i = 0; i < matchTableConfig.match_table_num; i++)
        {
            // operate stateless table
            auto match_table = matchTableConfig.matchTables[i];
            std::array<u32, 32> match_table_key{};
            for (int j = 0; j < match_table.match_field_byte_len; j++)
            {
                match_table_key[j] = now.key[match_table.match_field_byte_ids[j]];
            }

                cal_hash(i, match_table_key, match_table.match_field_byte_len, match_table.number_of_hash_ways,
                         match_table.hash_bit_per_way, match_table.hash_bit_sum, next);
                next.match_table_keys[i] = match_table_key;

        }
        // hash for identify flow
        ArrayHash hasher;
        u64 flow_id = hasher(now.key);
        // high 32 bit
        phv[flow_id_in_phv[0]] = flow_id >> 32;
        // low 32 bit
        phv[flow_id_in_phv[1]] = flow_id << 32 >> 32;

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

struct Dispatcher: public Logic{
    Dispatcher(int id): Logic(id){}

    void execute(const PipeLine* now, PipeLine* next) override{
        const DispatcherRegister& now_dp_0 = now->processors[processor_id].dp;
        GetAddressRegister &get_address = next->processors[processor_id].getAddress;

        dispatcher_cycle_1(now_dp_0, get_address, now->proc_states[processor_id], next->proc_states[processor_id]);
    }

    bool test_flow_occupied(DispatcherQueueItem dp_item){
        // todo: test flow occupied
        return false;
    };

    void dispatcher_cycle_1(const DispatcherRegister& now, GetAddressRegister& next, const ProcessorState& now_proc, ProcessorState& next_proc){
        if(now.enable1){
            // if pipeline has packet
            u32 queue_id = get_flow_id(now.phv) & 0xf;
            next_proc.dispatcher_queues[queue_id].push_back(now.dq_item);
        }
        // get schedule id
        int schedule_id = now_proc.schedule_id;
        int queues_count = 0;
        while(now_proc.dispatcher_queues[schedule_id].empty() || test_flow_occupied(now_proc.dispatcher_queues[schedule_id].front())){
            // if queue empty or first flow occupied
            schedule_id = (schedule_id + 1) % 16;
            queues_count += 1;
            if(queues_count == 16) break;
        }
        next_proc.schedule_id = schedule_id;

        if(queues_count == 16){
            next.enable1 = false;
            return;
        }
        else{
            next.enable1 = true;
            auto dp_item = now_proc.dispatcher_queues[schedule_id].front();
            next.phv = dp_item.phv;
            next.hash_values = dp_item.hash_values;
            next.match_table_keys = dp_item.match_table_keys;
            next.match_table_guider = dp_item.match_table_guider;
            next.gateway_guider = dp_item.gateway_guider;
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
        ConditionEvaluationRegister ceReg = next->processors[processor_id].conditionEvaluation;

        get_state(compareReg, ceReg);
    }

    void get_state(const CompareRegister &now, ConditionEvaluationRegister &next)
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
                next.hits[i] = false;
                continue;
            }
            auto match_table = matchTableConfig.matchTables[i];

            int found_flag = 1;
            for (int j = 0; j < match_table.number_of_hash_ways; j++)
            {
                // compare obtained key with original key
                for (int k = 0; k < match_table.key_width; k++)
                {
                    if (now.match_table_keys[i][k * 4 + 0] == now.obtained_keys[i][j][k][0] && now.match_table_keys[i][k * 4 + 1] == now.obtained_keys[i][j][k][1] && now.match_table_keys[i][k * 4 + 2] == now.obtained_keys[i][j][k][2] && now.match_table_keys[i][k * 4 + 3] == now.obtained_keys[i][j][k][3])
                    {
                    }
                    else{
                        found_flag = 0;
                    }
                }
                if (found_flag == 1)
                {
                    // get the value of this way
                    next.hits[i] = true;
                    // first 32 bit: state; next every 32 bit: register, max 16
                    next.states[i] = now.obtained_values[i][j][0][0];
                    for(int m = 0; m < match_table.value_width; m++){
                        if(m == 4) break;
                        // register_0 == state; register 1-15 refers to registers
                        next.registers[i][m*4] = now.obtained_values[i][j][m][0];
                        next.registers[i][m*4+1] = now.obtained_values[i][j][m][1];
                        next.registers[i][m*4+2] = now.obtained_values[i][j][m][2];
                        next.registers[i][m*4+3] = now.obtained_values[i][j][m][3];
                    }
                    break; // handle next table
                }
            }
            if(found_flag == 0){
                //default
                next.hits[i] = false;
            }
        }
        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
        next.match_table_keys = now.match_table_keys;
        // newly added
    }
};

struct ConditionEvaluation: public Logic{
    ConditionEvaluation(int id): Logic(id){};
    void execute(const PipeLine *now, PipeLine *next) override{
        const ConditionEvaluationRegister& ceReg = now->processors[processor_id].conditionEvaluation;
        KeyRefactorRegister& refactorReg = next->processors[processor_id].refactor;

        condition_evaluation(ceReg, refactorReg);
    }

    void condition_evaluation(const ConditionEvaluationRegister& now, KeyRefactorRegister& next){
        next.enable1 = now.enable1;
        if(!now.enable1){
            return;
        }

        auto enable_function_config = enable_functions_configs[processor_id];

        array<array<bool, 7>, MAX_PARALLEL_MATCH_NUM> enable_function_result{};
        for(int i = 0; i < MAX_PARALLEL_MATCH_NUM; i++){
            for(int j = 0; j < 7; j++){
                if(enable_function_config.enable_functions[i][j].enable){
                    enable_function_result[i][j] = get_enable_function_result(now, enable_function_config.enable_functions[i][j], i);
                }
            }
        }

        next.enable_function_result = enable_function_result;
        next.phv = now.phv;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.states = now.states;
        next.registers = now.registers;
    }

    static u32 get_operand_value(const ConditionEvaluationRegister &now,
                                EnableFunctionsConfig::EnableFunction::Parameter operand, int table_id) {
        switch (operand.type) {
            case EnableFunctionsConfig::EnableFunction::Parameter::CONST:
                return operand.value;
            case EnableFunctionsConfig::EnableFunction::Parameter::HEADER:
                return now.match_table_keys[table_id][operand.value];
            case EnableFunctionsConfig::EnableFunction::Parameter::STATE:
                return now.states[table_id];
            case EnableFunctionsConfig::EnableFunction::Parameter::REGISTER:
                return now.registers[table_id][operand.value];
        }
    }

    bool get_enable_function_result(const ConditionEvaluationRegister& now, EnableFunctionsConfig::EnableFunction function, int table_id){
        auto op = function.op;
        auto operand1 = get_operand_value(now, function.operand1, table_id);
        auto operand2 = get_operand_value(now, function.operand2, table_id);
        switch (op) {
            case EnableFunctionsConfig::EnableFunction::EQ: {
                return operand1 == operand2;
            }
            case EnableFunctionsConfig::EnableFunction::GT: {
                return operand1 > operand2;
            }
            case EnableFunctionsConfig::EnableFunction::LT: {
                return operand1 < operand2;
            }
            case EnableFunctionsConfig::EnableFunction::GTE:{
                return operand1 >= operand2;
            }
            case EnableFunctionsConfig::EnableFunction::LTE: {
                return operand1 <= operand2;
            }
            case EnableFunctionsConfig::EnableFunction::NEQ: {
                return operand1 != operand2;
            }
            default:
                break;
        }
        return false;
    }
};

struct KeyRefactor: public Logic{
    KeyRefactor(int id): Logic(id){}

    void execute(const PipeLine *now, PipeLine *next) override{
        // 7位 enable function 放到一个32位中，state 放到一个32位中，两个32位放到matchkey之后
        const KeyRefactorRegister& refactorReg = now->processors[processor_id].refactor;
        EfsmHashRegister& efsmHashReg = next->processors[processor_id].efsmHash[0];

        refactor_key(refactorReg, efsmHashReg);
    }

    static u32 bool_array_2_u32(const array<bool, 7> bool_array)
    {
        u32 sum = 0;
        for (int i = 0; i < 7; i++)
        {
            if(bool_array[i]) {
                sum += (1 << (6 - i));
            }
        }
        return sum;
    }

    void refactor_key(const KeyRefactorRegister& now, EfsmHashRegister& next){
        next.enable1 = now.enable1;
        if(!now.enable1) return;

        for(int i = 0; i < efsmTableConfigs[processor_id].efsm_table_num; i++){
            auto efsm_table = efsmTableConfigs[processor_id].efsmTables[i];
            auto enable_result_position = efsm_table.byte_len - 2;
            auto state_position = efsm_table.byte_len - 1;
            next.match_table_keys[i] = now.match_table_keys[i];
            next.match_table_keys[i][enable_result_position] = bool_array_2_u32(now.enable_function_result[i]);
            next.match_table_keys[i][state_position] = now.states[i];
        }

        next.phv = now.phv;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.states = now.states;
        next.registers = now.registers;
    }
};

struct EFSMHash: public Logic{
    // 逻辑基本复用 Hash 模块，key 增加了比较结果和state
    EFSMHash(int id): Logic(id){}
    void execute(const PipeLine *now, PipeLine *next) override
    {
        const EfsmHashRegister &hashReg = now->processors[processor_id].efsmHash[0];
        EfsmHashRegister &nextHashReg = next->processors[processor_id].efsmHash[1];
        EfsmGetAddressRegister& nextGetAddrReg = next->processors[processor_id].efsmGetAddress;

        get_hash(hashReg, nextHashReg);
        for (int i = 1; i < HASH_CYCLE - 1; i++)
        {
            get_left_hash(now->processors[processor_id].efsmHash[i], next->processors[processor_id].efsmHash[i + 1]);
        }

        const EfsmHashRegister &lastHashReg = now->processors[processor_id].efsmHash[HASH_CYCLE - 1];

         nextGetAddrReg.enable1 = lastHashReg.enable1;
        if (!lastHashReg.enable1)
        {
            return;
        }
         nextGetAddrReg.phv = lastHashReg.phv;
         nextGetAddrReg.hash_values = lastHashReg.hash_values;
         nextGetAddrReg.match_table_guider = lastHashReg.match_table_guider;
         nextGetAddrReg.gateway_guider = lastHashReg.gateway_guider;
         nextGetAddrReg.match_table_keys = lastHashReg.match_table_keys;
         nextGetAddrReg.states = lastHashReg.states;
         nextGetAddrReg.registers = lastHashReg.registers;
    }

    static void get_left_hash(const EfsmHashRegister &now, EfsmHashRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }
        next.phv = now.phv;
        next.match_table_keys = now.match_table_keys;
        next.hash_values = now.hash_values;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.states = now.states;
        next.registers = now.registers;
    }

    void get_hash(const EfsmHashRegister &now, EfsmHashRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }
        auto efsmTableConfig = efsmTableConfigs[processor_id];
        // hash for efsm table
        for (int i = 0; i < efsmTableConfig.efsm_table_num; i++)
        {
            auto efsm_table = efsmTableConfig.efsmTables[i];
                cal_hash(i, now.match_table_keys[i], efsm_table.byte_len, efsm_table.num_of_hash_ways,
                         efsm_table.hash_bit_per_way, efsm_table.hash_bit_sum, next);

        }
        // hash for identify flow
        next.phv = now.phv;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.states = now.states;
        next.registers = now.registers;
    }

    static void cal_hash(int table_id, const std::array<u32, 32> &match_table_key, int match_field_byte_len,
                         int number_of_hash_ways, std::array<int, 4> hash_bit_per_way, int hash_bit_sum, EfsmHashRegister &next)
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

struct EFSMGetAddress: public Logic{
    EFSMGetAddress(int id): Logic(id){}

    void execute(const PipeLine* now, PipeLine* next) override{
        const EfsmGetAddressRegister& getAddress = now->processors[processor_id].efsmGetAddress;
        EfsmMatchRegister& match = next->processors[processor_id].efsmMatch;

        get_address(getAddress, match);
    }

    void get_address(const EfsmGetAddressRegister &now, EfsmMatchRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }

        auto efsmTableConfig = efsmTableConfigs[processor_id];

        // get the sram indexes per way for each match table
        for (int i = 0; i < efsmTableConfig.efsm_table_num; i++)
        {
            auto hash_values = now.hash_values[i];
            auto efsmTable = efsmTableConfig.efsmTables[i];
            for (int j = 0; j < efsmTable.num_of_hash_ways; j++)
            {
                u32 start_key_index = (hash_values[j] >> 10) * efsmTable.key_width;
                u32 start_value_index = (hash_values[j] >> 10) * efsmTable.value_width;
                next.on_chip_addrs[i][j] = (hash_values[j] << 22 >> 22);
                for (int k = 0; k < efsmTable.key_width; k++)
                {
                    next.key_sram_columns[i][j][k] = efsmTable.key_sram_index_per_hash_way[j][start_key_index + k];
                    next.mask_sram_columns[i][j][k] = efsmTable.mask_sram_index_per_hash_way[j][start_key_index + k];
                }
                for (int k = 0; k < efsmTable.value_width; k++)
                {
                    next.value_sram_columns[i][j][k] = efsmTable.value_sram_index_per_hash_way[j][start_value_index + k];
                }
            }
        }
        next.phv = now.phv;
        next.match_table_keys = now.match_table_keys;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.states = now.states;
        next.registers = now.registers;
    }
};

struct EFSMMatch: public Logic{
    EFSMMatch(int id): Logic(id){}

    void execute(const PipeLine* now, PipeLine* next) override{
        const EfsmMatchRegister& match = now->processors[processor_id].efsmMatch;
        EfsmCompareRegister& compare = next->processors[processor_id].efsmCompare;

        get_value_to_compare(match, compare);
    }

    void get_value_to_compare(const EfsmMatchRegister &now, EfsmCompareRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }

        auto efsmTableConfig = efsmTableConfigs[processor_id];
        for (int i = 0; i < efsmTableConfig.efsm_table_num; i++)
        {
            // follow match table guider
            if (!now.match_table_guider[processor_id * 16 + i])
                continue;
            auto efsmTable = efsmTableConfig.efsmTables[i];
            for (int j = 0; j < efsmTable.num_of_hash_ways; j++)
            {
                for (int k = 0; k < efsmTable.key_width; k++)
                {
                    next.obtained_keys[i][j][k] = SRAMs[processor_id][now.key_sram_columns[i][j][k]].get(now.on_chip_addrs[i][j]);
                    next.obtained_masks[i][j][k] = SRAMs[processor_id][now.mask_sram_columns[i][j][k]].get(now.on_chip_addrs[i][j]);
                }
                for (int k = 0; k < efsmTable.value_width; k++)
                {
                    next.obtained_values[i][j][k] = SRAMs[processor_id][now.value_sram_columns[i][j][k]].get(now.on_chip_addrs[i][j]);
                }
            }
        }

        next.phv = now.phv;
        next.match_table_keys = now.match_table_keys;
        next.match_table_guider = now.match_table_guider;
        next.gateway_guider = now.gateway_guider;
        next.states = now.states;
        next.registers = now.registers;
    }
};

struct EfsmCompare: public Logic{
    EfsmCompare(int id): Logic(id){}

    void execute(const PipeLine* now, PipeLine* next) override{

    }

    void get_action(const EfsmCompareRegister &now, GetActionRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }
        auto efsmTableConfig = efsmTableConfigs[processor_id];

        for (int i = 0; i < efsmTableConfig.efsm_table_num; i++)
        {
            if (!now.match_table_guider[processor_id * 16 + i])
            {
                next.final_values[i].second = false;
                continue;
            }
            auto efsmTable = efsmTableConfig.efsmTables[i];

            int found_flag = 1;
            for (int j = 0; j < efsmTable.num_of_hash_ways; j++)
            {
                // compare obtained key with original key
                for (int k = 0; k < efsmTable.key_width; k++)
                {
                    if (now.match_table_keys[i][k * 4 + 0] == (now.obtained_keys[i][j][k][0] & now.obtained_masks[i][j][k][0])
                    && now.match_table_keys[i][k * 4 + 1] == (now.obtained_keys[i][j][k][1] & now.obtained_masks[i][j][k][1])
                    && now.match_table_keys[i][k * 4 + 2] == (now.obtained_keys[i][j][k][2] & now.obtained_masks[i][j][k][2])
                    && now.match_table_keys[i][k * 4 + 3] == (now.obtained_keys[i][j][k][3] & now.obtained_masks[i][j][k][3]))
                    {
                    }
                    else{
                        found_flag = 0;
                    }
                }
                if (found_flag == 1)
                {
                    // get the value of this way
                    next.final_values[i] = std::make_pair(now.obtained_values[i][j], true);
                    break; // handle next table
                }
            }
            if(found_flag == 0){
                //default
                next.final_values[i].second = false;
            }
        }
        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
        next.states = now.states;
        next.registers = now.registers;
        // newly added
    }
};

#endif // RPISA_SW_MATCH_H
