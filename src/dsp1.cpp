/*
    ____                  __   ______ ___  
   / __ \                 \ \ / /___ \__ \ 
  | |  | |_ __   ___ _ __  \ V /  __) | ) |
  | |  | | '_ \ / _ \ '_ \  > <  |__ < / / 
  | |__| | |_) |  __/ | | |/ . \ ___) / /_ 
   \____/| .__/ \___|_| |_/_/ \_\____/____|
         | |                               
         |_|                               
  
  OpenX32 - The OpenSource Operating System for the Behringer X32 Audio Mixing Console
  Copyright 2025 OpenMixerProject
  https://github.com/OpenMixerProject/OpenX32
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  version 3 as published by the Free Software Foundation.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
*/

#include "dsp1.h"

#include "const.h"
#include "defines.h"
#include "helper.h"
#include <cstdint>

namespace OMC
{
    
DSP1::DSP1(X32BaseParameter* basepar) : X32Base(basepar)
{
    spi = new SPI(basepar);
    if (!state->bodyless && !state->raspi && !config->IsModelAnyWing())
    {
        spi->UploadBitstreamDsps(true); // use CLI to show progress
        spi->OpenConnectionDsps();
    }
    
    fxmath = new FxMath(basepar);
    for (uint8_t i = 0; i < MAX_FX_SLOTS; i++){
        fx_slot[i] = new FxSlot(basepar);
    }
};

void DSP1::Init()
{
    for (uint8_t chanIndex = 0; chanIndex < 40; chanIndex++)
    {
         for (uint8_t peqIndex = 0; peqIndex < MAX_CHAN_EQS; peqIndex++)
         {
            rChannel[chanIndex].peq[peqIndex].type = config->GetUint(config->MpCalcId(CHANNEL_EQ_TYPE1, peqIndex));
            rChannel[chanIndex].peq[peqIndex].fc = config->GetFloat(config->MpCalcId(CHANNEL_EQ_FREQ1, peqIndex));
            rChannel[chanIndex].peq[peqIndex].Q = config->GetFloat(config->MpCalcId(CHANNEL_EQ_Q1, peqIndex));
            rChannel[chanIndex].peq[peqIndex].gain = config->GetFloat(config->MpCalcId(CHANNEL_EQ_GAIN1, peqIndex));
        }
    }
}

bool DSP1::ChannelHasAdjustableGain(uint chanIndex)
{
    // check if channel has an adjustable gain
    uint dspChannelInputRouting = config->GetUint(ROUTING_DSP_INPUT, chanIndex);
    uint dspChannelFpgaSource = config->GetUint(ROUTING_FPGA, chanIndex);

    // check if channel uses external signal from FPGA
    if ((dspChannelInputRouting >= DSP_BUF_IDX_DSPCHANNEL) && (dspChannelInputRouting < (DSP_BUF_IDX_AUX + 8)))
    {
        // this channel gets its input from the FPGA. Now check if it has an adjustable gain
        if (((dspChannelFpgaSource >= FPGA_INPUT_IDX_XLR) && (dspChannelFpgaSource < FPGA_INPUT_IDX_XLR + 32)) ||
            (dspChannelFpgaSource >= FPGA_INPUT_IDX_AES50A && dspChannelFpgaSource < FPGA_INPUT_IDX_AES50A + 48))
        {
            return true;
        }
    }

    return false;
}

float DSP1::CompensateGainAndVolume(float targetGainDb, float targetVolumeDb)
{
    float clampedGainDb = max(-2.0f, min(45.5f, targetGainDb));
    float stepIndex = floor((clampedGainDb - (-2.0f)) / 2.5f);
    float hardwareGainDb = -2.0f + (stepIndex * 2.5f);

    float missingGainDb = targetGainDb - hardwareGainDb;
    float finalDigitalVolumeDb = targetVolumeDb + missingGainDb;

    return finalDigitalVolumeDb;
}

// set the general volume of one of the 40 DSP-channels, 8 FX-Returns and all Mixbusses
void DSP1::SendChannelVolume(uint chanIndex)
{
    float balanceLeft = helper->Saturate(100.0f - config->GetFloat(CHANNEL_PANORAMA, chanIndex), 0.0f, 100.0f) / 100.0f;
    float balanceRight = helper->Saturate(config->GetFloat(CHANNEL_PANORAMA, chanIndex) + 100.0f, 0.0f, 100.0f) / 100.0f;
    float volumeLR = config->GetFloat(CHANNEL_VOLUME, chanIndex);
    float volumeSub = config->GetFloat(CHANNEL_VOLUME_SUB, chanIndex);

    if (!config->GetBool(CHANNEL_SEND_LR, chanIndex) || config->GetBool(CHANNEL_MUTE, chanIndex)) {
        volumeLR = VOLUME_MIN; // dB
    }
    if (!config->GetBool(CHANNEL_SEND_SUB, chanIndex) || config->GetBool(CHANNEL_MUTE, chanIndex)) {
        volumeSub = VOLUME_MIN; // dB
    }

    // convert volume from dB to linear
    float volumeLR_pu;
    float volumeSub_pu;

    // check if current channel has an adjustable gain. If not, apply GAIN as TRIM
    if (ChannelHasAdjustableGain(chanIndex))
    {
        // apply digital gain to increase gain-resolution as hardware supports 2.5dB-steps "only"
        float volumeLR_new = CompensateGainAndVolume(config->GetFloat(CHANNEL_GAIN, chanIndex), volumeLR);
        float volumeSub_new = CompensateGainAndVolume(config->GetFloat(CHANNEL_GAIN, chanIndex), volumeSub);
        volumeLR_pu = pow(10.0f, volumeLR_new/20.0f);
        volumeSub_pu = pow(10.0f, volumeSub_new/20.0f);
    }
    else
    {
        // GAIN as TRIM
        float trim = config->GetFloat(CHANNEL_GAIN, chanIndex);
        float trim_pu = pow(10.0f, trim/20.0f);
        volumeLR_pu = pow(10.0f, volumeLR/20.0f) * trim_pu;
        volumeSub_pu = pow(10.0f, volumeSub/20.0f) * trim_pu;
    }

    // apply DCAs if enabled
    // loop through all DCA groups
    for (uint i = 0; i < DCA_GROUPS; i++)
    {
        MP_ID dcaGroupId = config->MpCalcId(DCA_GROUP_1, i);

        // check if we are part of the DCA group
        if (config->GetBool(dcaGroupId, chanIndex))
        {
            // current channel is part of this DCA group -> apply DCA value to volume
            float dcaValue = config->GetFloat(CHANNEL_VOLUME, (uint)X32_VCHANNEL_BLOCK::DCA + i);
            float dcaValue_pu = pow(10.0f, dcaValue/20.0f);
            volumeLR_pu *= dcaValue_pu;
            volumeSub_pu *= dcaValue_pu;
        }
    }

    // send volume to DSP via SPI
    float values[4];
    values[0] = volumeLR_pu; // volume of this specific channel
    values[1] = balanceLeft; // 1 .. 1 ..  0
    values[2] = balanceRight; // 0  .. 1 .. 1
    values[3] = volumeSub_pu; // subwoofer

    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendChannelVolume() channelindex %d: %f, %f, %f, %f", chanIndex, (double)values[0], (double)values[1], (double)values[2], (double)values[3]);

    spi->QueueDspData(0, 'v', chanIndex, 0, 4, &values[0]);
}

void DSP1::SendChannelSolo(uint chanIndex)
{
    uint32_t values[2];

    values[0] = config->GetBool(CHANNEL_SOLO, chanIndex);
    values[1] = config->GetBool(SOLO_ACTIVE);

    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendChannelSolo() channelindex %d: channel solo %u, solo active %u", chanIndex, values[0], values[1]);

    spi->QueueDspData(0, 'v', chanIndex, 10, 2, (float*)&values[0]);
}

// send BusSends
void DSP1::SendChannelSend(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendChannelSend() channelindex %d", chanIndex);

    float values[16];
    for (uint8_t i_mixbus = 0; i_mixbus < 16; i_mixbus++)
    {
        float sendVol = config->GetFloat(config->MpCalcId(CHANNEL_BUS_SEND01, i_mixbus), chanIndex);

        values[i_mixbus] = pow(10.0f, sendVol/20.0f); // volume of this specific channel
    }

    spi->QueueDspData(0, 's', chanIndex, 0, 16, &values[0]);
}

void DSP1::ChannelSendTapPoints(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "ChannelSendTapPoints() for channelindex %d", chanIndex);

    for (uint8_t sendChannel = 0; sendChannel < 16; sendChannel++)
    {
        uint32_t values[2];
        uint dspindex = 0;
        values[0] = sendChannel;
        values[1] = config->GetUint(config->MpCalcId(CHANNEL_BUS_SEND01_TAPPOINT, sendChannel), chanIndex);

        // Mixbus
        if (chanIndex >= 48) // Mixbus
        {
            // Mixbus has only 6 sends to matrix
            if (sendChannel > 5)
            {
                return;
            }

            dspindex = 1;
            chanIndex = chanIndex - 48;
        }

        spi->QueueDspData(0, 't', chanIndex, dspindex, 2, (float*)&values[0]);
    }
}

void DSP1::SendMatrixVolume(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendMatrixVolume() channelindex %d", chanIndex);

    // send volume to DSP via spi->
    float values[1];
    float sendVol = VOLUME_MIN;

    if (!config->GetBool(CHANNEL_MUTE, chanIndex))
    {
        sendVol = config->GetFloat(CHANNEL_VOLUME, chanIndex);
    }

    float sendVol_pu = pow(10.0f, sendVol/20.0f);

    // apply DCAs if enabled
    // loop through all DCA groups
    for (uint i = 0; i < DCA_GROUPS; i++)
    {
        MP_ID dcaGroupId = config->MpCalcId(DCA_GROUP_1, i);

        // check if we are part of the DCA group
        if (config->GetBool(dcaGroupId, chanIndex))
        {
            // current channel is part of this DCA group -> apply DCA value to volume
            float dcaValue = config->GetFloat(CHANNEL_VOLUME, (uint)X32_VCHANNEL_BLOCK::DCA + i);
            sendVol_pu *= pow(10.0f, dcaValue/20.0f);
        }
    }

    uint matrixChannelIndex = 64;
    values[0] = sendVol_pu; // volume of this specific channel
    spi->QueueDspData(0, 'v', chanIndex - matrixChannelIndex, 2, 1, &values[0]);
}

void DSP1::SendMatrixSolo(uint chanIndex)
{
    uint32_t values[2];

    values[0] = config->GetBool(CHANNEL_SOLO, chanIndex);
    values[1] = config->GetBool(SOLO_ACTIVE);

    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendMatrixSolo() channelindex %d: %u, %u", chanIndex, values[0], values[1]);

    uint matrixChannelIndex = 64;
    spi->QueueDspData(0, 'v', chanIndex - matrixChannelIndex, 12, 2, (float*)&values[0]);
}

void DSP1::SendMonitorVolume() {
    // send volume to DSP via spi
    float values[1];
    
    values[0] = pow(10.0f, config->GetFloat(MONITOR_VOLUME)/20.0f); // volume of this specific channel

    spi->QueueDspData(0, 'v', 0, 4, 1, &values[0]);
}

void DSP1::SendMainVolume(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendMainVolume()");

    uint mainChannelIndex = 80;
    uint subChannelIndex = 71;
    float volumeLeft_pu = (helper->Saturate(100.0f - config->GetFloat(CHANNEL_PANORAMA, mainChannelIndex), 0.0f, 100.0f) / 100.0f) * pow(10.0f, config->GetFloat(CHANNEL_VOLUME, mainChannelIndex)/20.0f);
    float volumeRight_pu = (helper->Saturate(config->GetFloat(CHANNEL_PANORAMA, mainChannelIndex) + 100.0f, 0.0f, 100.0f) / 100.0f) * pow(10.0f, config->GetFloat(CHANNEL_VOLUME, mainChannelIndex)/20.0f);
    float volumeSub_pu = pow(10.0f, config->GetFloat(CHANNEL_VOLUME_SUB, subChannelIndex)/20.0f);

    if (config->GetBool(CHANNEL_MUTE, mainChannelIndex))
    {
        volumeLeft_pu = 0; // p.u.
        volumeRight_pu = 0; // p.u.
    }
    if (config->GetBool(CHANNEL_MUTE, subChannelIndex))
    {
        volumeSub_pu = 0; // p.u.
    }

    // apply DCAs if enabled
    // loop through all DCA groups
    for (uint i = 0; i < DCA_GROUPS; i++)
    {
        MP_ID dcaGroupId = config->MpCalcId(DCA_GROUP_1, i);

        // check if we are part of the DCA group
        if (config->GetBool(dcaGroupId, chanIndex))
        {
            // current channel is part of this DCA group -> apply DCA value to volume
            float dcaValue = config->GetFloat(CHANNEL_VOLUME, (uint)X32_VCHANNEL_BLOCK::DCA + i);
            float dcaValue_pu = pow(10.0f, dcaValue/20.0f);
            volumeLeft_pu *= dcaValue_pu;
            volumeRight_pu *= dcaValue_pu;
            volumeSub_pu *= dcaValue_pu;
        }
    }

    // send volume to DSP via spi
    float values[3];
    values[0] = volumeLeft_pu;
    values[1] = volumeRight_pu;
    values[2] = volumeSub_pu;

    spi->QueueDspData(0, 'v', 0, 3, 3, &values[0]);
}

void DSP1::SendMainSolo()
{
    uint32_t values[3];

    uint mainChannelIndex = 80;
    uint subChannelIndex = 71;
    values[0] = config->GetBool(CHANNEL_SOLO, mainChannelIndex);
    values[1] = config->GetBool(SOLO_ACTIVE);
    values[2] = config->GetBool(CHANNEL_SOLO, subChannelIndex);
    

    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendMainSolo() channelindex %d: %u, %u, %u", 0, values[0], values[1], values[2]);

    spi->QueueDspData(0, 'v', 0, 13, 3, (float*)&values[0]);
}

void DSP1::SendGate(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendGate() channelindex %d", chanIndex);

    using enum MP_ID;

    float samplerate = (float)config->GetUint(SAMPLERATE);
    float bufferrate = samplerate/(float)DSP_SAMPLES_IN_BUFFER;
    float values[5];

    // threshold
    values[0] = (pow(2.0f, 31.0f) - 1.0f) * pow(10.0f, config->GetFloat(CHANNEL_GATE_TRESHOLD, chanIndex)/20.0f); // send threshold as linear value for 32-bit fixed point representation

    // gainmin
    // range of 60dB means that we will reduce the signal on active gate by 60dB. We have to convert logarithmic dB-value into linear value for gain
    values[1] = 1.0f / pow(10.0f, config->GetFloat(CHANNEL_GATE_RANGE, chanIndex)/20.0f);

    // coeff_attack (envelope is recalculated every sample)
    // to get a smooth behaviour, we will use a low-pass with a damping to get 10%/90% changes within the desired time
    // ln(10%) - ln(90%) = -2.197224577
    // we are using -2.19722 with an additional factor of 1000.0f converts ms -> seconds)
    values[2] = 1.0f - exp(-2197.22457734f/(samplerate * config->GetFloat(CHANNEL_GATE_ATTACK, chanIndex)));

    // hold_ticks (recalculated every 16 samples, hence 333 microseconds)
    values[3] = config->GetFloat(CHANNEL_GATE_HOLD, chanIndex) * bufferrate / 1000.0f;

    // coeff_release (envelope is recalculated every sample)
    // we are using -2.19722 with an additional factor of 1000.0f converts ms -> seconds)
    values[4] = 1.0f - exp(-2197.22457734f/(samplerate * config->GetFloat(CHANNEL_GATE_RELEASE, chanIndex)));

    spi->QueueDspData(0, 'g', chanIndex, 0, 5, &values[0]);
}

void DSP1::SendLowcut(uint8_t chan)
{
    float values[1];

    // Source: https://www.dsprelated.com/showarticle/1769.php
    // alpha = 1 / (1 + 2 * pi * f_c * 1/f_s)
    // Equation for samples: output = alpha * (input + previous_output - previous_input)
    values[0] = 1.0f / (1.0f + 2.0f * M_PI * config->GetFloat(CHANNEL_LOWCUT_FREQ, chan) * (1.0f/(float)config->GetUint(SAMPLERATE)));

    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendLowcut() channelindex %d -> %f", chan, values[0]);


    spi->QueueDspData(0, 'e', chan, 'l', 1, &values[0]);
}

void DSP1::SendEQ(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendEQ() channelindex %d", chanIndex);

    // biquad_trans() needs the coeffs in the following order
    // a0 a0 a1 a1 a2 a2 b1 b1 b2 b2 (section 0/1)
    // a0 a0 a1 a1 a2 a2 b1 b1 b2 b2 (section 2/3)
    // a0 a1 a2 b1 b2 (section 4)

    float values[MAX_CHAN_EQS * 5];

    for (uint peqIndex = 0; peqIndex < MAX_CHAN_EQS; peqIndex++)
    {
        fxmath->RecalcFilterCoefficients_PEQ(&(rChannel[chanIndex].peq[peqIndex]));

/*
        // send coeffiecients without interleaving for biquad() function
        int sectionIndex = peq * 5;
        values[sectionIndex + 0] = -Channel[chanIndex].peq[peqIndex].b[2]; // -b2 (poles)
        values[sectionIndex + 1] = -Channel[chanIndex].peq[peqIndex].b[1]; // -b1 (poles)
        values[sectionIndex + 2] = Channel[chanIndex].peq[peqIndex].a[2]; // a2 (zeros)
        values[sectionIndex + 3] = Channel[chanIndex].peq[peqIndex].a[1]; // a1 (zeros)
        values[sectionIndex + 4] = Channel[chanIndex].peq[peqIndex].a[0]; // a0 (zeros)
*/

        // interleave coefficients for biquad_trans()
        if (((MAX_CHAN_EQS % 2) == 0) || (peqIndex < (MAX_CHAN_EQS - 1))) {
            // we have even number of PEQ-sections
            // or we have odd number but we are still below the last section
            // store data with interleaving
            int sectionIndex = ((peqIndex / 2) * 2) * 5;
            if ((peqIndex % 2) != 0) {
                // odd section index
                sectionIndex += 1;
            }
            values[sectionIndex + 0] = rChannel[chanIndex].peq[peqIndex].a[0]; // a0 (zeros)
            values[sectionIndex + 2] = rChannel[chanIndex].peq[peqIndex].a[1]; // a1 (zeros)
            values[sectionIndex + 4] = rChannel[chanIndex].peq[peqIndex].a[2]; // a2 (zeros)
            values[sectionIndex + 6] = -rChannel[chanIndex].peq[peqIndex].b[1]; // -b1 (poles)
            values[sectionIndex + 8] = -rChannel[chanIndex].peq[peqIndex].b[2]; // -b2 (poles)
        }else{
            // last section: store without interleaving
            int sectionIndex = (MAX_CHAN_EQS - 1) * 5;
            values[sectionIndex + 0] = rChannel[chanIndex].peq[peqIndex].a[0]; // a0 (zeros)
            values[sectionIndex + 1] = rChannel[chanIndex].peq[peqIndex].a[1]; // a1 (zeros)
            values[sectionIndex + 2] = rChannel[chanIndex].peq[peqIndex].a[2]; // a2 (zeros)
            values[sectionIndex + 3] = -rChannel[chanIndex].peq[peqIndex].b[1]; // -b1 (poles)
            values[sectionIndex + 4] = -rChannel[chanIndex].peq[peqIndex].b[2]; // -b2 (poles)
        }
    }

    spi->QueueDspData(0, 'e', chanIndex, 'e', MAX_CHAN_EQS * 5, &values[0]);
}

void DSP1::ResetEq(uint8_t chan)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "ResetEq() channelindex %d", chan);

    float values[1];
    values[0] = 0;
    spi->QueueDspData(0, 'e', chan, 'r', 1, &values[0]);
}

void DSP1::SendCompressor(uint8_t chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "SendCompressor() channelindex %d", chanIndex);

    using enum MP_ID;

    float values[6];

    float samplerate = (float)config->GetUint(SAMPLERATE);
    float bufferrate = samplerate/(float)DSP_SAMPLES_IN_BUFFER;

    // threshold
    //values[0] = (pow(2.0f, 31.0f) - 1.0f) * pow(10.0f, config->GetFloat(CHANNEL_DYNAMICS_TRESHOLD, chanIndex)/20.0f); // send threshold as linear value for 32-bit fixed point representation
    values[0] = config->GetFloat(CHANNEL_DYNAMICS_TRESHOLD, chanIndex); // send threshold as dB value, DSP will convert it to linear value for 32-bit fixed point representation

    // pre-calculated ratio
    values[1] = (1.0f - 1.0f / config->GetFloat(CHANNEL_DYNAMICS_RATIO, chanIndex));

    // makeup
    values[2] = pow(10.0f, config->GetFloat(CHANNEL_DYNAMICS_MAKEUP, chanIndex)/20.0f);

    // to get a smooth behaviour, we will use a low-pass with a damping to get 10%/90% changes within the desired time
    // ln(10%) - ln(90%) = -2.197224577
    // attack (envelope is recalculated every sample)
    // we are using -2.19722 with an additional factor of 1000.0f converts ms -> seconds)
    values[3] = 1.0f - exp(-2197.22457734f/(samplerate * config->GetFloat(CHANNEL_DYNAMICS_ATTACK, chanIndex)));
    // hold (hold-timer is calculated every 16 samples, hence every 333 microseconds)
    values[4] = config->GetFloat(CHANNEL_DYNAMICS_HOLD, chanIndex) * bufferrate / 1000.0f;
    // release (envelope is recalculated every sample)
    // we are using -2.19722 with an additional factor of 1000.0f converts ms -> seconds)
    values[5] = 1.0f - exp(-2197.22457734f/(samplerate * config->GetFloat(CHANNEL_DYNAMICS_RELEASE, chanIndex)));

    spi->QueueDspData(0, 'c', chanIndex, 0, 6, &values[0]);
}

void DSP1::SetInputDelay(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "Hardware: DSP Input Delay for channelindex %d", chanIndex);

    uint32_t values[1];
    values[0] = (config->GetFloat(DELAY_DSP_INPUT, chanIndex) * (float)config->GetUint(SAMPLERATE) * 0.001f);
    spi->QueueDspData(0, 'd', chanIndex, 'i', 1, (float*)&values[0]);
}

void DSP1::SetOutputDelay(uint chanIndex) {
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "Hardware: DSP Output Delay for channelindex %d", chanIndex);

    uint32_t values[1];
    values[0] = (config->GetFloat(DELAY_DSP_OUTPUT, chanIndex) * (float)config->GetUint(SAMPLERATE) * 0.001f);
    spi->QueueDspData(0, 'd', chanIndex, 'o', 1, (float*)&values[0]);
}

void DSP1::SetInputRouting(uint chanIndex)
{
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "Hardware: DSP Input Routing for channelindex %d", chanIndex);

    uint32_t values[2];
    values[0] = config->GetUint(ROUTING_DSP_INPUT, chanIndex);
    values[1] = config->GetUint(ROUTING_DSP_INPUT_TAPPOINT, chanIndex);
    spi->QueueDspData(0, 'r', chanIndex, 0, 2, (float*)&values[0]);
}

void DSP1::SetOutputRouting(uint chanIndex) {
    helper->DEBUG_DSP1(DEBUGLEVEL_NORMAL, "Hardware: DSP Output Routing for channelindex %d", chanIndex);

    uint32_t values[2];
    values[0] = config->GetUint(ROUTING_DSP_OUTPUT, chanIndex);
    values[1] = config->GetUint(ROUTING_DSP_OUTPUT_TAPPOINT, chanIndex);
    spi->QueueDspData(0, 'r', chanIndex, 1, 2, (float*)&values[0]);
}

uint8_t DSP1::GetPeak(int i, uint8_t steps)
{
    if (steps==6) {
        if (rChannel[i].meter >= VUTRESH_00_DBFS_CLIP) { return 6; } // CLIP
        else if (rChannel[i].meter >= VUTRESH_MINUS_06_DBFS) { return 5; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_12_DBFS) { return 4; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_18_DBFS) { return 3; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_30_DBFS) { return 2; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_60_DBFS) { return 1; }
    }

    if (steps==8) {
        if (rChannel[i].meter >= VUTRESH_00_DBFS_CLIP) { return 8; } // CLIP
        else if (rChannel[i].meter >= VUTRESH_MINUS_03_DBFS) { return 7; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_06_DBFS) { return 6; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_09_DBFS) { return 5; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_12_DBFS) { return 4; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_18_DBFS) { return 3; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_30_DBFS) { return 2; }
        else if (rChannel[i].meter >= VUTRESH_MINUS_60_DBFS) { return 1; }
    }

    return 0;
}

//#########################################################################################################################
//
// ########   ######  ########     ###        ######     ###    ##       ##       ########     ###     ######  ##     ## 
// ##     ## ##    ## ##     ##  ## ##       ##    ##   ## ##   ##       ##       ##     ##   ## ##   ##    ## ##    ##  
// ##     ## ##       ##     ##     ##       ##        ##   ##  ##       ##       ##     ##  ##   ##  ##       ##   ##   
// ##     ##  ######  ########      ##       ##       ##     ## ##       ##       ########  ##     ## ##       #####     
// ##     ##       ## ##            ##       ##       ######### ##       ##       ##     ## ######### ##       ##   ##   
// ##     ## ##    ## ##            ##       ##    ## ##     ## ##       ##       ##     ## ##     ## ##    ## ##    ##  
// ########   ######  ##           ####       ######  ##     ## ######## ######## ########  ##     ##  ######  ##     ## 
//
//#########################################################################################################################

void DSP1::CallbackStateMachine() {
    if (state->dsp_disable_readout) {
        return;
    }

    float value;

    // each step is called with 10ms delay
    // so we make sure that we have enough time between the individual SPI-transmissions
    // and the DSP has enough time to switch between SPI Core Read and SPI DMA Chain Transmission
    switch (readState) {
        case 0:
            // request new data from DSP1
            value = 0;
            spi->QueueDspData(0, '?', 'u', 0, 1, &value);
            break;
        case 1:
            // read data from DSP1 via DMA
            spi->ReadDspData(0, '?', 0, 0); // read the answer from DSP

            // request new data from DSP2
            value = 0;
            spi->QueueDspData(1, '?', 'u', 0, 1, &value);
            break;
        case 2:
            // request new data from DSP2
            spi->ReadDspData(1, '?', 0, 0); // read the answer from DSP
            break;
        case 3:
            // process received SPI-Data
            while (spi->HasNextEvent()) {
                SpiEvent* spiEvent = spi->GetNextEvent();

                if (spiEvent->dsp == 0) {
                    callbackDsp1(spiEvent->classId, spiEvent->channel, spiEvent->index, spiEvent->valueCount, spiEvent->values);
                } else {
                    callbackDsp2(spiEvent->classId, spiEvent->channel, spiEvent->index, spiEvent->valueCount, spiEvent->values);
                }
            }
            UpdateVuMeter(50);
            break;
        case 4:
            // wait-state
            break;
        default:
            break;
    }

    // write to DSP1 every 10ms, but only when not within a DMA-Read-Cycle
    if (readState != 1) {
        spi->ProcessDspTxQueue(0);
    }
    // write to DSP2 every 10ms, but only when not within a DMA-Read-Cycle
    if (readState != 2) {
        spi->ProcessDspTxQueue(1);
    }

    // increment readState up to 4 and reset to 0 to get 50ms cycles
    readState++;
    if (readState >= 5) {
        readState = 0;
    }
}

#define gotoxy(x,y) printf("\033[%d;%dH", (y), (x))
bool clearer = true;

void DSP1::callbackDsp1(uint8_t classId, uint8_t channel, uint8_t index, uint8_t valueCount, void* values)
{
    if (helper->DEBUG_DSP1(DEBUGLEVEL_VERBOSE))
    {
        if(clearer)
        {
            printf("\033[H\033[J");
            clearer = false;
        }

        gotoxy(0,0);
        printf("\n\n--------------DSP1_CALLBACK----------------\n");
    }

    float* floatValues = (float*)values;
    uint32_t* intValues = (uint32_t*)values;

    switch (classId)
    {
        case 's': // status-feedback
            switch (channel)
            {
                case 'u': // Update pack
                    if (valueCount == (SPI_DMA_COMMDATA_SIZE))
                    {
                        // idx  0     = dspVersion
                        // idx  1     = CPU-cycles
                        // idx  2     = Audio Glitch Counter
                        // idx  3..42 = volume 40 DSP-channels
                        // idx 43..50 = volume 8 DSP2 FX-return channels
                        // idx 51..66 = volume 16 MixBusses
                        // idx 67..69 = volume 3 main-bus (L, R, C)

                        // future options:
                            // gain of 32 compressors
                            // gain of 32 gates

                        state->dspVersion[0] = floatValues[0];

                        // we are receiving 16 samples with 20.83us each resulting in 16*20.83us = 333us per interrupt
                        // DSP-Load calculation: number of used CPU-cycles for processing divided by the core-clock-frequency based on the 333us timebase
                        //state->dspLoad[0] = (((float)intValues[1]/264.0f) / (16.0f/0.048f)) * 100.0f;
                        //state->dspAudioGlitchCounter[0] = floatValues[1]; // audio-glitch-counter

                        if (helper->DEBUG_DSP1(DEBUGLEVEL_VERBOSE))
                        {
                            printf("Glitch ISR: %f\n", floatValues[1]);
                            printf("Glitch Main: %f\n", floatValues[2]);
                        }
                       
                        uint c  = 3;

                        state->dspLoad[0] = intValues[c + 0];
                        state->dspAudioGlitchCounter[0] = intValues[c + 26];

                        if (helper->DEBUG_DSP1(DEBUGLEVEL_VERBOSE))
                        {
                            printf("audioProcessData()    %10d\n", intValues[c + 0]);
                            printf("  prepare             %10d\n", intValues[c + 21]);
                            printf("  audioSmoothVolume   %10d\n", intValues[c + 22]);
                            printf("  Data Input          %10d\n", intValues[c + 1]);
                            printf("  InputDelay/Routing  %10d\n", intValues[c + 2]);
                            printf("  Lowcut              %10d\n", intValues[c + 3]);
                            printf("  Noisegate           %10d\n", intValues[c + 4]);
                            printf("  EQ                  %10d\n", intValues[c + 5]);
                            printf("  Dynamics            %10d\n", intValues[c + 6]);
                            printf("  Copy DSP2-Return    %10d\n", intValues[c + 27]);
                            printf("  Channel/Fader       %10d\n", intValues[c + 7]);
                            printf("  Mixbus              %10d\n", intValues[c + 8]);
                            printf("    set zero          %10d\n", intValues[c + 17]);
                            printf("    acc channels      %10d\n", intValues[c + 18]);
                            printf("    write back        %10d\n", intValues[c + 19]);
                            printf("    volume            %10d\n", intValues[c + 20]);
                            printf("  Main-Out            %10d\n", intValues[c + 9]);
                            printf("  EQMain              %10d\n", intValues[c + 10]);
                            printf("  Main Volume         %10d\n", intValues[c + 11]);
                            printf("  Matrix              %10d\n", intValues[c + 12]);
                            printf("  Monitor             %10d\n", intValues[c + 13]);
                            printf("  Routing/OutputDelay %10d\n", intValues[c + 14]);
                            printf("  Copy VU-Data        %10d\n", intValues[c + 15]);
                            printf("NonAudioprocess       %10d\n", intValues[c + 16]);
                            printf("  copy cylcedata      %10d\n", intValues[c + 23]);
                            printf("  process SPI         %10d\n", intValues[c + 24]);
                            printf("\n");
                            printf("SPI Buffer Overflow   %10d\n", intValues[c + 25]);
                            printf("Glitch Audio ISR      %10d\n", intValues[c + 26]);
                        }
                        
                        c  = 3 + CYCLEMAP_SIZE;

                        //copy meter-info to rChannel-struct
                        for (int i = 0; i < (MAX_FPGA_TO_DSP1_CHANNELS + 8 + 16 + 8); i++)
                        {
                            rChannel[i].meter = abs(floatValues[c + i]); // convert 32-bit audio-value
                        }

                        MainChannelLR.meter[0] = abs(floatValues[c + DSP_BUF_IDX_MAINLEFT -1]); // convert 32-bit audio-value
                        MainChannelLR.meter[1] = abs(floatValues[c + DSP_BUF_IDX_MAINRIGHT -1]); // convert 32-bit audio-value
                        MainChannelSub.meter[0] = abs(floatValues[c + DSP_BUF_IDX_MAINSUB -1]); // convert 32-bit audio-value
                    }
                    else if (helper->DEBUG_DSP1(DEBUGLEVEL_VERBOSE))
                    {
                        printf("DSP1 \"u\" -> valuecount is wrong = %d\n", valueCount);
                    }
                    break;
            }
            break;
        default:
            break;
    }
}


void DSP1::UpdateVuMeter(uint8_t intervalMs)
{
    uint8_t coefficientDecay = 250 / intervalMs; // 50ms * 5 = 250ms

	// Calculate decayed value

	// MainLeft
	if (MainChannelLR.meter[0] > MainChannelLR.meterDecay[0])
    {
		// current value is above stored decay-value -> copy value immediatly
		MainChannelLR.meterDecay[0] = MainChannelLR.meter[0];
	}
    else
    {
		// current value is below -> afterglow
		MainChannelLR.meterDecay[0] -= (MainChannelLR.meterDecay[0] / coefficientDecay);
	}
    config->Set(MAIN_L_METER_DECAYED_POST_GAIN, helper->get_dbfs_from_peak_arm_opt(MainChannelLR.meterDecay[0]));

	// MainRight
	if (MainChannelLR.meter[1] > MainChannelLR.meterDecay[1])
    {
		// current value is above stored decay-value -> copy value immediatly
		MainChannelLR.meterDecay[1] = MainChannelLR.meter[1];
	}
    else
    {
		// current value is below -> afterglow
		MainChannelLR.meterDecay[1] -= (MainChannelLR.meterDecay[1] / coefficientDecay);
	}
    config->Set(MAIN_R_METER_DECAYED_POST_GAIN, helper->get_dbfs_from_peak_arm_opt(MainChannelLR.meterDecay[1]));

	// MainSub
	if (MainChannelSub.meter[0] > MainChannelSub.meterDecay[0])
    {
		// current value is above stored decay-value -> copy value immediatly
		MainChannelSub.meterDecay[0] = MainChannelSub.meter[0];
	}
    else
    {
		// current value is below -> afterglow
		MainChannelSub.meterDecay[0] -= (MainChannelSub.meterDecay[0] / coefficientDecay);
	}
    config->Set(SUB_METER_DECAYED_POST_GAIN, helper->get_dbfs_from_peak_arm_opt(MainChannelSub.meterDecay[0]));

	// Now calculate the VU Meter LEDs for each channel
	for (int i = 0; i < (40 + 8 + 16); i++)
    {
		// Calculate decayed value
        if (rChannel[i].meter > rChannel[i].meterDecay)
        {
            // current value is above stored decay-value -> copy value immediatly
            rChannel[i].meterDecay = rChannel[i].meter;
        }
        else
        {
            // current value is below -> afterglow
            // this function is called every 10ms. A Decay-Rate of 6dB/second would be ideal, but we do a rought estimation here
            rChannel[i].meterDecay -= (rChannel[i].meterDecay / coefficientDecay);
        }
        config->Set(CHANNEL_METER_DECAYED_POST_GAIN, helper->get_dbfs_from_peak_arm_opt(rChannel[i].meterDecay), i);
    }

    

    // // only the first 32 full-featured channels have dynamic-information for compressor and gate
    // for (int i = 0; i < 32; i++)
    // {
    //     if(config->IsModelX32Core() || config->IsModelX32Rack())
    //     {
	// 	    // the dynamic-information is received with the 'd' information, but we will store them here
	// 	    //if (!!RECEIVED_CHANNEL_GAIN!! < 1.0f) { rChannel[i].meter6Info |= 0b01000000; }
	// 	    //if (Channel[i].compressor.gain < 1.0f) { rChannel[i].meter6Info |= 0b10000000; }
    //     }
	// }
}



//#################################################
//#
//#   ########   ######  ########    #####   
//#   ##     ## ##    ## ##     ## ##     ## 
//#   ##     ## ##       ##     ##        ## 
//#   ##     ##  ######  ########       ##   
//#   ##     ##       ## ##          ##      
//#   ##     ## ##    ## ##        ##        
//#   ########   ######  ##        ######### 
//#
//#################################################


void DSP1::DSP2_SetFx(int fxSlot, FX_TYPE fxType, int mode)
{
    helper->DEBUG_DSP2(DEBUGLEVEL_NORMAL, "DSP2_SetFx() fxslot %d type %d mode %d", fxSlot, int(fxType), mode);

    fx_slot[fxSlot]->LoadFx(fxType);

    int values[2];
    values[0] = (int)fxType; // type of the Effect
    values[1] = (int)mode; // mode of the effect (e.g. DualMono or Stereo)
    spi->QueueDspData(1, 'f', 'r', fxSlot, 2, (float*)values);

    DSP2_SendFxParameter(fxSlot);
}

void DSP1::DSP2_SendFxParameter(int slotIdx)
{
    if (slotIdx > MAX_FX_SLOTS-1 || !fx_slot[slotIdx]->HasFx()) {
        return;
    }

    helper->DEBUG_DSP2(DEBUGLEVEL_NORMAL, "DSP2_SendFxParameter() fxslot %d", slotIdx);

    float values[45]; // MultibandCompressor takes a maximum of 41 float-parameters
    int valueCount;

    float depth[2];
    float delayMs[2];
    float phase[2];
    float freq[4];

    FxBase* fx = fx_slot[slotIdx]->fx;

    switch(fx_slot[slotIdx]->fxType)
    {
        case FX_TYPE::REVERB:
            fxmath->fxCalcParameters_Reverb(&values[0], 
                config->GetFloat(fx->GetParameterDefinition(0), slotIdx), // roomSizeMs
                config->GetFloat(fx->GetParameterDefinition(1), slotIdx), // rt60
                config->GetFloat(fx->GetParameterDefinition(2), slotIdx), // lpfFreq
                config->GetFloat(fx->GetParameterDefinition(3), slotIdx), // dry
                config->GetFloat(fx->GetParameterDefinition(4), slotIdx)  // wet
            );
            valueCount = 6;
            spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);
            break;
        case FX_TYPE::CHORUS:
            depth[0] = config->GetFloat(fx->GetParameterDefinition(0), slotIdx);
            delayMs[0] = config->GetFloat(fx->GetParameterDefinition(1), slotIdx);
            phase[0] = config->GetFloat(fx->GetParameterDefinition(2), slotIdx);
            freq[0] = config->GetFloat(fx->GetParameterDefinition(3), slotIdx);
            depth[1] = config->GetFloat(fx->GetParameterDefinition(4), slotIdx);
            delayMs[1] = config->GetFloat(fx->GetParameterDefinition(5), slotIdx);
            phase[1] = config->GetFloat(fx->GetParameterDefinition(6), slotIdx);
            freq[1] = config->GetFloat(fx->GetParameterDefinition(7), slotIdx);
            
            fxmath->fxCalcParameters_Chorus(&values[0], depth, delayMs, phase, freq, config->GetFloat(fx->GetParameterDefinition(8), slotIdx));
            valueCount = 9;
            spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);
            break;
        case FX_TYPE::TRANSIENTSHAPER:
            fxmath->fxCalcParameters_TransientShaper(&values[0],
                config->GetFloat(fx->GetParameterDefinition(0), slotIdx), // tFastMs
                config->GetFloat(fx->GetParameterDefinition(1), slotIdx), // tMediumMs
                config->GetFloat(fx->GetParameterDefinition(2), slotIdx), // tSlowMs
                config->GetFloat(fx->GetParameterDefinition(3), slotIdx), // attack
                config->GetFloat(fx->GetParameterDefinition(4), slotIdx), // sustain
                config->GetFloat(fx->GetParameterDefinition(5), slotIdx)  // delayMs
                );
            valueCount = 6;
            spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);
            break;
        case FX_TYPE::OVERDRIVE:
            fxmath->fxCalcParameters_Overdrive(&values[0],
                config->GetFloat(fx->GetParameterDefinition(0), slotIdx), // preGain
                config->GetFloat(fx->GetParameterDefinition(1), slotIdx), // Q
                config->GetFloat(fx->GetParameterDefinition(2), slotIdx), // Bias
                config->GetFloat(fx->GetParameterDefinition(3), slotIdx), // hpfInputFreq
                config->GetFloat(fx->GetParameterDefinition(4), slotIdx), // lpfInputFreq
                config->GetFloat(fx->GetParameterDefinition(5), slotIdx)  // lpfOutputFreq
            );
            valueCount = 6;
            spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);
            break;
        case FX_TYPE::DELAY:
            delayMs[0] = config->GetFloat(fx->GetParameterDefinition(0), slotIdx);
            delayMs[1] = config->GetFloat(fx->GetParameterDefinition(1), slotIdx);
            fxmath->fxCalcParameters_Delay(&values[0], delayMs);
            valueCount = 2;
            spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);
            break;
        case FX_TYPE::MULTIBANDCOMPRESOR: //                       channel  band   threshold  ratio   attack  hold   release   makeup
            // first send parameters for all channels and all bands
            for (int c = 0; c < 2; c++) {
                // now prepare the frequencies for both channels
                freq[0] = config->GetFloat(fx->GetParameterDefinition(34 * c + 0), slotIdx);
                freq[1] = config->GetFloat(fx->GetParameterDefinition(34 * c + 1), slotIdx);
                freq[2] = config->GetFloat(fx->GetParameterDefinition(34 * c + 2), slotIdx);
                freq[3] = config->GetFloat(fx->GetParameterDefinition(34 * c + 3), slotIdx);
                fxmath->fxCalcParameters_MultibandCompressorFreq(&values[0], c, freq);
                valueCount = 41;
                spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);

                for (int band = 0; band < 5; band++) {
                    fxmath->fxCalcParameters_MultibandCompressor(&values[0], c, band, 
                        config->GetFloat(fx->GetParameterDefinition(34 * c + 4 + band * 6), slotIdx), // threshold
                        config->GetFloat(fx->GetParameterDefinition(34 * c + 5 + band * 6), slotIdx), // ratio
                        config->GetFloat(fx->GetParameterDefinition(34 * c + 6 + band * 6), slotIdx), // attack
                        config->GetFloat(fx->GetParameterDefinition(34 * c + 7 + band * 6), slotIdx), // hold
                        config->GetFloat(fx->GetParameterDefinition(34 * c + 8 + band * 6), slotIdx), // release
                        config->GetFloat(fx->GetParameterDefinition(34 * c + 9 + band * 6), slotIdx)  // makeup
                    );
                    valueCount = 8;
                    spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);
                }
            }

            break;
        case FX_TYPE::DYNAMICEQ: //                       band type  freq   staticGain  maxDynGain  Q  thresh  ratio  attack  release
            for (int band = 0; band < 3; band++) {
                fxmath->fxCalcParameters_DynamicEQ(&values[0], band,
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 0), slotIdx), // type
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 1), slotIdx), // freq
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 2), slotIdx), // staticGain
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 3), slotIdx), // maxDynGain
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 4), slotIdx), // Q
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 5), slotIdx), // threshold
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 6), slotIdx), // ratio
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 7), slotIdx), // attack
                    config->GetFloat(fx->GetParameterDefinition(9 * band + 8), slotIdx)  // release
                );
                valueCount = 11;
                spi->QueueDspData(1, 'f', 'c', slotIdx, valueCount, values);
            }
            break;
        case FX_TYPE::DEFEEDBACK:
            // nothing to send at the moment
            break;
        default:
            break;
    }
}

void DSP1::DSP2_SetOscillator(uint8_t oscIndex, float frequency, float volumedB) {
    dsp2Oscillator[oscIndex].frequency = frequency;
    dsp2Oscillator[oscIndex].volume = pow(10.0f, volumedB / 20.0f);
    DSP2_SendOscillatorValues();
}

void DSP1::DSP2_SendOscillatorValues() {
    float values[4];
    values[0] = dsp2Oscillator[0].frequency;
    values[1] = dsp2Oscillator[1].frequency;
    values[2] = dsp2Oscillator[0].volume;
    values[3] = dsp2Oscillator[1].volume;
    spi->QueueDspData(1, 'a', 'o', 0, 4, values);
}

void DSP1::callbackDsp2(uint8_t classId, uint8_t channel, uint8_t index, uint8_t valueCount, void* values) {
    float* floatValues = (float*)values;
    uint32_t* intValues = (uint32_t*)values;

    //helper->DEBUG_DSP2(DEBUGLEVEL_TRACE, "Callback - classid=%c channel=%c, index=%d, valueCount=%d", classId, channel, index, valueCount);

    switch (classId) {
        case 's': // status-feedback
            switch (channel) {
                case 'u': // Update pack
                    if (valueCount == (4 + 64)) {
                        state->dspVersion[1] = floatValues[0];
                        state->dspLoad[1] = (((float)intValues[1]/264.0f) / (16.0f/0.048f)) * 100.0f;
                        state->dspFreeHeapWords[1] = floatValues[2]; // free heap-space in 32-bit words
                        state->dspAudioGlitchCounter[1] = floatValues[3]; // audio-glitch-counter

                        // direct copy of RTA-data
                        memcpy(&rtaData[0], &floatValues[4], 64 * sizeof(float));
/*
                        // optimize the data and convert linear frequency axis to logarithmic axis
                        float attack = 1.0f;  // fast rise
                        float release = 0.75f; // slower decay
                        uint8_t RTA_BINS = 128;
                        uint8_t DISPLAY_BANDS = 64;
                        float* rta_rspectrum = &floatValues[3];

                        // k defines the bowing of the logarithmic curve
                        // a value of around 0.05 to 0.1 should be fine for 64 display-bands
                        const float k = 0.07f; 
                        const float denom = expf(k * (float)(DISPLAY_BANDS - 1)) - 1.0f;

                        for (int i = 0; i < DISPLAY_BANDS; i++) {
                            // Step 1: logarithmix map-equation
                            // calculate the Bar-index i to the FFT-Bin-Index
                            float logIndex = (float)(RTA_BINS - 1) * (expf(k * (float)i) - 1.0f) / denom;
                            
                            // security check for all arrays
                            int idxLo = (int)logIndex;
                            if (idxLo < 0) idxLo = 0;
                            int idxHi = idxLo + 1;
                            
                            // limit to the array-borders
                            if (idxHi >= RTA_BINS) {
                                idxHi = RTA_BINS - 1;
                                idxLo = idxHi - 1;
                                if(idxLo < 0) idxLo = 0;
                            }
                            
                            float frac = logIndex - (float)idxLo;
                            
                            // linear interpolation between two Bins
                            float targetVal = rta_rspectrum[idxLo] + frac * (rta_rspectrum[idxHi] - rta_rspectrum[idxLo]);

                            // Step 2: temporal smoothing (Ballistic)
                            if (targetVal > rtaData[i]) {
                                rtaData[i] += attack * (targetVal - rtaData[i]);
                            } else {
                                rtaData[i] += release * (targetVal - rtaData[i]);
                            }
                        }      
*/                  
                    }
                    break;
            }
            break;
        default:
            break;
    }
}

}