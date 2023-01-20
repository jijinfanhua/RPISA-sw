//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_MATCHER_H
#define RPISA_SW_MATCHER_H

#include "../PipeLine.h"



struct MatcherConfig {
    struct TableConfig {
        vector<vector<int>> tableSId;
        int keySWidth;
    };
    vector<TableConfig> tables;
};



struct Matcher : public Logic {

    SRAMs& sram;
    array<MatcherConfig, READ_TABLE_NUM> matcherConfig;

    void execute(const PipeLine &now, PipeLine &next) override {
        MatcherRegister& nextMatcher  = next.processors[processor_id].matcher;
        const MatcherRegister& matcher = now.processors[processor_id].matcher;
        nextMatcher.keyCycleCompare = matcher.keyCycleMatch;
        nextMatcher.readEnableCycleCompare = matcher.readEnableCycleMatch;
        nextMatcher.readEnableOutput[0] = matcher.readEnableCycleCompare;
        match  (matcher, nextMatcher);
        compare(matcher, nextMatcher);
        for (int i = 1; i < MATCHER_ALL_CYCLE; i++) {
            nextMatcher.phv[i] = matcher.phv[i - 1];
        }
        for (int i = 1; i < MATCHER_ALL_CYCLE - 1; i++) {
            nextMatcher.valueCycleOutput[i] = matcher.valueCycleOutput[i - 1];
            nextMatcher.compare[i] = matcher.compare[i - 1];
            nextMatcher.readEnableOutput[i] = matcher.readEnableOutput[i - 1];
        }
    }





    void match(const MatcherRegister& now, MatcherRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            if (now.readEnableCycleMatch[i]) {
                int j = 0;
                for (const auto& table : matcherConfig[i].tables) {
                    auto row = table.tableSId[now.addressCycleMatch[i][j] >> LOG_SRAM_SIZE];
                    vector<int> keySId   = vector<int> (row.begin(), row.begin() + table.keySWidth);
                    vector<int> valueSId = vector<int> (row.begin() + table.keySWidth,   row.end());
                    next.keyMatchCycleCompare  [i][j] = sram.get(keySId,   now.addressCycleMatch[i][j] % SRAM_SIZE);
                    next.valueMatchCycleCompare[i][j] = sram.get(valueSId, now.addressCycleMatch[i][j] % SRAM_SIZE);
                    j++;
                }
            }
        }
    }

    void compare(const MatcherRegister& now, MatcherRegister& next) {
        for (int i = 0; i < READ_TABLE_NUM; i++) {
            if (now.readEnableCycleCompare[i]) {
                int j;
                for (j = 0; j < ADDRESS_WAY; j++) {
                    if (now.keyMatchCycleCompare[i][j] == now.keyCycleCompare[i]) {
                        next.valueCycleOutput[0][i] = now.valueMatchCycleCompare[i][j];
                        next.compare[0][i] = true;
                        break;
                    }
                }
                if (j == ADDRESS_WAY) next.compare[0][i] = false;
            } else {
                next.compare[0][i] = false;
            }

        }
    }
};

#endif //RPISA_SW_MATCHER_H
