#pragma once
#include "page.h"
using namespace std;

namespace OMC
{

class PageEffects: public Page {

    private:

        uint selectedFx = 0;
        uint selectedFxBefore = 0;

        uint banking = 0;
        uint bankingBefore = 0;

        vector<MP_CAT> filter_cat_fx = {
            MP_CAT::FX_PARAMETER_REVERB,
            MP_CAT::FX_PARAMETER_CHORUS,
            MP_CAT::FX_PARAMETER_DELAY,
            MP_CAT::FX_PARAMETER_TRANSIENTSHAPER,
            MP_CAT::FX_PARAMETER_OVERDRIVE,
            MP_CAT::FX_PARAMETER_MULTIBANDCOMPRESOR,
            MP_CAT::FX_PARAMETER_DYNAMICEQ
        };

        bool HasFx(uint slot)
        {
            return config->GetUint(FX_TYPE, slot) != (uint)FX_TYPE::NONE;
        }

        int ParameterCount(uint slot)
        {
            enum FX_TYPE fxtype = (enum FX_TYPE)config->GetUint(FX_TYPE, slot);

            if (fxtype == FX_TYPE::NONE)
            {
                return 0;
            }

            return config->GetFxParameterCountFromType(fxtype);
        }

    public:
        PageEffects(PageBaseParameter* pagebasepar) : Page(pagebasepar) {
            tabLayer0 = objects.maintab;
            tabIndex0 = 5;
        }

        void OnShow() override
        {
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_1, String(LV_SYMBOL_REFRESH) + "\nSelect FX");
            
            lv_table_set_column_count(objects.fxtable, 8);
            for (uint8_t i = 0; i < MAX_FX_SLOTS; i++) {
                lv_table_set_column_width(objects.fxtable, i, DISPLAY_RESOLUTION_X / 8);
            }
        }

        void OnChangeCustomEncoder(SurfaceElementId surface_element_id, int amount) override
        {
            if (surface_element_id == SurfaceElementId::DISPLAY_ENCODER_1)
            {
                // ENCODER 1 -> select new FX
                // calculate new FX-Type based on current FX-Type and encoder turn amount
                enum FX_TYPE newFxType = (enum FX_TYPE)(config->GetUint(FX_TYPE, selectedFx) + amount);
                if (newFxType < FX_TYPE::NONE)
                {
                    newFxType = (enum FX_TYPE)((int)FX_TYPE::__ELEMENT_COUNTER_DO_NOT_MOVE - 1);
                }
                else if (newFxType >= FX_TYPE::__ELEMENT_COUNTER_DO_NOT_MOVE)
                {
                    newFxType = FX_TYPE::NONE;
                }

                // install new effect
                config->Set(FX_TYPE, (uint)newFxType, selectedFx);

                // reset Banking
                banking = 0;
                bankingBefore = 0;

                // force update of page
                OnChange(true);
                SyncEncoderWidgets(true);
            }
        }
        
        void OnChange(bool force_update) override
        {
            if (config->HasParameterChanged(DISPLAY_LEFT))
            {
                config->SetParameterUnchanged(DISPLAY_LEFT);
                prevFX();
            }

            if (config->HasParameterChanged(DISPLAY_RIGHT))
            {
                config->SetParameterUnchanged(DISPLAY_RIGHT);
                nextFX();                
            }

            if (config->HasParameterChanged(DISPLAY_UP))
            {
                prevParameterBank();
            }

            if (config->HasParameterChanged(DISPLAY_DOWN))
            {
                nextParameterBank();
            }


            bool parameterChanged = config->GetChangedParameterIndexes(filter_cat_fx).size() > 0;
            bool selectionChanged = selectedFx != selectedFxBefore;
            bool bankingChanged = banking != bankingBefore;

            if(parameterChanged || selectionChanged || bankingChanged || force_update)
            {
                for (uint slotIndex = 0; slotIndex < MAX_FX_SLOTS; slotIndex++)
                {   
                    if (selectionChanged || bankingChanged || force_update)
                    {
                        if (slotIndex == selectedFx) {
                            
                            // FX number (selected)
                            lv_table_set_cell_value_fmt(objects.fxtable, 0, selectedFx*2, "FX%d <<<", selectedFx+1);
                            lv_table_set_selected_cell(objects.fxtable, 0, selectedFx*2);

                            if (HasFx(slotIndex)) {

                                // reset banking, if FX has less parameters
                                if (ParameterCount(slotIndex) <= (banking * 5))
                                {
                                    banking = 0;
                                }

                                for (uint8_t e = 0; e < (MAX_DISPLAY_ENCODER - 1); e++)
                                {
                                    SurfaceElementId surface_element_id = (SurfaceElementId)(((uint)(SurfaceElementId::DISPLAY_ENCODER_2)) + e);
                                    MP_ID parameter_id = config->GetFxParameterFromType((enum FX_TYPE)config->GetUint(FX_TYPE, slotIndex), e + (banking * 5));

                                    if (parameter_id == NONE)
                                    {
                                        config->SurfaceUnbind(surface_element_id);
                                    }
                                    else
                                    {
                                        config->SurfaceBind(surface_element_id, MixerparameterAction::CHANGE, parameter_id, selectedFx);
                                    }
                                }

                            } else {
                                // slot has no FX -> show nothing

                                banking = 0;

                                for (uint8_t e = 0; e < (MAX_DISPLAY_ENCODER - 1); e++)
                                {
                                    config->SurfaceUnbind((SurfaceElementId)(((uint)(SurfaceElementId::DISPLAY_ENCODER_2)) + e));
                                }
                            }

                        } else {
                            // FX number (not selected)
                            lv_table_set_cell_value_fmt(objects.fxtable, 0, slotIndex * 2, "FX%d", slotIndex+1);
                        }
                    }
                    
                    if(parameterChanged || force_update)
                    {
                        // FX Name
                        String name = HasFx(slotIndex) ? config->GetParameter(FX_TYPE)->GetFormatedValue(slotIndex) : "No FX";
                        lv_table_set_cell_value(objects.fxtable, 1, slotIndex * 2, name.c_str());

                        // FX Parameters
                        uint8_t p = 0;
                        for (; p < ParameterCount(slotIndex); p++)
                        {
                            MP_ID parameter_id = config->GetFxParameterFromType((enum FX_TYPE)config->GetUint(FX_TYPE, slotIndex), p);
                            Mixerparameter *parameter = config->GetParameter(parameter_id);
                            lv_table_set_cell_value(objects.fxtable, p + 3, (slotIndex * 2), parameter->GetName().c_str());
                            lv_table_set_cell_value(objects.fxtable, p + 3, (slotIndex * 2) + 1, parameter->GetFormatedValue(slotIndex).c_str());
                        }
                        // clear parameters until row 100
                        for (; p < 100; p++){
                            lv_table_set_cell_value(objects.fxtable, p + 2, (slotIndex * 2), "");
                            lv_table_set_cell_value(objects.fxtable, p + 2, (slotIndex * 2) + 1, "");
                        }
                    }
                }

                selectedFxBefore = selectedFx;
                bankingBefore = banking;
            }
        }

        void nextParameterBank()
        {
            if (HasFx(selectedFx) && (ParameterCount(selectedFx) > ((banking + 1) * 5)))
            {
                banking++;
            }
        }

        void prevParameterBank()
        {
            if (banking > 0)
            {
                banking--;
            }
            else
            {
                banking = 0;
            }
        }

        void nextFX()
        {
            if (selectedFx < MAX_FX_SLOTS - 1)
            {
                selectedFx++;
            }
            else
            {
                selectedFx = 0;
            }
        }

        void prevFX()
        {
            if (selectedFx > 0)
            {
                selectedFx--;
            }
            else
            {
                selectedFx = MAX_FX_SLOTS - 1;
            }
        }
};

}