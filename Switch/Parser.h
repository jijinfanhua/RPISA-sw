//
// Created by Walker on 2023/1/29.
//

#ifndef RPISA_SW_PARSER_H
#define RPISA_SW_PARSER_H

#include <iostream>
#include <bitset>
#include <string>
#include "../defs.h"

using byte = unsigned char;
struct Field {
    int start, length;
};
using ParserEntry = vector<Field>;
using Packet      = deque<byte>;
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

u32 bytes_to_u32(byte byte_1, byte byte_2, byte byte_3, byte byte_4){
    return ((u32)byte_1 << 24) + ((u32)byte_2 << 16) + ((u32)byte_3 << 8) + (u32)byte_4;
}

u32 bytes_to_u32(byte byte_1, byte byte_2){
    return ((u32)byte_1 << 8) + (u32)byte_2;
}

struct Parser {
    vector<ParserConfig> config;
    PHV parse(const Packet& packet) {
        PHV phv{};
        phv[0] = packet[12];
        phv[64] = bytes_to_u32(packet[8], packet[9]);
        phv[65] = bytes_to_u32(packet[10], packet[11]);
        phv[160] = bytes_to_u32(packet[0], packet[1], packet[2], packet[3]);
        phv[161] = bytes_to_u32(packet[4], packet[5], packet[6], packet[7]);
        phv[2] = packet[13];
        return phv;
    }
};

string division(string str, int m, int n, int & remain){
    string result = "";
    int a;
    remain = 0;

    for(int i = 0; i < str.size(); i++){
        a = (n * remain + (str[i] - '0'));
        str[i] = a / m + '0';
        remain = a % m;
    }
    //去掉多余的0 比如10/2=05
    int pos = 0;
    while(str[pos] == '0'){
        pos++;
    }
    return str.substr(pos);
}

string conversion(string str, int m, int n){
    string result = "";
    char c;
    int a;
    //因为去掉了多余的0，所以终止条件是字符串为空 例：当上一步运算结果为"0"时，实际上返回的结果为""
    while(str.size() != 0){
        str = division(str, m , n,a);
        result = char(a + '0') +result;

    }
    return result;
}

Packet input_to_packet(const string& input, const string& pkt_length){
    Packet output = Packet();
    // src ip 32 bit; dst ip 32 bit; src_addr 16 bit; dst addr 16 bit; is_tcp 1 bit
    string result = conversion(input, 2, 10);
    bitset<97> bits(result);
    string bitsStr = bits.to_string();

    for(int i = 0; i < 12; i++){
        byte input_byte = bitset<8>(bitsStr.substr(i*8, 8)).to_ulong();
        output.push_back(input_byte);
    }
    output.push_back(bitset<1>(bitsStr.substr(96, 1)).to_ulong());

    output.push_back(stoi(pkt_length));

    return output;
}



#endif //RPISA_SW_PARSER_H
