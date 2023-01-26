//
// Created by root on 1/23/23.
//

#ifndef RPISA_SW_MATCH_H
#define RPISA_SW_MATCH_H

#include "../PipeLine.h"
#include "../dataplane_config.h"

struct GetKey : public Logic {
    GetKey(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {
        const GetKeysRegister & getKey = now.processors[processor_id].getKeys;
        GatewayRegister & nextGateway = next.processors[processor_id].gateway[0];

        getKeyFromPHV(getKey, nextGateway);
    }

    void getKeyFromPHV(const GetKeysRegister &now, GatewayRegister &next) {
        if (!now.enable1) {
            next.enable1 = now.enable1;
            return;
        }
        GetKeyConfig getKeyConfig = getKeyConfigs[processor_id];
        for (int i = 0; i < getKeyConfig.used_container_num; i++) {
            GetKeyConfig::UsedContainer2MatchFieldByte it = getKeyConfig.used_container_2_match_field_byte[i];
            int id = it.used_container_id;
            if (it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U8) {
                next.key[it.match_field_byte_ids[0]] = now.phv[0][id];
            } else if(it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U16) {
                next.key[it.match_field_byte_ids[0]] = now.phv[0][id] << 16 >> 24;
                next.key[it.match_field_byte_ids[1]] = now.phv[0][id] << 24 >> 24;
            } else {
                next.key[it.match_field_byte_ids[0]] = now.phv[0][id] >> 24;
                next.key[it.match_field_byte_ids[1]] = now.phv[0][id] << 8 >> 24;
                next.key[it.match_field_byte_ids[2]] = now.phv[0][id] << 16 >> 24;
                next.key[it.match_field_byte_ids[3]] = now.phv[0][id] << 24 >> 24;
            }
        }

        next.phv = now.phv[0];
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.gateway_guider;
    }
};

struct Gateway : public Logic {
    Gateway(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {
        const GatewayRegister & gatewayReg = now.processors[processor_id].gateway[0];
        GatewayRegister & nextGatewayReg = next.processors[processor_id].gateway[1];
        HashRegister &  hashReg = next.processors[processor_id].hash;

        gateway_first_cycle(gatewayReg, nextGatewayReg);
        gateway_second_cycle(nextGatewayReg, hashReg);
    }

    void gateway_first_cycle(const GatewayRegister &now, GatewayRegister &next) {
        next.enable1 = now.enable1;
        if (!now.enable1) {
            return;
        }

        array<bool, MAX_GATEWAY_NUM> gate_res = {false};
        // copy the gateway enabler from bus: enable which gates of 16.
        for (int i = 0; i < MAX_GATEWAY_NUM; i++) {
            if (now.gateway_guider[processor_id * 16 + i]) {
                gate_res[i] = get_gate_res(now, gatewaysConfigs[processor_id].gates[i]);
            }
            next.gate_res[i] = gate_res[i];
        }

        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.gateway_guider;
    }

    void gateway_second_cycle(const GatewayRegister &now, HashRegister &next) {
        auto gatewayConfig = gatewaysConfigs[processor_id];
        u32 res = bool_array_2_u32(now.gate_res);
        auto match_bitmap = gatewayConfig.gateway_res_2_match_tables[res];
        auto gateway_bitmap = gatewayConfig.gateway_res_2_gates[res];
        // get the newest match table guider and gateway guider
        for (int i = processor_id * 16; i < MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM; i++) {
            next.match_table_guider[i] = now.match_table_guider[i] | match_bitmap[i];
            next.gateway_guider[i] = now.gateway_guider[i] | gateway_bitmap[i];
        }

        next.phv = now.phv;
    }

    static u32 bool_array_2_u32(const array<bool, 16> bool_array) {
        u32 sum = 0;
        for(int i = 0 ; i < 16; i++) {
            sum += (1 << (15 - i));
        }
        return sum;
    }

    static bool get_gate_res(const GatewayRegister &now, GatewaysConfig::Gate gate) {
        auto op = gate.op;
        auto operand1 = get_operand_value(now, gate.operand1);
        auto operand2 = get_operand_value(now, gate.operand2);
        switch (op) {
            case GatewaysConfig::Gate::OP::EQ : return operand1 == operand2;
            case GatewaysConfig::Gate::OP::GT : return operand1 > operand2;
            case GatewaysConfig::Gate::OP::LT : return operand1 < operand2;
            case GatewaysConfig::Gate::OP::GTE : return operand1 >= operand2;
            case GatewaysConfig::Gate::OP::LTE : return operand1 <= operand2;
            default : break;
        }
        return false;
    }

    static u32 get_operand_value(const GatewayRegister &now, GatewaysConfig::Gate::Parameter operand) {
        switch(operand.type) {
            case GatewaysConfig::Gate::Parameter::CONST:
                return operand.content.value;
            case GatewaysConfig::Gate::Parameter::HEADER: {
                u32 tmp = 0;
                int len = operand.content.operand_match_field_byte.len;
                // get byte from match_field and compute the result
                for (int i = 0; i < len; i++) {
                    tmp += now.key[operand.content.operand_match_field_byte.match_field_byte_ids[i] << (len - 1) * 8];
                }
                return tmp;
            }
            default: break;
        }
    }
};

struct GetHash : public Logic {
    GetHash(int id) : Logic(id) {}

    void execute(const PipeLine &now, PipeLine &next) override {

    }

}


#endif //RPISA_SW_MATCH_H
