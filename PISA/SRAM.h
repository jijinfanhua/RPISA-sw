//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_SRAM_H
#define RPISA_SW_SRAM_H

#include "../defs.h"
const int SRAM_NUM = 80;
const int SRAM_SIZE = 1024;

struct Row {
    Key key, value;
};

using SRAM_UNIT = array<Row, SRAM_SIZE>;

struct SRAM {

    array<SRAM_UNIT, SRAM_NUM> content;
    const Row& get (int id, int key) {
        return content[id][key];
    }
};

#endif //RPISA_SW_SRAM_H
