//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_GETKEY_H
#define RPISA_SW_GETKEY_H

#include "../PipeLine.h"
// 这里配置get key所用的时间周期


struct KeyConfig {
    int id;
    int length;
};

struct GatewayConfig {
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
    vector<int> id;
    vector<int> length;
    int hashValueLength;

    u64 hash(const b1024& key) const {
        BitArray bit;
        for (int i = 0; i < id.size(); i++) {
            BitArray::Combine(bit, BitArray(key[id[i]], length[i]));
        }

        u64 hash = (u64) 2166136261L;
        for (int i = 0; i < bit.Length(); i++) {
            hash = (hash * 16777619) ^ u64(bit.GetBit(i));
        }

        return hash & ((1 << hashValueLength) - 1);

    }

};

struct AddressConfig {
    vector<int> length;
};


struct GetKey : public Logic {

    GetKey(int id) : Logic(id) {}

    array<KeyConfig, KEY_NUM>    keyConfig;
    array<HashConfig, HASH_NUM> hashConfig;
    array<GatewayConfig, GATEWAY_NUM> gatewayConfig;
    map<array<bool, GATEWAY_NUM>, array<bool, READ_TABLE_NUM>> gatewayTable;

    void execute(const PipeLine &now, PipeLine &next) override {
        const GetKeyRegister& getKey = now.processors[processor_id].getKey;
        GetKeyRegister& nextGetKey  = next.processors[processor_id].getKey;

        nextGetKey.enable2 = getKey.enable1;
        getKeyFromPHV(getKey, nextGetKey);

        gateway(getKey, nextGetKey);
        getHash(getKey, nextGetKey);
        getAddress(getKey, nextGetKey);

    }

    void getKeyFromPHV(const GetKeyRegister &now, GetKeyRegister &next) {
        for (int i = 0; i < keyConfig.size(); i++) {
            next.key[i] = now.phv[0][keyConfig[i].id] & ((1 << keyConfig[i].length) - 1);
        }
        for (int i = 1; i < GET_KEY_ALL_CYCLE; i++) {
            next.phv[i] = now.phv[i - 1];
        }
    }

    //todo: Gateway 需要改
    void gateway(const GetKeyRegister &now, GetKeyRegister &next) {

        array<bool, GATEWAY_NUM>  gatewayResult;

        for (int i = 0; i < gatewayConfig.size(); i++) {
            u32 a = gatewayGetValue(gatewayConfig[i].a, now.key);
            u32 b = gatewayGetValue(gatewayConfig[i].b, now.key);
            gatewayResult[i] = compare(a, b, gatewayConfig[i].op);
        }
        if (now.enable2)
            next.readEnable[0] = gatewayTable[gatewayResult];
        else
            next.readEnable[0] = {};
        for (int i = 1; i < HASH_CYCLE; i++) {
            next.readEnable[i] = next.readEnable[i - 1];
        }
    }

    u32 gatewayGetValue(const GatewayConfig::Parameter& parameter, const b1024& key) {
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

    void getHash(const GetKeyRegister &now, GetKeyRegister &next) {
        for (int i = 0; i < hashConfig.size(); i++) {
            next.hashValue[0][i] = hashConfig[i].hash(now.key);
        }
        for (int i = 1; i < HASH_CYCLE; i++) {
            next.hashValue[i] = now.hashValue[i - 1];
        }
    }




    array<AddressConfig, READ_TABLE_NUM> addressConfig;
    void getAddress(const GetKeyRegister& now, GetKeyRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            u64 hashValue = now.hashValue[HASH_CYCLE - 1][i];
            int sum = 0;
            for (int j : addressConfig[i].length) {
                sum += j;
            }
            hashValue &= (1 << sum) - 1;
            for (int j = 0; j < addressConfig[i].length.size(); j++) {
                next.address[i][j] = hashValue & ((1 << addressConfig[i].length[j]) - 1);
                hashValue >>= addressConfig[i].length[j];
            }
        }
    }


};

#endif
