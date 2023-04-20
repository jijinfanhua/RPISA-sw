//
// Created by ASUS on 2023/1/12.
//

#ifndef RPISA_SW_REGISTER_H
#define RPISA_SW_REGISTER_H

#include "../../defs.h"
#include "SRAM.h"
#include "../../dataplane_config.h"

const int HASH_CYCLE = 4;
const int ADDRESS_WAY = 4;
const int HASH_NUM = 16;
const int GATEWAY_NUM = 16;
const int ADDRESS_MAX_NUM = 2;
const int GET_KEY_ALL_CYCLE = HASH_CYCLE + 1 + 1;

/*********** fengyong add start ***************/
struct BaseRegister {
    bool enable1;
    PHV phv;

    std::array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> gateway_guider;
};

struct GetKeysRegister : public BaseRegister{

};

struct DispatcherQueueItem{
    PHV phv;
};

struct HashRegister : public BaseRegister{
    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
};

struct DispatcherRegister: public BaseRegister{
    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    DispatcherQueueItem dq_item;
};

struct OperateRegister: public BaseRegister{

};



#endif //RPISA_SW_REGISTER_H
