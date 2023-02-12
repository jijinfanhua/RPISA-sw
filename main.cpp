//
// Created by Walker on 2023/1/12.
//

#include <iostream>
using namespace std;

#include "defs.h"
#include "pipeline.h"


int main(int argc, char** argv) {
    PipeLine pipeline = PipeLine(); // 初始化
    while (true) {
        pipeline.log();
        pipeline = pipeline.execute(); // execute 代表一次时钟上升沿到来，开始计算下一个状态
    }
    return 0;
}
