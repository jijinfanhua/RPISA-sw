//
// Created by Walker on 2023/1/29.
//

#ifndef RPISA_SW_PARSER_H
#define RPISA_SW_PARSER_H

#include "../defs.h"
struct Field {
    int start, length;
};
using ParserEntry = vector<Field>;
using Packet      = deque<std::byte>;
struct Node {
    struct FieldInfo {
        int phv_id;
        int field_length;
    };
    vector<FieldInfo> parseBehaviour;
    void parse(PHV& phv, const Packet& packet) {

    }

};

struct Edge {
    ParserEntry entry;
    int value;
    Node* nextNode;
};

struct ParserConfig {

    ParserEntry entry;



};

struct Parser {
    vector<ParserConfig> config;
    PHV parse(const Packet& packet) {
        return {};
    }
};



#endif //RPISA_SW_PARSER_H
