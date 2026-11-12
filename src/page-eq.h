#pragma once

#include "page.h"

using namespace std;

namespace OMC
{

class PageEq: public Page {

    using enum MP_ID;

    public:
        PageEq(PageBaseParameter* pagebasepar) : Page(pagebasepar) {
            prevPage = X32_PAGE::COMPRESSOR;
            nextPage = X32_PAGE::SENDS;
            tabLayer0 = objects.maintab;
            tabIndex0 = 0;
            tabLayer1 = objects.hometab;
            tabIndex1 = 4;
            noLedOnRack = true;

            fxmath = new FxMath(pagebasepar);
        }

        void OnInit() override {
            // setup EQ-graph
            lv_chart_set_type(objects.current_channel_eq, LV_CHART_TYPE_LINE);
            lv_obj_set_style_size(objects.current_channel_eq, 0, 0, LV_PART_INDICATOR);
            lv_obj_set_style_line_width(objects.current_channel_eq, 5, LV_PART_ITEMS);
            chartSeriesEQ = lv_chart_add_series(objects.current_channel_eq, lv_palette_main(LV_PALETTE_AMBER), LV_CHART_AXIS_PRIMARY_Y);
            chartSeriesEQPhase = lv_chart_add_series(objects.current_channel_eq, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_SECONDARY_Y);
            lv_chart_set_div_line_count(objects.current_channel_eq, 5, 7); // hdiv_num, vdiv_num
            lv_chart_set_range(objects.current_channel_eq, LV_CHART_AXIS_PRIMARY_Y, -15000, 15000);
            lv_chart_set_range(objects.current_channel_eq, LV_CHART_AXIS_SECONDARY_Y, -15000, 15000);
            lv_chart_set_point_count(objects.current_channel_eq, 200);
            lv_chart_set_series_color(objects.current_channel_eq, chartSeriesEQ, lv_color_hex(0xef7900));
            lv_chart_set_series_color(objects.current_channel_eq, chartSeriesEQPhase, lv_color_hex(0x008000));
            //chart-shadow: 0x7e4000
        }

        void OnShow() override
        {
            BindEncoders();
            DrawEq();
        }

        void OnChange(bool force_update) override
        {
            if (config->HasParameterChanged(EQ_PAGE_EDIT_ALTEQ))
            {
                BindEncoders();
            }

            if (config->HasParameterChanged(CHANNEL_LOWCUT_FREQ) ||
                config->HasParametersChanged({MP_CAT::CHANNEL_EQ}) ||
                config->HasParametersChanged({BANKING_EQ, BANKING_ALTEQ, EQ_PAGE_EDIT_ALTEQ, SELECTED_CHANNEL}) ||
                force_update
            )
            {
                DrawEq();
                SyncEncoderWidgets(true);
            }
        }

    private:
        lv_chart_series_t* chartSeriesEQ;
        lv_chart_series_t* chartSeriesEQPhase;
        FxMath* fxmath;

        // encoder button 5 switches between the channel EQ and the alternative EQ (e.g. occlusion EQ for in-ear monitoring)
        void BindEncoders()
        {
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_1, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_LOWCUT_FREQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_LOWCUT_ENABLE);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_5, MixerparameterAction::TOGGLE, EQ_PAGE_EDIT_ALTEQ);

            if (config->GetBool(EQ_PAGE_EDIT_ALTEQ))
            {
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_ALTEQ_FREQ1, (uint)BANKING_ALTEQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_3, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_ALTEQ_GAIN1, (uint)BANKING_ALTEQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_4, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_ALTEQ_Q1, (uint)BANKING_ALTEQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_5, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_ALTEQ_TYPE1, (uint)BANKING_ALTEQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_6, MixerparameterAction::CHANGE, BANKING_ALTEQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_ALTEQ_ENABLE);
            }
            else
            {
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_FREQ1, (uint)BANKING_EQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_3, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_GAIN1, (uint)BANKING_EQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_4, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_Q1, (uint)BANKING_EQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_5, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_6, MixerparameterAction::CHANGE, BANKING_EQ);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_EQ_ENABLE);
            }
        }

        void DrawEq() {

            uint selectedChannelIndex = config->GetUint(SELECTED_CHANNEL);

            // Draw EQ only for normal and aux channels
            if (selectedChannelIndex >= int(X32_VCHANNEL_BLOCK::FXRET)) {
                return;
            }

            // the alternative EQ is drawn alone, it is applied on top of the channel processing
            bool editAltEq = config->GetBool(EQ_PAGE_EDIT_ALTEQ);
            sPEQ* peqs = editAltEq ? state->rChannel[selectedChannelIndex].altPeq : state->rChannel[selectedChannelIndex].peq;
            uint peqCount = editAltEq ? MAX_CHAN_ALTEQS : MAX_CHAN_EQS;

            bool lowCutEnabled = !editAltEq && config->GetBool(CHANNEL_LOWCUT_ENABLE, selectedChannelIndex);
            bool eqEnabled = config->GetBool(editAltEq ? CHANNEL_ALTEQ_ENABLE : CHANNEL_EQ_ENABLE, selectedChannelIndex);

            // calculate the filter-response between 20 Hz and 20 kHz for all 4 PEQs
            float eqValue[200];
            float freq;

            memset(&eqValue[0], 0, sizeof(eqValue));

            // draw the amplitude response over frequency
            int32_t* chartSeriesEqPoints = lv_chart_get_series_y_array(objects.current_channel_eq, chartSeriesEQ);
            
            for (uint16_t pixel = 0; pixel < 200; pixel++) {
                freq = 20.0f * powf(1000.0f, ((float)pixel/199.0f));

                // LowCut
                if (lowCutEnabled)
                {
                    eqValue[pixel] += fxmath->CalcFrequencyResponse_LC(freq, config->GetFloat(CHANNEL_LOWCUT_FREQ, selectedChannelIndex), config->GetUint(SAMPLERATE));
                }

                if (eqEnabled)
                {
                    // PEQ
                    for (uint8_t i_peq = 0; i_peq < peqCount; i_peq++)
                    {
                        sPEQ* peq = &peqs[i_peq];

                        fxmath->RecalcFilterCoefficients_PEQ(peq);

                        eqValue[pixel] += fxmath->CalcFrequencyResponse_PEQ(
                            peq->a[0],
                            peq->a[1],
                            peq->a[2],
                            peq->b[1],
                            peq->b[2],
                            freq,
                            config->GetUint(SAMPLERATE)
                        );
                    }
                }

                // draw point
                chartSeriesEqPoints[pixel] = eqValue[pixel] * 1000.0f; // convert to primary Y-axis range (+/-15 -> +/-15,000)
            }

            // draw the phase response over frequency
            int32_t* chartSeriesEqPhasePoints = lv_chart_get_series_y_array(objects.current_channel_eq, chartSeriesEQPhase);
            for (uint16_t pixel = 0; pixel < 200; pixel++) {
                freq = 20.0f * powf(1000.0f, ((float)pixel/199.0f));
                float phase = 0.0f;

                // LowCut
                if (lowCutEnabled)
                {
                    phase += fxmath->CalcPhaseResponse_LC(freq, config->GetFloat(CHANNEL_LOWCUT_FREQ, selectedChannelIndex));
                }

                if (eqEnabled)
                {
                    // PEQ  
                    for (uint8_t i_peq = 0; i_peq < peqCount; i_peq++)
                    {
                        sPEQ* peq = &peqs[i_peq];

                        phase += fxmath->CalcPhaseResponse_PEQ(
                            peq->a[0],
                            peq->a[1],
                            peq->a[2],
                            1.0f,
                            peq->b[1],
                            peq->b[2],
                            freq,
                            config->GetUint(SAMPLERATE));
                    }
                }

                // limit phase to +/- PI
                while (phase > PI)  phase -= 2.0f * PI;
                while (phase < -PI) phase += 2.0f * PI;

                // draw point
                chartSeriesEqPhasePoints[pixel] = phase * (-15000.0f / PI); // convert to secondary Y-axis range (+/-PI -> +/-15,000)
            }

            lv_chart_refresh(objects.current_channel_eq);
        }
};

}