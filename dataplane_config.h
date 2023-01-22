//
// Created by root on 1/22/23.
//

#ifndef RPISA_SW_DATAPLANE_CONFIG_H
#define RPISA_SW_DATAPLANE_CONFIG_H

// store all configs that dataplane processors need: global variables;
// can only be configured before dataplane start

// getKey configs for N processors:
//      use which containers;
//      map containers to 1024bits

// gateway configs for each processor:
//      use which 16 gateways;
//      ops
//      operands: from which match_fields
//      map result to match tables; map result to next gateways.

// hash unit configs:
//      hash function

// table configs:
//      size: depth & width [key + value]
//      fields from which match_fields
//      number of hash ways

// action configs:
//      action_id to primitive mapping: enable which way of 224
//      ALU: alu_id and op;
//      ALU operands: from PHV (field & metadata), from constant registers, from action_data
//      action_data: extract from value, action_data_id

#endif //RPISA_SW_DATAPLANE_CONFIG_H
