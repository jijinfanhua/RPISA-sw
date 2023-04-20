//
// Created by root on 1/22/23.
//

#ifndef RPISA_SW_DATAPLANE_CONFIG_H
#define RPISA_SW_DATAPLANE_CONFIG_H

#include <unordered_map>
#include "PISA/Register/SRAM.h"

#define MAX_PHV_CONTAINER_NUM 224
#define MAX_MATCH_FIELDS_BYTE_NUM 128
#define MAX_GATEWAY_NUM 16
#define MAX_PARALLEL_MATCH_NUM 16
#define PROCESSOR_NUM 1

#define PROCESSING_FUNCTION_NUM 150

// store all configs that dataplane processors need: global variables;
// can only be configured before dataplane start

// getKey configs for N processors:
//      use which containers;
//      map containers to 1024bits
struct GetKeyConfig
{
    int used_container_num;

    struct UsedContainer2MatchFieldByte
    {
        int used_container_id;
        enum ContainerType
        {
            U8,
            U16,
            U32
        } container_type;
        std::array<int, 4> match_field_byte_ids;
    };

    std::array<UsedContainer2MatchFieldByte,
               MAX_MATCH_FIELDS_BYTE_NUM>
        used_container_2_match_field_byte;
};

GetKeyConfig getKeyConfigs[PROC_NUM];

const int dispatcher_queue_width = 16;

// two positions in phv for flow id; can use any of the hash result
std::array<int, 2> flow_id_in_phv;

#endif // RPISA_SW_DATAPLANE_CONFIG_H+
