//
// Created by Walker on 2023/1/12.
//

#include <iostream>
#include <fstream>
//#include <direct.h>
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
    string line;
    getline(fin, line);
    if(line == ""){
        return "";
    }
    else{
        int start_pos = line.find("'");
        int end_pos = line.find("'", start_pos+1);
        string result = line.substr(start_pos+1, end_pos-start_pos-1);
        return result;
    }
}

string PARENT_DIR = "D:\\code\\RPISA-sw\\cmake-build-debug\\";
string INPUT_FILE_NAME = "part_trace.txt";
std::array<bool, PROC_NUM> processor_selects = {true, false, true};
std::array<ofstream*, PROC_NUM> outputs{};

void init_outputs(const string& parent_dir){
    for(int i = 0; i < PROC_NUM; i++){
        if(processor_selects[i]){
            auto* output = new ofstream();
            auto output_file = parent_dir + "processor_" + to_string(i) + ".txt";
            output->open(output_file, ios_base::out);
            outputs[i] = output;
        }
    }
}

int main(int argc, char** argv) {

//    char *dir;
//    //也可以将buffer作为输出参数
//    if((dir = getcwd(nullptr, 0)) == nullptr)
//    {
//        perror("getcwd error");
//    }
//    else
//    {
//        PARENT_DIR = string(dir);
//
//    }

    ifstream infile;
    infile.open(PARENT_DIR + INPUT_FILE_NAME);
    init_outputs(PARENT_DIR);

    int cycle = 0;
    Switch switch_ = Switch();
    switch_.Config();
    for(int i = 0; i < 2000; i++) {
//        if(cycle % 100 == 0){
        std::cout << "cycle: " << cycle << endl << endl;
//        }
        if(cycle % 3 == 0) switch_.Execute(0, fake_packet());
        else{
            string input = read_from_file(infile);
            if(input == ""){
                switch_.Execute(0, Packet());
            }
            else{
                switch_.Execute(1, input_to_packet(input));
            }
        }

        switch_.Log(outputs, processor_selects);
        cycle += 1;
    }
    return 0;
}
