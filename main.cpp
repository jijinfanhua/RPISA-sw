//
// Created by Walker on 2023/1/12.
//

#include <iostream>
using namespace std;

#include "defs.h"
#include "Switch/Switch.h"


int main(int argc, char** argv) {
    
    while (true) {
        Packet input_packet = Packet();
        Switch switch_ = Switch();
        switch_.Execute(1, input_packet); 
    }
    return 0;
}
