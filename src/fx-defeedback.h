#pragma once

#include "fx-base.h"
#include "defines.h"

class FxDeFeedback : public FxBase
{
    public:
        FxDeFeedback(X32BaseParameter* basepar) : FxBase(basepar) {
        }

        String GetName() override{
            return "DeFeedback";
        }
};