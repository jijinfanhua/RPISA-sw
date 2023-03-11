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

std::unordered_map<u64, std::vector<int>> arrive_id_by_flow;

string PARENT_DIR = "/tools/oldz/";
string INPUT_FILE_NAME = "switch.txt";
std::array<bool, PROC_NUM> processor_selects = {true, true, true, true, true, true};
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

bool testing_order(){
    for(auto item: arrive_id_by_flow){
        int before = -1;
        for(auto flow: item.second){
            if(flow > before){
                before = flow;
            }
            else{
                return false;
            }
        }
    }
    return true;
}

int main(int argc, char** argv) {


    ifstream infile;
    infile.open(PARENT_DIR + INPUT_FILE_NAME);
    init_outputs(PARENT_DIR);

    int cycle = 0;
    int pkt = 0;
    Switch switch_ = Switch();
    switch_.Config();
    for(int i = 0; i < 200000; i++) {
        std::cout << "cycle: " << cycle << endl << endl;
//        if(cycle % 7 == 0){
//                switch_.Execute(0, Packet());
//        }
//        else{
            string input = read_from_file(infile);
            if(input == ""){
                switch_.Execute(0, Packet(), cycle);
            }
            else{
                pkt += 1;
                switch_.Execute(1, input_to_packet(input), cycle);
            }
//        }

        auto output_arrive_id = switch_.get_output_arrive_id();
            if(output_arrive_id.first != -1){
                if(arrive_id_by_flow.find(output_arrive_id.first) == arrive_id_by_flow.end()){
                    arrive_id_by_flow.insert({output_arrive_id.first, std::vector<int>({output_arrive_id.second})});
                }
                else{
                    arrive_id_by_flow.at(output_arrive_id.first).push_back(output_arrive_id.second);
                }
            }
        switch_.Log(outputs, processor_selects);
        cycle += 1;
    }

    auto ordered = testing_order();
    cout << ordered << endl;

    cout << pkt << endl;
    return 0;
}
