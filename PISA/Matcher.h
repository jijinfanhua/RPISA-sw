//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_MATCHER_H
#define RPISA_SW_MATCHER_H

#include "../defs.h"
#include "SRAM.h"
const int MATCHER_ALL_CYCLE = 4;
struct PipeLine::MatcherRegister {

    array<PHV, MATCHER_ALL_CYCLE> phv;


    array<u32,  READ_TABLE_NUM> hashValue;
    array<Key,  READ_TABLE_NUM> keyCycleMatch;
    array<bool, READ_TABLE_NUM> readEnableCycleMatch;

    array<Row,  READ_TABLE_NUM> row;
    array<Key,  READ_TABLE_NUM> keyCycleCompare;
    array<bool, READ_TABLE_NUM> readEnableCycleCompare;

    array<Key,  READ_TABLE_NUM> value;
    array<bool, READ_TABLE_NUM> compare;
    array<bool, READ_TABLE_NUM> readEnableOutput;


};

struct MatcherConfig {
    int tableBegin;
    int keyLength;
};
struct Matcher : public Logic {

    SRAM& sram;
    array<MatcherConfig, READ_TABLE_NUM> matcherConfig;

    void execute(const PipeLine &now, PipeLine &next) override {
        PipeLine::MatcherRegister& nextMatcher  = next.processors[processor_id].matcher;
        const PipeLine::MatcherRegister& matcher = now.processors[processor_id].matcher;
        nextMatcher.keyCycleCompare = matcher.keyCycleMatch;
        nextMatcher.readEnableCycleCompare = matcher.readEnableCycleMatch;
        nextMatcher.readEnableOutput = matcher.readEnableCycleCompare;
        match  (matcher, nextMatcher);
        compare(matcher, nextMatcher);
        for (int i = 1; i < MATCHER_ALL_CYCLE; i++) {
            nextMatcher.phv[i] = matcher.phv[i - 1];
        }
    }

    void match(const PipeLine::MatcherRegister& now, PipeLine::MatcherRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            if (now.readEnableCycleMatch[i]) {
                int offset = now.hashValue[i] >> matcherConfig[i].keyLength;
                int id = matcherConfig[i].tableBegin + offset;
                next.row[i] = sram.get(id, now.hashValue[i] & ((1 << matcherConfig[i].keyLength) - 1));

            }
        }
    }

    void compare(const PipeLine::MatcherRegister& now, PipeLine::MatcherRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            if (now.readEnableCycleCompare[i]) {
                if (now.row[i].key == now.keyCycleCompare[i]) {
                    next.value[i] = now.row[i].value;
                    next.compare[i] = true;
                } else {
                    next.compare[i] = false;
                }
            } else {
                next.compare[i] = false;
            }
        }
    }



};

#endif //RPISA_SW_MATCHER_H
