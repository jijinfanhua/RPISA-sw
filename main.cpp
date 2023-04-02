//
// Created by Walker on 2023/1/12.
//

#include <iostream>
#include <fstream>
//#include <direct.h>
using namespace std;

#include "defs.h"
#include "Switch/Switch.h"

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

string PARENT_DIR = "C:\\Users\\PC\\Desktop\\code\\RPISA-sw\\cmake-build-debug\\";
string INPUT_FILE_NAME = "switch.txt";
std::array<bool, PROCESSOR_NUM> processor_selects = {true, true, true, true, true, true};
std::array<ofstream*, PROCESSOR_NUM> outputs{};
int unordered_flow_count = 0;

void init_outputs(const string& parent_dir){
    for(int i = 0; i < PROCESSOR_NUM; i++){
        if(processor_selects[i]){
            auto* output = new ofstream();
            auto output_file = parent_dir + "processor_" + to_string(i) + ".txt";
            output->open(output_file, ios_base::out);
            outputs[i] = output;
        }
    }
}

void testing_order(){
    for(auto item: arrive_id_by_flow){
        int before = -1;
        for(auto flow: item.second){
            if(flow > before){
                before = flow;
            }
            else{
                unordered_flow_count += 1;
                break;
            }
        }
    }
}

float average(std::vector<int>& v){
    float sum = 0;
    for(auto item: v){
        sum += item;
    }
    return sum / v.size();
}

int main(int argc, char** argv) {
    // phv[223]: packet id
    // phv[222]: tag
    // tag 为 proc_num 位的独热码，哪一位为 1 代表有该 proc 的 tag.

    ifstream infile;
    infile.open(PARENT_DIR + INPUT_FILE_NAME);
    init_outputs(PARENT_DIR);

    int cycle = 0;
    int pkt = 0;
    int output_pkt = 0;
    std::vector<int> execute_latency;
    Switch switch_ = Switch();
    switch_.Config();
    for(int i = 0; i < 200000; i++) {
//        if(cycle == 16348){
//            DEBUG_ENABLE = true;
//        }
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
            output_pkt += 1;
            execute_latency.push_back(cycle - output_arrive_id.second);
            if(arrive_id_by_flow.find(output_arrive_id.first) == arrive_id_by_flow.end()){
                arrive_id_by_flow.insert({output_arrive_id.first, std::vector<int>({output_arrive_id.second})});
            }
            else{
                arrive_id_by_flow.at(output_arrive_id.first).push_back(output_arrive_id.second);
            }
        }
        cycle += 1;
    }

    switch_.Log(outputs, processor_selects);

    testing_order();
    cout << "total flow: " << arrive_id_by_flow.size() << endl;
    cout << "unordered flow: " << unordered_flow_count << endl;

    cout << "output pkt: " << output_pkt << endl;
    cout << "input pkt: " << pkt << endl;
    cout << "average execute latency: " << average(execute_latency) << endl;
    return 0;
}
