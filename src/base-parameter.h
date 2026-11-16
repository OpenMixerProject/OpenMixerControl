#pragma once

#include "../lib/CLI11.hpp"
#include "config.h"
#include "state.h"
#include "helper.h"

namespace OMC
{

    class X32BaseParameter{
        public:
            CLI::App* app;
            Config* config;    
            State* state;
            Helper* helper;

            X32BaseParameter(CLI::App* a, Config* c, State* s, Helper* h){
                this->app = a;
                this->config = c;
                this->state = s;
                this->helper = h;
            }
    };

}