cmake_minimum_required(VERSION 3.22)
project(RPISA_sw)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

add_executable(RPISA_sw
        defs.h
        main.cpp
        RPISA/Schedule.h
        PISA/Register/SRAM.h
        PISA/Register/Register.h
        dataplane_config.h
        PISA/match.h
        PISA/action.h
        pipeline.h
        RPISA/Register/RRegister.h
        Switch/Switch.h
        Switch/Parser.h
        )
