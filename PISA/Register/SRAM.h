//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_SRAM_H
#define RPISA_SW_SRAM_H

#include "../../defs.h"

using SRAM_UNIT = array<b128, SRAM_SIZE>;

struct SRAM {
    b128 get(int index) {
        return entries[index];
    }
    void set(int index, b128 entry) {
        entries[index] = entry;
    }
    private: std::array<b128, SRAM_SIZE> entries;
};

struct SRAMs {

    array<SRAM_UNIT, SRAM_NUM> sram;
    b1024 get (vector<int> id, int row) {
        b1024 res = {};
        for (int i = 0; i < id.size(); i++) {
            for (int j = 0; j < VALUE_NUM; j++) {
                res[i * VALUE_NUM + j] = sram[id[i]][row][j];
            }
        } return res;
    }
};

#endif //RPISA_SW_SRAM_H
