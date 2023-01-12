//
// Created by ASUS on 2023/1/12.
//

#ifndef RPISA_SW_RPISA_REGISTER_H
#define RPISA_SW_RPISA_REGISTER_H

#include "../../defs.h"

struct GetKeyResult {
    PHV phv;
    array<Key,  READ_TABLE_NUM> key;
    array<u32,  READ_TABLE_NUM> hashValue;
    array<bool, READ_TABLE_NUM> readEnable;
    GetKeyResult(
            const PHV& phv,
            const array<Key,  READ_TABLE_NUM>& key,
            const array<u32,  READ_TABLE_NUM>& hashValue,
            const array<bool, READ_TABLE_NUM>& readEnable) :
            phv(phv), key(key), hashValue(hashValue), readEnable(readEnable) {
    }

};


struct PIRRegister {
    GetKeyResult output;
    bool priority;
    map<int, int> timeOut;
};


struct PORegister {
    queue<GetKeyResult> ready;
};


struct PIWRegister {

};



#endif
