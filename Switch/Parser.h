//
// Created by Walker on 2023/1/29.
//

#ifndef RPISA_SW_PARSER_H
#define RPISA_SW_PARSER_H

#include <iostream>
#include <bitset>
#include <string>
#include "../defs.h"
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
        return phv;
    }
};

Packet input_to_packet(const string& input){
    // src ip 32 bit; dst ip 32 bit; src_addr 16 bit; dst addr 16 bit; is_tcp 1 bit
    Packet packet;
    bitset<97> bits(input);
    string bitsStr = bits.to_string();

    byte src_ip_1 = bitset<8>(bitsStr.substr(0, 8)).to_ulong();
    byte src_ip_2 = bitset<8>(bitsStr.substr(8, 8)).to_ulong();
    byte src_ip_3 = bitset<8>(bitsStr.substr(16, 8)).to_ulong();
    byte src_ip_4 = bitset<8>(bitsStr.substr(24, 8)).to_ulong();

    packet.push_back(src_ip_1);
    packet.push_back(src_ip_2);
    packet.push_back(src_ip_3);
    packet.push_back(src_ip_4);

//    uint32_t num2 = bitset<32>(bitsStr.substr(32, 32)).to_ulong();
//    uint16_t num3 = bitset<16>(bitsStr.substr(64, 16)).to_ulong();
//    uint16_t num4 = bitset<16>(bitsStr.substr(80, 16)).to_ulong();
//    uint8_t num5 = bitset<1>(bitsStr.substr(96, 1)).to_ulong();

}



#endif //RPISA_SW_PARSER_H
