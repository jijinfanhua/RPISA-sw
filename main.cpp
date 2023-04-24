//
// Created by Walker on 2023/1/12.
//

#include <iostream>
#include <fstream>
#include <sstream>
//#include <direct.h>
using namespace std;

#include "defs.h"
#include "Switch/Switch.h"

FiveTuple read_five_tuple_from_file(const string& line){
    FiveTuple input{};
    std::stringstream ss(line);
    ss >> input.time >> input.src_ip >> input.dst_ip >> input.protocol >> input.src_port >> input.dst_port >> input.pkt_length;
    return input;
}

std::unordered_map<u64, std::vector<int>> arrive_id_by_flow;

std::unordered_map<string, int> flow_occurrence;

string PARENT_DIR = R"(C:\Users\PC\Desktop\code\RPISA-sw\cmake-build-debug\)";
std::array<bool, PROCESSOR_NUM> processor_selects = {true};
std::array<ofstream*, PROCESSOR_NUM> outputs{};
ofstream* main_output;
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

    main_output = new ofstream();
    auto output_file = parent_dir + "main" + ".txt";
    main_output->open(output_file, ios_base::out);

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
    // argc = 3
    // argv[1] = num of cycle; argv[2] = percent of throughput; argv[3] = percent of write back of processor 2
    // phv[223]: packet id
    // phv[222]: tag
    // tag 为 proc_num 位的独热码，哪一位为 1 代表有该 proc 的 tag.

    ifstream infile;
    infile.open(argv[6]);
    init_outputs(PARENT_DIR);

    float empty_controller = 0;
    float throughput_ratio = stof(argv[2]);

    dispatcher_queue_width = stoi(argv[3]);
    processing_function_num = stoi(argv[4]);
    max_queue_size_allowed = stoi(argv[5]);

    int cycle = 0;
    int pkt = 0;
    int output_pkt = 0;
    int bubble_left = 0;
    std::vector<int> execute_latency;
    Switch switch_ = Switch();
    switch_.Config();
    for(int i = 0; i < stoi(argv[1]) + 5 * dispatcher_queue_width * max_queue_size_allowed + processing_function_num; i++) {
        if(cycle > stoi(argv[1])){
            // add empty cycles to empty the pipeline
            switch_.Execute(0, Packet(), cycle);
        }
        else{
            empty_controller += throughput_ratio;
            if(empty_controller < 1){
                // add bubble to control throughput
                switch_.Execute(0, Packet(), cycle);
            }
            else{
                empty_controller -= 1;
                if(bubble_left == 0){
                    string line;
                    getline(infile, line);
                    auto input = read_five_tuple_from_file(line);
                    bubble_left = input.pkt_length / 64;
                    pkt += 1;
                    switch_.Execute(1, input_to_packet(input), cycle);
                }
                else{
                    // add bubble to balance pkt length
                    switch_.Execute(0, Packet(), cycle);
                    bubble_left--;
                }
            }
        }

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

    switch_.Log(outputs, processor_selects, cycle);

    testing_order();

    *main_output << "average execute latency: " << average(execute_latency) << endl;
    *main_output << "input_pkt: " << pkt << endl;
    *main_output << "output_pkt: " << output_pkt << endl;
    *main_output << "packet lost rate: " << float(switch_.pipeline->proc_states[0].packet_lost) / float(pkt) << endl;
    return 0;
}