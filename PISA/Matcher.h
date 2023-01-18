//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_MATCHER_H
#define RPISA_SW_MATCHER_H

#include "../PipeLine.h"



struct MatcherConfig {
    vector<vector<int>> tableSId;
    int keySWidth;
};

struct Matcher : public Logic {

    SRAMs& sram;
    array<MatcherConfig, READ_TABLE_NUM> matcherConfig;

    void execute(const PipeLine &now, PipeLine &next) override {
        MatcherRegister& nextMatcher  = next.processors[processor_id].matcher;
        const MatcherRegister& matcher = now.processors[processor_id].matcher;
        nextMatcher.keyCycleCompare = matcher.keyCycleMatch;
        nextMatcher.readEnableCycleCompare = matcher.readEnableCycleMatch;
        nextMatcher.readEnableOutput = matcher.readEnableCycleCompare;
        match  (matcher, nextMatcher);
        compare(matcher, nextMatcher);
        for (int i = 1; i < MATCHER_ALL_CYCLE; i++) {
            nextMatcher.phv[i] = matcher.phv[i - 1];
        }
    }

    void match(const MatcherRegister& now, MatcherRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            if (now.readEnableCycleMatch[i]) {
                auto row = matcherConfig[i].tableSId[now.hashValue[i] >> LOG_SRAM_SIZE];
                vector<int> keySId   = vector<int> (row.begin(), row.begin() + matcherConfig[i].keySWidth);
                vector<int> valueSId = vector<int> (row.begin() + matcherConfig[i].keySWidth, row.end());
                next.keyMatchCycleCompare  [i] = sram.get(keySId,   now.hashValue[i] % SRAM_SIZE);
                next.valueMatchCycleCompare[i] = sram.get(valueSId, now.hashValue[i] % SRAM_SIZE);
            }
        }
    }
    //todo: Cuckoo hash

    void compare(const MatcherRegister& now, MatcherRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            if (now.readEnableCycleCompare[i]) {
                if (now.keyMatchCycleCompare[i] == now.keyCycleCompare[i]) {
                    next.valueCycleOutput[i] = now.valueMatchCycleCompare[i];
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
