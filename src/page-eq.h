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
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_1, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_LOWCUT_FREQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_LOWCUT_ENABLE);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_FREQ1, (uint)BANKING_EQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_LOWCUT_SLOPE);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_3, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_GAIN1, (uint)BANKING_EQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_HIGHCUT_ENABLE);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_4, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_Q1, (uint)BANKING_EQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_4, MixerparameterAction::CHANGE, BANKING_EQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_5, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_6, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_HIGHCUT_FREQ);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_EQ_ENABLE);

            DrawEq();
        }

        void OnChange(bool force_update) override
        {
            if (config->HasParametersChanged({CHANNEL_LOWCUT_FREQ, CHANNEL_LOWCUT_SLOPE, CHANNEL_LOWCUT_ENABLE, CHANNEL_HIGHCUT_FREQ, CHANNEL_HIGHCUT_ENABLE}) ||
                config->HasParametersChanged({MP_CAT::CHANNEL_EQ}) ||
                config->HasParametersChanged({BANKING_EQ, SELECTED_CHANNEL}) ||
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

        void DrawEq() {

            uint selectedChannelIndex = config->GetUint(SELECTED_CHANNEL);

            // Draw EQ only for normal and aux channels
            if (selectedChannelIndex >= int(X32_VCHANNEL_BLOCK::FXRET)) {
                return;
            }

            bool lowCutEnabled = config->GetBool(CHANNEL_LOWCUT_ENABLE, selectedChannelIndex);
            bool highCutEnabled = config->GetBool(CHANNEL_HIGHCUT_ENABLE, selectedChannelIndex);
            bool eqEnabled = config->GetBool(CHANNEL_EQ_ENABLE, selectedChannelIndex);
            float fs = (float)config->GetUint(SAMPLERATE);

            // Precompute all filter coefficients once outside the 200-pixel loop
            sBiquadCoeffs lcSections[2];
            if (lowCutEnabled)
            {
                fxmath->CalcLowcutBiquads(
                    config->GetFloat(CHANNEL_LOWCUT_FREQ, selectedChannelIndex),
                    (uint8_t)config->GetUint(CHANNEL_LOWCUT_SLOPE, selectedChannelIndex),
                    fs,
                    lcSections
                );
            }

            sBiquadCoeffs hcSection;
            if (highCutEnabled)
            {
                fxmath->CalcHighcutBiquad(
                    config->GetFloat(CHANNEL_HIGHCUT_FREQ, selectedChannelIndex),
                    fs,
                    &hcSection
                );
            }

            if (eqEnabled)
            {
                for (uint8_t i_peq = 0; i_peq < MAX_CHAN_EQS; i_peq++)
                {
                    fxmath->RecalcFilterCoefficients_PEQ(&state->rChannel[selectedChannelIndex].peq[i_peq]);
                }
            }

            // calculate the filter-response between 20 Hz and 20 kHz for LowCut, HighCut, and all 4 PEQs
            float eqValue[200];
            float freq;

            memset(&eqValue[0], 0, sizeof(eqValue));

            // draw the amplitude response over frequency
            int32_t* chartSeriesEqPoints = lv_chart_get_series_y_array(objects.current_channel_eq, chartSeriesEQ);
            
            for (uint16_t pixel = 0; pixel < 200; pixel++) {
                freq = 20.0f * powf(1000.0f, ((float)pixel/199.0f));

                // LowCut (6/12/18/24 dB/oct)
                if (lowCutEnabled)
                {
                    eqValue[pixel] += fxmath->CalcFrequencyResponse_PEQ(lcSections[0].a[0], lcSections[0].a[1], lcSections[0].a[2], lcSections[0].b[1], lcSections[0].b[2], freq, fs);
                    eqValue[pixel] += fxmath->CalcFrequencyResponse_PEQ(lcSections[1].a[0], lcSections[1].a[1], lcSections[1].a[2], lcSections[1].b[1], lcSections[1].b[2], freq, fs);
                }

                // HighCut (12 dB/oct)
                if (highCutEnabled)
                {
                    eqValue[pixel] += fxmath->CalcFrequencyResponse_PEQ(hcSection.a[0], hcSection.a[1], hcSection.a[2], hcSection.b[1], hcSection.b[2], freq, fs);
                }

                if (eqEnabled)
                {
                    // PEQ
                    for (uint8_t i_peq = 0; i_peq < MAX_CHAN_EQS; i_peq++)
                    {
                        sPEQ* peq = &state->rChannel[selectedChannelIndex].peq[i_peq];

                        eqValue[pixel] += fxmath->CalcFrequencyResponse_PEQ(
                            peq->a[0],
                            peq->a[1],
                            peq->a[2],
                            peq->b[1],
                            peq->b[2],
                            freq,
                            fs
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

                // LowCut (6/12/18/24 dB/oct)
                if (lowCutEnabled)
                {
                    phase += fxmath->CalcPhaseResponse_PEQ(lcSections[0].a[0], lcSections[0].a[1], lcSections[0].a[2], 1.0f, lcSections[0].b[1], lcSections[0].b[2], freq, fs);
                    phase += fxmath->CalcPhaseResponse_PEQ(lcSections[1].a[0], lcSections[1].a[1], lcSections[1].a[2], 1.0f, lcSections[1].b[1], lcSections[1].b[2], freq, fs);
                }

                // HighCut (12 dB/oct)
                if (highCutEnabled)
                {
                    phase += fxmath->CalcPhaseResponse_PEQ(hcSection.a[0], hcSection.a[1], hcSection.a[2], 1.0f, hcSection.b[1], hcSection.b[2], freq, fs);
                }

                if (eqEnabled)
                {
                    // PEQ  
                    for (uint8_t i_peq = 0; i_peq < MAX_CHAN_EQS; i_peq++)
                    {
                        sPEQ* peq = &state->rChannel[selectedChannelIndex].peq[i_peq];

                        phase += fxmath->CalcPhaseResponse_PEQ(
                            peq->a[0],
                            peq->a[1],
                            peq->a[2],
                            1.0f,
                            peq->b[1],
                            peq->b[2],
                            freq,
                            fs);
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