//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_GETKEY_H
#define RPISA_SW_GETKEY_H

#include "../defs.h"
// 这里配置get key所用的时间周期
const int HASH_CYCLE = 4;
const int HASH_NUM = 8;
const int GATEWAY_NUM = 16;

struct PipeLine::GetKeyRegister {
    PHV phvInput;
    Key key;
    array<array<bool, GATEWAY_NUM>, HASH_CYCLE> gatewayResult;
    array<array<u64, HASH_NUM>,     HASH_CYCLE> hashValue;
};

struct KeyConfig {
    int id;
    int length;


};

struct GatewayConfig {
    u32 mask;
    struct Parameter {
        enum Type {CONST, HEADER} type;
        union {
            u32 value;
            u32 id;
        } content;
    } a, b;
    enum OP {
        EQ, NONE
    } op;
};

struct HashConfig {
    Key mask;
    u64 (* hash)(const Key&);
};



struct GetKey : public Logic {

    GetKey(int id) : Logic(id) {}

    array<KeyConfig, KEY_NUM> keyConfig;
    array<HashConfig, HASH_NUM> hashConfig;
    array<GatewayConfig, GATEWAY_NUM> gatewayConfig;

    void execute(const PipeLine &now, PipeLine &next) override {
        const PipeLine::GetKeyRegister& getKey = now.processors[processor_id].getKey;
        PipeLine::GetKeyRegister& nextGetKey  = next.processors[processor_id].getKey;

        getKeyFromPHV(getKey, nextGetKey);

        gateway(getKey, nextGetKey);
        getHash(getKey, nextGetKey);

    }

    void getKeyFromPHV(const PipeLine::GetKeyRegister &now, PipeLine::GetKeyRegister &next) {
        next.key = {};
        for (int i = 0; i < keyConfig.size(); i++) {
            next.key[i] = now.phvInput[keyConfig[i].id] & ((1 << keyConfig[i].length) - 1);
        }
    }

    void gateway(const PipeLine::GetKeyRegister &now, PipeLine::GetKeyRegister &next) {
        for (int i = 0; i < gatewayConfig.size(); i++) {
            u32 a = gatewayGetValue(gatewayConfig[i].a, now.key);
            u32 b = gatewayGetValue(gatewayConfig[i].b, now.key);
            next.gatewayResult[0][i] = compare(a, b, gatewayConfig[i].op);
        }
        for (int i = 1; i < HASH_CYCLE; i++) {
            next.gatewayResult[i] = next.gatewayResult[i - 1];
        }
    }

    u32 gatewayGetValue(const GatewayConfig::Parameter& parameter, const Key& key) {
        switch (parameter.type) {
            case GatewayConfig::Parameter::CONST:
                return parameter.content.value;
            case GatewayConfig::Parameter::HEADER:
                return key[parameter.content.id];
        }
    }

    bool compare(u32 a, u32 b, GatewayConfig::OP op) {
        switch (op) {
            case GatewayConfig::EQ:
                return a == b;
            case GatewayConfig::NONE:
                return false;
        }
    }

    void getHash(const PipeLine::GetKeyRegister &now, PipeLine::GetKeyRegister &next) {
        for (int i = 0; i < hashConfig.size(); i++) {
            next.hashValue[0][i] = hashConfig[i].hash(Mask(now.key, hashConfig[i].mask));
        }
        for (int i = 1; i < HASH_CYCLE; i++) {
            next.hashValue[i] = next.hashValue[i - 1];
        }
    }


    Key Mask(const Key& key, const Key& mask) {
        Key res = key;
        for (int j = 0; j < key.size(); j++) {
            res[j] &= mask[j];
        } return res;
    }
};

#endif
