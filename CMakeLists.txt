cmake_minimum_required(VERSION 3.22)
project(RPISA_sw)

set(CMAKE_CXX_STANDARD 14)

add_executable(RPISA_sw
        defs.h
        main.cpp RPISA/PI.h PISA/Gateway.h PISA/Matcher.h PISA/SRAM.h PISA/Executor.h)
