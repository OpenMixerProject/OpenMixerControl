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

#include "mixer.h"

namespace OMC
{

Mixer::Mixer(X32BaseParameter* basepar): X32Base(basepar) {
    fpga = new Fpga(basepar);
    dsp = new DSP1(basepar);
    adda = new Adda(basepar);
    card = new Card(basepar, adda);
}

void Mixer::Init() {
    helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "fpga->Init()");
    fpga->Init();

    helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "dsp->Init()");
    dsp->Init();

    helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "adda->Init()");
    adda->Init();

    helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "card->Init()");
    card->Init();
}

void Mixer::Tick100ms()
{
    card->Tick100ms();
}

// #################################################################################################################################
//
// ########  ######## ########    ###    ##     ## ##       ######## 
// ##     ## ##       ##         ## ##   ##     ## ##          ##    
// ##     ## ##       ##        ##   ##  ##     ## ##          ##    
// ##     ## ######   ######   ##     ## ##     ## ##          ##    
// ##     ## ##       ##       ######### ##     ## ##          ##    
// ##     ## ##       ##       ##     ## ##     ## ##          ##    
// ########  ######## ##       ##     ##  #######  ########    ##    
//
//
//  ######  ##     ##    ###    ##    ## ##    ## ######## ##       ##          ###    ##    ##  #######  ##     ## ######## 
// ##    ## ##     ##   ## ##   ###   ## ###   ## ##       ##       ##         ## ##    ##  ##  ##     ## ##     ##    ##    
// ##       ##     ##  ##   ##  ####  ## ####  ## ##       ##       ##        ##   ##    ####   ##     ## ##     ##    ##    
// ##       ######### ##     ## ## ## ## ## ## ## ######   ##       ##       ##     ##    ##    ##     ## ##     ##    ##    
// ##       ##     ## ######### ##  #### ##  #### ##       ##       ##       #########    ##    ##     ## ##     ##    ##    
// ##    ## ##     ## ##     ## ##   ### ##   ### ##       ##       ##       ##     ##    ##    ##     ## ##     ##    ##    
//  ######  ##     ## ##     ## ##    ## ##    ## ######## ######## ######## ##     ##    ##     #######   #######     ##    
//
// #################################################################################################################################




void Mixer::ClearSolo()
{
    if (IsSoloActivated())
    {
        for (int i=0; i<MAX_VCHANNELS; i++)
        {
            config->Set(MP_ID::CHANNEL_SOLO, 0.0f, i);
        }
    }
}

bool Mixer::IsSoloActivated()
{
    for (uint8_t i=0; i < MAX_VCHANNELS; i++)
    {
        if (config->GetBool(MP_ID::CHANNEL_SOLO, i))
        {
            return true;
        }
    }
    return false;
}

String Mixer::GetCardModelString(){
    return adda->GetExpansion();
}

//#########################################
//#
//#  ######  ##    ## ##    ##  ######  
//# ##    ##  ##  ##  ###   ## ##    ## 
//# ##         ####   ####  ## ##       
//#  ######     ##    ## ## ## ##       
//#       ##    ##    ##  #### ##       
//# ##    ##    ##    ##   ### ##    ## 
//#  ######     ##    ##    ##  ######  
//#
//#########################################

void Mixer::Sync()
{
    helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "------------------- mixer->Sync() ----------------");

    if (helper->DEBUG_MIXER(DEBUGLEVEL_NORMAL))
    {
        uint changedParameterCount = 0;
        for (auto const& [parameter_id, indexSet] : *(config->GetChangedParameterList()))
        {
            changedParameterCount += indexSet.size();
        }
        helper->DEBUG_MIXER(DEBUGLEVEL_NORMAL, "syncing %d changed Mixerparameters to hardware", changedParameterCount);
    }

    vector<MP_ID> filter;

    // Mute Groups - check if mute group has changed
    for (uint muteGroupIdx = 0; muteGroupIdx < MUTE_GROUPS; muteGroupIdx++)
    {
        filter.push_back(config->MpCalcId(MUTE_GROUP_1_MUTE, muteGroupIdx));
    }
    if (config->HasParametersChanged(filter))
    {
        // loop through all channels
        for (uint chanIndex = 0; chanIndex < MAX_VCHANNELS; chanIndex++)
        {
            // loop through all mute groups
            bool channelIsAMuteGroupMember = false;
            bool muteChannel = false;
            for (uint i = 0; i < MUTE_GROUPS; i++)
            {
                MP_ID muteGroupSwitchId = config->MpCalcId(MUTE_GROUP_1_MUTE, i);
                MP_ID muteGroupId = config->MpCalcId(MUTE_GROUP_1, i);
        
                // if we are part of the mute group
                if (config->GetBool(muteGroupId, chanIndex))
                {
                    // add to the desired channel mute state according to mute group switch
                    // -> if at least one group is muted, mute the channel
                    muteChannel |= config->GetBool(muteGroupSwitchId);
                    channelIsAMuteGroupMember = true;
                }
            }

            // only change mute is channel is a member
            if (channelIsAMuteGroupMember)
            {
                // and only change, if the mute group state is different than the channel state
                if (muteChannel != config->GetBool(CHANNEL_MUTE, chanIndex))
                {
                    config->Set(CHANNEL_MUTE, muteChannel, chanIndex);
                }
            }
        }
    }

    // DCA Groups - check if DCA group has changed (channel added or removed to DCA group)
    filter.clear();
    for (uint dcaGroupIdx = 0; dcaGroupIdx < DCA_GROUPS; dcaGroupIdx++)
    {
        filter.push_back(config->MpCalcId(DCA_GROUP_1, dcaGroupIdx));
    }
    if (config->HasParametersChanged(filter))
    {
        // loop through all channels
        for (uint chanIndex = 0; chanIndex < MAX_VCHANNELS; chanIndex++)
        {
            bool dcaBindOnChannel = false;

            // loop through all DCA groups
            for (uint i = 0; i < DCA_GROUPS; i++)
            {
                MP_ID dcaGroupId = config->MpCalcId(DCA_GROUP_1, i);
        
                // check if we are part of the DCA group
                if (config->GetBool(dcaGroupId, chanIndex))
                {
                    dcaBindOnChannel = true;
                }
            }

            if (dcaBindOnChannel)
            {
                // this channel is part of at least one DCA group -> resend channel-volume for this channel to DSP to update DCA group values
                if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::NORMAL) ||
                helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::AUX) ||
                helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::FXRET) ||
                helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::BUS) )
                {
                    dsp->SendChannelVolume(chanIndex);
                }
                else if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::MATRIX))
                {
                    dsp->SendMatrixVolume(chanIndex);
                }
                else if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::MAINSUB))
                {
                    dsp->SendMainVolume(chanIndex);
                }
                else if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::MAIN))
                {
                    dsp->SendMainVolume(chanIndex);
                }
            }
        }
    }

    // Volume
    filter = {CHANNEL_GAIN, CHANNEL_VOLUME, CHANNEL_VOLUME_SUB, CHANNEL_MUTE, CHANNEL_PANORAMA, CHANNEL_SEND_LR, CHANNEL_SEND_SUB};
    if (config->HasParametersChanged(filter))
    {
        vector<uint> changedIndexes = config->GetChangedParameterIndexes(filter);
        for (auto const& changedIndex : changedIndexes)
        {
            if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::NORMAL) ||
                helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::AUX) ||
                helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::FXRET) ||
                helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::BUS) )
            {
                dsp->SendChannelVolume(changedIndex);
            }
            else if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::MATRIX))
            {
                dsp->SendMatrixVolume(changedIndex);
            }
            else if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::DCA))
            {
                // DCA channel has changed -> resend volume for all channels that are part of this DCA group to update DCA group values
                for (uint chanIndex = 0; chanIndex < MAX_VCHANNELS; chanIndex++)
                {
                    MP_ID dcaGroupId = config->MpCalcId(DCA_GROUP_1, changedIndex - (uint)X32_VCHANNEL_BLOCK::DCA); 
                    if (config->GetBool(dcaGroupId, chanIndex))
                    {
                        if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::NORMAL) ||
                            helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::AUX) ||
                            helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::FXRET) ||
                            helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::BUS) )
                        {
                            dsp->SendChannelVolume(chanIndex);
                        }
                        else if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::MATRIX))
                        {
                            dsp->SendMatrixVolume(chanIndex);
                        }
                        else if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::MAINSUB))
                        {
                            dsp->SendMainVolume(chanIndex);
                        }
                        else if (helper->IsInChannelBlock(chanIndex, X32_VCHANNEL_BLOCK::MAIN))
                        {
                            dsp->SendMainVolume(chanIndex);
                        }  
                    }
                }
            }
            else if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::MAINSUB))
            {
                dsp->SendMainVolume(changedIndex);
            }
            else if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::MAIN))
            {
                dsp->SendMainVolume(changedIndex);
            }  
        }
    }

    // Solo
    filter = {CHANNEL_SOLO};
    if (config->HasParametersChanged(filter))
    {
        // first set state of solo (it's needed to activate the solo bus in DSP1)
        config->Set(SOLO_ACTIVE, IsSoloActivated());

        // then set the solo
        vector<uint> changedIndexes = config->GetChangedParameterIndexes(filter);
        for (auto const& changedIndex : changedIndexes)
        {
            if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::NORMAL) ||
                helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::AUX) ||
                helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::FXRET) ||
                helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::BUS) )
            {
                dsp->SendChannelSolo(changedIndex);
            }
            else if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::MATRIX))
            {
                dsp->SendMatrixSolo(changedIndex);
            }
            else if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::MAINSUB)) {
                // TODO
                //dsp->SendMainSolo();
            }
            else if (helper->IsInChannelBlock(changedIndex, X32_VCHANNEL_BLOCK::MAIN)) {
                dsp->SendMainSolo();
            }  
        }
    }

    // Sends
    if (config->HasParametersChanged(MP_CAT::CHANNEL_SENDS))
    { 
        vector<uint> changedIndexes = config->GetChangedParameterIndexes(MP_CAT::CHANNEL_SENDS);
        for (auto const& changedIndex : changedIndexes)
        {
            dsp->SendChannelSend(changedIndex);
            dsp->ChannelSendTapPoints(changedIndex);
        }
    }

    // Routing FPGA
    if (config->HasParameterChanged(ROUTING_FPGA))
    { 
        vector<uint> changedIndexes = config->GetChangedParameterIndexes({ROUTING_FPGA});
        for (auto const& changedIndex : changedIndexes)
        {
            String warning = config->GetFpgaPatchInsertWarning(changedIndex, config->GetUint(ROUTING_FPGA, changedIndex));
            if (warning.length() > 0)
            {
                helper->Error("%s\n", warning.c_str());
            }

            fpga->SendRoutingToFpga(changedIndex);

            // If a DSP In slot (72..111) changed, update headamp gain, volume, and any insert send fed by it
            if (changedIndex >= (FPGA_OUTPUT_IDX_DSP - 1) &&
                changedIndex < ((FPGA_OUTPUT_IDX_DSP - 1) + MAX_FPGA_TO_DSP1_CHANNELS))
            {
                uint dspInSlot = changedIndex - (FPGA_OUTPUT_IDX_DSP - 1) + 1;
                for (uint ch = 0; ch < MAX_FPGA_TO_DSP1_CHANNELS; ch++)
                {
                    if (config->GetUint(ROUTING_DSP_INPUT, ch) == dspInSlot)
                    {
                        halSendGain(ch);
                        dsp->SendChannelVolume(ch);
                        uint sendOut = config->GetUint(CHANNEL_INSERT_SEND, ch);
                        if (config->IsValidInsertSendOutput(sendOut))
                        {
                            fpga->SendRoutingToFpga(sendOut - 1);
                        }
                    }
                }
            }
        }
    }

    // Output Delay
    if (config->HasParametersChanged({DELAY_DSP_OUTPUT}))
    { 
        vector<uint> changedIndexes = config->GetChangedParameterIndexes({DELAY_DSP_OUTPUT});
        for (auto const& changedIndex : changedIndexes)
        {
            dsp->SetOutputDelay(changedIndex);
        }
    }

    // Input Delay
    if (config->HasParametersChanged({DELAY_DSP_INPUT}))
    { 
        vector<uint> changedIndexes = config->GetChangedParameterIndexes({DELAY_DSP_INPUT});
        for (auto const& changedIndex : changedIndexes)
        {
            dsp->SetInputDelay(changedIndex);
        }
    }

    // DSP Routing
    if (config->HasParametersChanged({ROUTING_DSP_OUTPUT, ROUTING_DSP_OUTPUT_TAPPOINT}))
    { 
        vector<uint> changedIndexes = config->GetChangedParameterIndexes({ROUTING_DSP_OUTPUT, ROUTING_DSP_OUTPUT_TAPPOINT});
        for (auto const& changedIndex : changedIndexes)
        {
            dsp->SetOutputRouting(changedIndex);
        }
    }

    set<uint> insertSyncChannels;

    if (config->HasParametersChanged({ROUTING_DSP_INPUT, ROUTING_DSP_INPUT_TAPPOINT}))
    { 
        vector<uint> changedIndexes = config->GetChangedParameterIndexes({ROUTING_DSP_INPUT, ROUTING_DSP_INPUT_TAPPOINT});
        for (auto const& changedIndex : changedIndexes)
        {
            dsp->SetInputRouting(changedIndex);

            if (changedIndex < MAX_FPGA_TO_DSP1_CHANNELS)
            {
                uint8_t oldDspIn = prevDspInput[changedIndex];
                uint8_t newDspIn = (uint8_t)config->GetUint(ROUTING_DSP_INPUT, changedIndex);
                if (oldDspIn != newDspIn)
                {
                    prevDspInput[changedIndex] = newDspIn;
                    if (oldDspIn >= 1 && oldDspIn <= MAX_FPGA_TO_DSP1_CHANNELS)
                    {
                        fpga->SendRoutingToFpga((FPGA_OUTPUT_IDX_DSP - 1) + (oldDspIn - 1));
                    }
                    insertSyncChannels.insert(changedIndex);
                    halSendGain(changedIndex);
                }
            }
        }
    }

    // Channel I/O Inserts (deduplicated per channel per tick)
    if (config->HasParametersChanged({CHANNEL_INSERT_ENABLE, CHANNEL_INSERT_SEND, CHANNEL_INSERT_RETURN}))
    {
        for (auto const& changedIndex : config->GetChangedParameterIndexes({CHANNEL_INSERT_ENABLE, CHANNEL_INSERT_SEND, CHANNEL_INSERT_RETURN}))
        {
            if (changedIndex < MAX_FPGA_TO_DSP1_CHANNELS)
            {
                insertSyncChannels.insert(changedIndex);
            }
        }
    }

    for (auto const& ch : insertSyncChannels)
    {
        syncChannelInsert(ch);
    }

    // Gate
    if (config->HasParametersChanged(MP_CAT::CHANNEL_GATE))
    {        
        vector<uint> changedIndexes = config->GetChangedParameterIndexes(MP_CAT::CHANNEL_GATE);
        for (auto const& changedIndex : changedIndexes)
        {
            dsp->SendGate(changedIndex);
        }
    }

    // Phantom / Gain
    if (config->HasParameterChanged (CHANNEL_PHANTOM) || config->HasParameterChanged(CHANNEL_GAIN))
    {
        vector<uint> changedIndexes = config->GetChangedParameterIndexes({CHANNEL_PHANTOM, CHANNEL_GAIN});
        for (auto const& changedIndex : changedIndexes)
        {
            halSendGain(changedIndex);
        }
    }

    // EQ
    if (config->HasParametersChanged(MP_CAT::CHANNEL_EQ))
    {
        vector<uint> changedIndexes = config->GetChangedParameterIndexes(MP_CAT::CHANNEL_EQ);
        for (auto const& changedIndex : changedIndexes)
        {
            // dsp->Channel holds only 40 Elements
            if (changedIndex > 39)
            {
                continue;
            }

            // TODO: Implement EQ without old Channel struct!
            // copy values fromm Mixerparameter
            for (uint8_t peq = 0; peq < MAX_CHAN_EQS; peq++)
            {
                state->rChannel[changedIndex].peq[peq].type = config->GetUint(config->MpCalcId(CHANNEL_EQ_TYPE1, peq), changedIndex);
                state->rChannel[changedIndex].peq[peq].fc = config->GetFloat(config->MpCalcId(CHANNEL_EQ_FREQ1, peq), changedIndex);
                state->rChannel[changedIndex].peq[peq].Q = config->GetFloat(config->MpCalcId(CHANNEL_EQ_Q1, peq), changedIndex);
                state->rChannel[changedIndex].peq[peq].gain = config->GetFloat(config->MpCalcId(CHANNEL_EQ_GAIN1, peq), changedIndex);
            }
    
            dsp->SendEQ(changedIndex);
            dsp->SendLowcut(changedIndex);
        }
    }

    // Dynamics
    if (config->HasParametersChanged(MP_CAT::CHANNEL_DYNAMICS))
    {
        vector<uint> changedIndexes = config->GetChangedParameterIndexes(MP_CAT::CHANNEL_DYNAMICS);
        for (auto const& changedIndex : changedIndexes)
        {
            dsp->SendCompressor(changedIndex);
        }
    }   

    // FX Settings
    if (config->HasParameterChanged(FX_TYPE))
    {
        vector<uint> changedIndexes = config->GetChangedParameterIndexes({FX_TYPE});
        for (auto const& changedIndex : changedIndexes)
        {
            // TODO: Select mode!
            dsp->DSP2_SetFx(changedIndex, (enum FX_TYPE)config->GetUint(FX_TYPE, changedIndex), 2);
        }
    }

    // FX Parameter
    vector<MP_CAT> filter_cat = {
        MP_CAT::FX_PARAMETER_REVERB,
        MP_CAT::FX_PARAMETER_CHORUS,
        MP_CAT::FX_PARAMETER_DELAY,
        MP_CAT::FX_PARAMETER_TRANSIENTSHAPER,
        MP_CAT::FX_PARAMETER_OVERDRIVE,
        MP_CAT::FX_PARAMETER_MULTIBANDCOMPRESOR,
        MP_CAT::FX_PARAMETER_DYNAMICEQ
    };
    vector<uint> changedIndexes = config->GetChangedParameterIndexes(filter_cat);
    for (auto const& fxSlot : changedIndexes)
    {
        dsp->DSP2_SendFxParameter(fxSlot);
    }

    // The existing DSP2 sender transmits both oscillators together.
    for (uint index : config->GetChangedParameterIndexes({OSC_FREQUENCY, OSC_LEVEL}))
    {
        dsp->DSP2_SetOscillator(index, config->GetFloat(OSC_FREQUENCY, index),
                               config->GetFloat(OSC_LEVEL, index));
    }

    // CLEAR SOLO
    if(config->HasParameterChanged(CLEAR_SOLO_COMMAND))
	{
		ClearSolo();
	}

    helper->DEBUG_MIXER(DEBUGLEVEL_NORMAL, "sync done");
}

bool Mixer::halWriteHeadamp(uint8_t fpgaInputIndex, float gain, bool phantom) {
    if ((fpgaInputIndex >= FPGA_INPUT_IDX_XLR) && (fpgaInputIndex < (FPGA_INPUT_IDX_XLR + 32)))
    {
        uint8_t boardId = adda->GetXlrInBoardId(fpgaInputIndex);
        if (boardId < 4)
        {
            uint8_t addaChannel = ((fpgaInputIndex - 1) % 8) + 1;
            adda->SetGain(boardId, addaChannel, gain, phantom);
            return true;
        }
    }
    else if ((fpgaInputIndex >= FPGA_INPUT_IDX_AES50A) && (fpgaInputIndex < (FPGA_INPUT_IDX_AES50A + 48)))
    {
        uint8_t aesChan = fpgaInputIndex - FPGA_INPUT_IDX_AES50A + 1;
        fpga->AES50SetHeadampGain(0, aesChan, gain);
        fpga->AES50SetPhantomPowerState(0, aesChan, phantom);
        return true;
    }
    else if ((fpgaInputIndex >= FPGA_INPUT_IDX_AES50B) && (fpgaInputIndex < (FPGA_INPUT_IDX_AES50B + 48)))
    {
        uint8_t aesChan = fpgaInputIndex - FPGA_INPUT_IDX_AES50B + 1;
        fpga->AES50SetHeadampGain(1, aesChan, gain);
        fpga->AES50SetPhantomPowerState(1, aesChan, phantom);
        return true;
    }
    return false;
}

// set the gain of the local XLR / AES50 head-amp-control (always bound to channel's logical physical input)
void Mixer::halSendGain(uint8_t dspChannel) {
    uint8_t src = (uint8_t)config->GetChannelLogicalFpgaInput(dspChannel);
    if (halWriteHeadamp(src, config->GetFloat(CHANNEL_GAIN, dspChannel), config->GetBool(CHANNEL_PHANTOM, dspChannel)))
    {
        // update channel-volume for virtual gain-resolution-increment (2.5dB hardware steps)
        dsp->SendChannelVolume(dspChannel);
    }
}

void Mixer::syncChannelInsert(uint chanIndex) {
    if (chanIndex >= MAX_FPGA_TO_DSP1_CHANNELS) return;

    uint sendOut = config->GetUint(CHANNEL_INSERT_SEND, chanIndex);
    uint returnIn = config->GetUint(CHANNEL_INSERT_RETURN, chanIndex);

    String warning = config->GetChannelInsertConflictWarning(chanIndex, sendOut, returnIn);
    if (warning.length() > 0)
    {
        helper->Error("%s\n", warning.c_str());
    }

    // Restore previous insert send output slot if it changed
    uint8_t oldSend = prevInsertSend[chanIndex];
    if (oldSend != sendOut && config->IsValidInsertSendOutput(oldSend))
    {
        fpga->SendRoutingToFpga(oldSend - 1);
    }
    prevInsertSend[chanIndex] = (uint8_t)sendOut;

    // Always keep the insert send fed with the channel's logical physical input
    if (config->IsValidInsertSendOutput(sendOut))
    {
        fpga->SendRoutingToFpga(sendOut - 1);
    }

    // Update the DSP In FPGA slot (routes insert return when enabled, or logical input when bypassed)
    uint dspIn = config->GetUint(ROUTING_DSP_INPUT, chanIndex);
    if (dspIn >= DSP_BUF_IDX_DSPCHANNEL && dspIn < (DSP_BUF_IDX_AUX + 8))
    {
        fpga->SendRoutingToFpga((FPGA_OUTPUT_IDX_DSP - 1) + (dspIn - 1));
    }

    // Lock analog headamp insert returns to -2.0dB (+48V off) for exact 0.0dB with +2.0dB DSP trim
    if (config->IsValidInsertReturnInput(returnIn) && config->IsFpgaInputHeadamp(returnIn))
    {
        halWriteHeadamp((uint8_t)returnIn, -2.0f, false);
    }

    // Update DSP input trim (+2.0dB when analog insert return is active, 0.0dB otherwise)
    dsp->SendChannelVolume(chanIndex);
}

}
