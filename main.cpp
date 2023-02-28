//
// Created by Walker on 2023/1/12.
//

#include <iostream>
#include <fstream>
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

string read_from_file(ifstream& fin){
    string time;
    string tuple;
    string length;
    fin >> time;
    fin >> tuple;
    fin >> length;
    return tuple;
}

int main(int argc, char** argv) {

    int cycle = 0;
    Switch switch_ = Switch();
    ifstream infile;
    infile.open("D:\\code\\RPISA-sw\\part_trace.txt");
    switch_.Config();
    for(int i = 0; i < 900; i++) {
        std::cout << "cycle: " << cycle << endl;
//        switch_.Execute(1, fake_packet());
        if(cycle % 5 == 0)
            switch_.Execute(1, input_to_packet(read_from_file(infile)));
        else
            switch_.Execute(0, fake_packet());
//        if(cycle == 0) {
//            switch_.Execute(1, input_to_packet("31917286809818793201644399359"));
//        }
//        else{
//            switch_.Execute(0, Packet());
//        }

        cycle += 1;
    }
    return 0;
}
