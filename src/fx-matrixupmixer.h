#pragma once

#include "fx-base.h"
#include "defines.h"

namespace OMC
{

class FxMatrixUpmixer : public FxBase
{
    using enum  MP_ID;

    public:
        FxMatrixUpmixer(X32BaseParameter* basepar) : FxBase(basepar) {
        }

        String GetName() override{
            return "MatrixUpmixer";
        }
};

}