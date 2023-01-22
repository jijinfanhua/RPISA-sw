cmake_minimum_required(VERSION 3.22)
project(RPISA_sw)

set(CMAKE_CXX_STANDARD 14)

add_executable(RPISA_sw
        defs.h
        main.cpp RPISA/Schedule.h PISA/Getkey.h PISA/Matcher.h PISA/Register/SRAM.h PISA/Executor.h RPISA/PO.h PipeLine.h RPISA/Register/Register.h PISA/Register/Register.h util/BitArray.h dataplane_config.h)
