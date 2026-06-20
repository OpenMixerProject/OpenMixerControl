#pragma once

#include "base-parameter.h"
#include "../lib/CLI11.hpp"
#include "x32config.h"
#include "state.h"
#include "helper.h"
#include "surface.h"

class PageBaseParameter : public X32BaseParameter {
    public:
        Surface* surface;

        PageBaseParameter(CLI::App* a, X32Config* c, State* s, Helper* h, Surface* su) : X32BaseParameter(a, c, s, h)
        {
            surface = su;
        }
};