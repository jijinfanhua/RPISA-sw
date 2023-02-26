//
// Created by Walker on 2023/1/12.
//

#include <iostream>
using namespace std;

#include "defs.h"
#include "Switch/Switch.h"

Packet fake_packet(){
    Packet input_packet = Packet();
    input_packet.push_back(1);
    input_packet.push_back(2);
    input_packet.push_back(3);
    input_packet.push_back(4);
    input_packet.push_back(5);
    input_packet.push_back(6);
    input_packet.push_back(7);
    input_packet.push_back(8);
    input_packet.push_back(9);
    input_packet.push_back(10);
    input_packet.push_back(11);
    input_packet.push_back(12);
    input_packet.push_back(13);
    return input_packet;
}

int main(int argc, char** argv) {

    int cycle = 0;
    Switch switch_ = Switch();
    switch_.Config();
    while (true) {
        std::cout << "cycle: " << cycle << endl;
        if(cycle == 0) {
            switch_.Execute(1, fake_packet());
        }
        else{
            switch_.Execute(0, Packet());
        }
        cycle += 1;
    }
    return 0;
}
