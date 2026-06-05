#pragma once

#include "fx-base.h"
#include "defines.h"

class FxMatrixUpmixer : public FxBase
{
    public:
        FxMatrixUpmixer(X32BaseParameter* basepar) : FxBase(basepar) {
        }

        String GetName() override{
            return "MatrixUpmixer";
        }
};