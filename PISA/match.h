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

        gateway_first_cycle(gatewayReg, nextGatewayReg);
    }

    void gateway_first_cycle(const GatewayRegister &now, GatewayRegister &next) {
        if (!now.enable1) {
            next.enable1 = now.enable1;
            return;
        }
        array<bool, MAX_GATEWAY_NUM> gate_enabler;
        array<bool, MAX_GATEWAY_NUM> gate_res;
        // copy the gateway enabler from bus: enable which gates of 16.
        for (int i = 0; i < MAX_GATEWAY_NUM; i++) {
            gate_enabler[i] = now.gateway_guider[processor_id * 16 + i];
            if (gate_enabler[i]) {
                gate_res[i] = get_gate_res(now, gatewaysConfigs[processor_id].gates[i]);
            }
        }
    }

    bool get_gate_res(const GatewayRegister &now, GatewaysConfig::Gate gate) {
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
    }

    u32 get_operand_value(const GatewayRegister &now, GatewaysConfig::Gate::Parameter operand) {
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


#endif //RPISA_SW_MATCH_H
