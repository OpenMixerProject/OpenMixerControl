#include "config.h"
#include "dsp1.h"
#include "mixer.h"
#include "../lib_ext/doctest/doctest/doctest.h"

namespace OMC
{
    TEST_CASE("OSC FX catalog uses all canonical names and oscillator reaches DSP2")
    {
        Helper helper{};
        auto* config = new Config("X32CORE", &helper, false);
        auto* paths = config->GetOscPaths();
        CHECK(paths->at("fx_reverb_rt60") == MP_ID::FX_REVERB_RT60);
        CHECK(paths->at("fx_dynamiceq_band3_release") == MP_ID::FX_DYNAMICEQ_BAND3_RELEASE);
        for (uint id = (uint)MP_ID::FX_TYPE; id <= (uint)MP_ID::FX_DYNAMICEQ_BAND3_RELEASE; ++id)
        {
            auto* parameter = config->GetParameter((MP_ID)id);
            CHECK(parameter->IsOSC());
            CHECK(parameter->GetInstances() == MAX_FX_SLOTS);
        }
        CHECK(config->GetParameter(MP_ID::FX_TYPE)->GetMax() == 9);
        CHECK(config->GetParameter(MP_ID::OSC_FREQUENCY)->GetInstances() == 2);
        CHECK(config->GetFloat(MP_ID::OSC_LEVEL, 1) == -120.0f);

        CLI::App app;
        State state{};
        state.bodyless = true;
        X32BaseParameter base(&app, config, &state, &helper);
        // Bodyless construction does not open DSP/FPGA hardware.
        auto* mixer = new Mixer(&base);
        config->Set(MP_ID::OSC_FREQUENCY, 440.0f, 0);
        config->Set(MP_ID::OSC_LEVEL, -20.0f, 0);
        config->Set(MP_ID::OSC_FREQUENCY, 880.0f, 1);
        config->Set(MP_ID::OSC_LEVEL, -40.0f, 1);
        mixer->Sync();
        CHECK(mixer->dsp->dsp2Oscillator[0].frequency == 440.0f);
        CHECK(mixer->dsp->dsp2Oscillator[0].volume == doctest::Approx(0.1f));
        CHECK(mixer->dsp->dsp2Oscillator[1].frequency == 880.0f);
        CHECK(mixer->dsp->dsp2Oscillator[1].volume == doctest::Approx(0.01f));
        CHECK_NOTHROW(mixer->dsp->DSP2_SetOscillator(2, 1000, 0));
        delete paths;
        delete mixer;
        delete config;
    }
}
