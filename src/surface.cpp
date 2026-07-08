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

#include "surface.h"
#include "enum.h"

namespace OMC
{

Surface::Surface(X32BaseParameter* basepar): X32Base(basepar)
{
    if (config->IsModelAnyWing())
    {      
        surfaceController = new SurfaceControllerWing(basepar);
    }
    else
    {
        surfaceController = new SurfaceControllerXM32(basepar);
    }
}

void Surface::Init(SurfaceCallback callback, void* arg)
{
    surfaceController->Init(callback, arg);

    Reset();

    InitBanks();
	LoadDefaultSurfaceBinding();
}

void Surface::Reset()
{
    helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Reset surface ...");

    if (state->bodyless) 
    {
        // TODO: integrate in Testing GUI
    }
    else
    {  
        if (!config->IsModelAnyWing()) {
            int fd = open("/sys/class/leds/reset_surface/brightness", O_WRONLY);
            write(fd, "1", 1);
            usleep(100 * 1000);
            write(fd, "0", 1);
            close(fd);
            usleep(2000 * 1000);
        }
    }

    if (surfaceController) {
        surfaceController->Reset();
    }

    helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "... Done");
}

void Surface::Tick10ms()
{
    if (surfaceController)
    {
        surfaceController->Tick10ms();
    }
};

void Surface::Tick100ms()
{
    if (surfaceController)
    {
        surfaceController->Tick100ms();
    }
};

void Surface::ProcessUartDataSurface()
{
    surfaceController->ProcessUartData();
}


//####################################################################
//
//  ######  ##     ## ########  ########    ###     ######  ######## 
// ##    ## ##     ## ##     ## ##         ## ##   ##    ## ##       
// ##       ##     ## ##     ## ##        ##   ##  ##       ##       
//  ######  ##     ## ########  ######   ##     ## ##       ######   
//       ## ##     ## ##   ##   ##       ######### ##       ##       
// ##    ## ##     ## ##    ##  ##       ##     ## ##    ## ##       
//  ######   #######  ##     ## ##       ##     ##  ######  ######## 
//
//
// ########  #### ##    ## ########  #### ##    ##  ######   
// ##     ##  ##  ###   ## ##     ##  ##  ###   ## ##    ##  
// ##     ##  ##  ####  ## ##     ##  ##  ####  ## ##        
// ########   ##  ## ## ## ##     ##  ##  ## ## ## ##   #### 
// ##     ##  ##  ##  #### ##     ##  ##  ##  #### ##    ##  
// ##     ##  ##  ##   ### ##     ##  ##  ##   ### ##    ##  
// ########  #### ##    ## ########  #### ##    ##  ######   
//
//####################################################################



// Bind Surfaceelements to Mixerparameter or special functions
void Surface::LoadDefaultSurfaceBinding()
{
	LoadMainFaderSurfaceBinding();

    if (config->HasDisplay())
    {
        // Display - XM32 and WING
		config->SurfaceBind(SurfaceElementId::HOME, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::HOME));
		config->SurfaceBind(SurfaceElementId::METERS, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::METERS));
		config->SurfaceBind(SurfaceElementId::ROUTING, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::ROUTING));
		config->SurfaceBind(SurfaceElementId::SETUP, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::SETUP));
		config->SurfaceBind(SurfaceElementId::LIBRARY, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::LIBRARY));
		config->SurfaceBind(SurfaceElementId::EFFECTS, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::EFFECTS));
        config->SurfaceBind(SurfaceElementId::UTILITY, MixerparameterAction::TOGGLE, DISPLAY_UTILITY);
    }

    if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32ROrRack() || config->IsModelAnyWing())
    {
        config->SurfaceBind(SurfaceElementId::CLEAR_SOLO, MixerparameterAction::CLEAR_SOLO, CLEAR_SOLO);
    }

	if (config->IsModelAnyXM32())
	{
        if (config->HasDisplay())
	    {
            // Display
            config->SurfaceBind(SurfaceElementId::MUTE_GRP, MixerparameterAction::TOGGLE, DISPLAY_MUTE_GROUP);

            config->SurfaceBind(SurfaceElementId::LEFT, MixerparameterAction::TOGGLE, DISPLAY_LEFT);
            config->SurfaceBind(SurfaceElementId::RIGHT, MixerparameterAction::TOGGLE, DISPLAY_RIGHT);
            config->SurfaceBind(SurfaceElementId::UP, MixerparameterAction::TOGGLE, DISPLAY_UP);
            config->SurfaceBind(SurfaceElementId::DOWN, MixerparameterAction::TOGGLE, DISPLAY_DOWN);
        }

		if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32R())
		{
			// Config / Preamp
			config->SurfaceBind(SurfaceElementId::GAIN_ENCODER, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_GAIN);
			config->SurfaceBind(SurfaceElementId::PHANTOM_48V, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_PHANTOM);
			config->SurfaceBind(SurfaceElementId::PHASE_INVERT, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_PHASE_INVERT);
			config->SurfaceBind(SurfaceElementId::LOW_CUT_FREQ_ENCODER, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_LOWCUT_FREQ);
			config->SurfaceBind(SurfaceElementId::LOW_CUT, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_LOWCUT_ENABLE);
			config->SurfaceBind(SurfaceElementId::VIEW_CONFIG, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::CONFIG));

			// Gate
			config->SurfaceBind(SurfaceElementId::GATE_THRESHOLD_ENCODER, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_GATE_TRESHOLD);
			config->SurfaceBind(SurfaceElementId::GATE, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_GATE_ENABLE);
			config->SurfaceBind(SurfaceElementId::VIEW_GATE, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::GATE));

			// Dynamics
			config->SurfaceBind(SurfaceElementId::DYNAMICS_THRESHOLD_ENCODER, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_DYNAMICS_TRESHOLD);
			config->SurfaceBind(SurfaceElementId::COMP_EXP, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_COMPRESSOR_ENABLE);
			config->SurfaceBind(SurfaceElementId::VIEW_DYNAMICS, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::COMPRESSOR));

			// EQ
			config->SurfaceBind(SurfaceElementId::EQ_HCUT_LED, MixerparameterAction::SET__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ, (uint)EQ_TYPE::HICUT);
			config->SurfaceBind(SurfaceElementId::EQ_HSHV_LED, MixerparameterAction::SET__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ, (uint)EQ_TYPE::HIGHSHELV);
			//config->SurfaceBind(SurfaceElementId::EQ_VEQ_LED, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ, (uint)EQ_TYPE::V);
			config->SurfaceBind(SurfaceElementId::EQ_PEQ_LED, MixerparameterAction::SET__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ, (uint)EQ_TYPE::PEQ);
			config->SurfaceBind(SurfaceElementId::EQ_LSHV_LED, MixerparameterAction::SET__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ, (uint)EQ_TYPE::LOWSHELV);
			config->SurfaceBind(SurfaceElementId::EQ_LCUT_LED, MixerparameterAction::SET__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ, (uint)EQ_TYPE::LOWCUT);
			config->SurfaceBind(SurfaceElementId::EQ_MODE, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_TYPE1, (uint)BANKING_EQ);

			config->SurfaceBind(SurfaceElementId::EQ_Q_ENCODER, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_Q1, (uint)BANKING_EQ);
			config->SurfaceBind(SurfaceElementId::EQ_FREQ_ENCODER, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_FREQ1, (uint)BANKING_EQ);
			config->SurfaceBind(SurfaceElementId::EQ_GAIN_ENCODER, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_EQ_GAIN1, (uint)BANKING_EQ);

			config->SurfaceBind(SurfaceElementId::EQ_LOW, MixerparameterAction::SET_TO_INDEX, BANKING_EQ, 0);
			config->SurfaceBind(SurfaceElementId::EQ_LOW_MID, MixerparameterAction::SET_TO_INDEX, BANKING_EQ, 1);
			config->SurfaceBind(SurfaceElementId::EQ_HIGH_MID, MixerparameterAction::SET_TO_INDEX, BANKING_EQ, 2);
			config->SurfaceBind(SurfaceElementId::EQ_HIGH, MixerparameterAction::SET_TO_INDEX, BANKING_EQ, 3);
			config->SurfaceBind(SurfaceElementId::EQ, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_EQ_ENABLE);
			config->SurfaceBind(SurfaceElementId::VIEW_EQ, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::EQ));

			// Bus Sends
			config->SurfaceBind(SurfaceElementId::VIEW_MIX_BUS_SENDS, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::SENDS));
			if (config->IsModelX32FullOrM32())
			{
				config->SurfaceBind(SurfaceElementId::BUS_SEND_ENCODER_1, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_BUS_SEND01, (uint)BANKING_BUS_SENDS, 4);
				config->SurfaceBind(SurfaceElementId::BUS_SEND_ENCODER_2, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_BUS_SEND02, (uint)BANKING_BUS_SENDS, 4);
				config->SurfaceBind(SurfaceElementId::BUS_SEND_ENCODER_3, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_BUS_SEND03, (uint)BANKING_BUS_SENDS, 4);
				config->SurfaceBind(SurfaceElementId::BUS_SEND_ENCODER_4, MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL, CHANNEL_BUS_SEND04, (uint)BANKING_BUS_SENDS, 4);

				config->SurfaceBind(SurfaceElementId::BUS_SEND_1_4, MixerparameterAction::SET_TO_INDEX, BANKING_BUS_SENDS, 0);
				config->SurfaceBind(SurfaceElementId::BUS_SEND_5_8, MixerparameterAction::SET_TO_INDEX, BANKING_BUS_SENDS, 1);
				config->SurfaceBind(SurfaceElementId::BUS_SEND_9_12, MixerparameterAction::SET_TO_INDEX, BANKING_BUS_SENDS, 2);
				config->SurfaceBind(SurfaceElementId::BUS_SEND_13_16, MixerparameterAction::SET_TO_INDEX, BANKING_BUS_SENDS, 3);
			}
			
			// Bus Mixes
			config->SurfaceBind(SurfaceElementId::MAIN_BUS_LEVEL_ENCODER, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_VOLUME_SUB);
			config->SurfaceBind(SurfaceElementId::MONO_BUS, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SEND_SUB);
			config->SurfaceBind(SurfaceElementId::PAN_BAL_ENCODER, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_PANORAMA);
			config->SurfaceBind(SurfaceElementId::MAIN_LR_BUS, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SEND_LR);
			config->SurfaceBind(SurfaceElementId::VIEW_MAIN, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::MAIN));
			
			// Scenes
			config->SurfaceBind(SurfaceElementId::VIEW_SCENES, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::SCENES));

			// Assign
			
			// assign Channel 1-4 with Volume, LCD, Solo, Mute
			config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_1, MixerparameterAction::CHANGE, CHANNEL_VOLUME, 0);
			config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_2, MixerparameterAction::CHANGE, CHANNEL_VOLUME, 1);
			config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_3, MixerparameterAction::CHANGE, CHANNEL_VOLUME, 2);
			config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_4, MixerparameterAction::CHANGE, CHANNEL_VOLUME, 3);
			config->SurfaceBind(SurfaceElementId::ASSIGN_LCD_1, MixerparameterAction::LCD_Channel, NONE, 0);
			config->SurfaceBind(SurfaceElementId::ASSIGN_LCD_2, MixerparameterAction::LCD_Channel, NONE, 1);
			config->SurfaceBind(SurfaceElementId::ASSIGN_LCD_3, MixerparameterAction::LCD_Channel, NONE, 2);
			config->SurfaceBind(SurfaceElementId::ASSIGN_LCD_4, MixerparameterAction::LCD_Channel, NONE, 3);
			config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_2, MixerparameterAction::CHANGE, CHANNEL_VOLUME, 1);
			config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_3, MixerparameterAction::CHANGE, CHANNEL_VOLUME, 2);
			config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_4, MixerparameterAction::CHANGE, CHANNEL_VOLUME, 3);
			config->SurfaceBind(SurfaceElementId::ASSIGN_5, MixerparameterAction::TOGGLE, CHANNEL_SOLO, 0);
			config->SurfaceBind(SurfaceElementId::ASSIGN_6, MixerparameterAction::TOGGLE, CHANNEL_SOLO, 1);
			config->SurfaceBind(SurfaceElementId::ASSIGN_7, MixerparameterAction::TOGGLE, CHANNEL_SOLO, 2);
			config->SurfaceBind(SurfaceElementId::ASSIGN_8, MixerparameterAction::TOGGLE, CHANNEL_SOLO, 3);
			config->SurfaceBind(SurfaceElementId::ASSIGN_9, MixerparameterAction::TOGGLE, CHANNEL_MUTE, 0);
			config->SurfaceBind(SurfaceElementId::ASSIGN_10, MixerparameterAction::TOGGLE, CHANNEL_MUTE, 1);
			config->SurfaceBind(SurfaceElementId::ASSIGN_11, MixerparameterAction::TOGGLE, CHANNEL_MUTE, 2);
			config->SurfaceBind(SurfaceElementId::ASSIGN_12, MixerparameterAction::TOGGLE, CHANNEL_MUTE, 3);

			config->SurfaceBind(SurfaceElementId::ASSIGN_A, MixerparameterAction::SET_TO_INDEX, BANKING_ASSIGN, 0);
			config->SurfaceBind(SurfaceElementId::ASSIGN_B, MixerparameterAction::SET_TO_INDEX, BANKING_ASSIGN, 1);
			config->SurfaceBind(SurfaceElementId::ASSIGN_C, MixerparameterAction::SET_TO_INDEX, BANKING_ASSIGN, 2);
			config->SurfaceBind(SurfaceElementId::VIEW_ASSIGN, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::SETUP_SURFACE));

			// Remote
			config->SurfaceBind(SurfaceElementId::DAW_REMOTE, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::REMOTE1));

			// Banking of Input Section
			if (config->IsModelX32Full())
			{
				config->SurfaceBind(SurfaceElementId::CH1_16, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::CH1_16));
				config->SurfaceBind(SurfaceElementId::CH17_32, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::CH17_32));
				config->SurfaceBind(SurfaceElementId::AUX_USB_RX_RET, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::AUX_USB_FX_RET));
				config->SurfaceBind(SurfaceElementId::BUS_MASTER, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::BUS1_16));
			}
			else if (config->IsModelX32CompactOrProducerOrM32R())
			{
				config->SurfaceBind(SurfaceElementId::CH1_8, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::CH1_8));
				config->SurfaceBind(SurfaceElementId::CH9_16, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::CH9_16));
				config->SurfaceBind(SurfaceElementId::CH17_24, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::CH17_24));
				config->SurfaceBind(SurfaceElementId::CH25_32, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::CH25_32));
				config->SurfaceBind(SurfaceElementId::AUX_USB, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::AUX_USB));
				config->SurfaceBind(SurfaceElementId::FX_RET, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::FX_RET));
				config->SurfaceBind(SurfaceElementId::BUS1_8_MASTER, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::BUS1_8));
				config->SurfaceBind(SurfaceElementId::BUS9_16_MASTER, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::BUS9_16));
			}

			// Banking of Bus Section
			config->SurfaceBind(SurfaceElementId::DCA, MixerparameterAction::SET_TO_INDEX, BANKING_BUS, (uint)(OMCBankId::DCA));
			config->SurfaceBind(SurfaceElementId::BUS1_8, MixerparameterAction::SET_TO_INDEX, BANKING_BUS, (uint)(OMCBankId::BUS1_8));
			config->SurfaceBind(SurfaceElementId::BUS9_16, MixerparameterAction::SET_TO_INDEX, BANKING_BUS, (uint)(OMCBankId::BUS9_16));
			config->SurfaceBind(SurfaceElementId::MATRIX_MAIN, MixerparameterAction::SET_TO_INDEX, BANKING_BUS, (uint)(OMCBankId::MATRIX_MAIN));

			// Mute Groups
			config->SurfaceBind(SurfaceElementId::MUTE_GROUP_1, MixerparameterAction::TOGGLE, MUTE_GROUP_1_MUTE);
			config->SurfaceBind(SurfaceElementId::MUTE_GROUP_2, MixerparameterAction::TOGGLE, MUTE_GROUP_2_MUTE);
			config->SurfaceBind(SurfaceElementId::MUTE_GROUP_3, MixerparameterAction::TOGGLE, MUTE_GROUP_3_MUTE);
			config->SurfaceBind(SurfaceElementId::MUTE_GROUP_4, MixerparameterAction::TOGGLE, MUTE_GROUP_4_MUTE);
			config->SurfaceBind(SurfaceElementId::MUTE_GROUP_5, MixerparameterAction::TOGGLE, MUTE_GROUP_5_MUTE);
			config->SurfaceBind(SurfaceElementId::MUTE_GROUP_6, MixerparameterAction::TOGGLE, MUTE_GROUP_6_MUTE);
		}		

		if (config->IsModelX32Rack())
		{
			config->SurfaceBind(SurfaceElementId::VIEW_SCENES, MixerparameterAction::SET_TO_INDEX, ACTIVE_PAGE, (uint)(X32_PAGE::SCENES));

			config->SurfaceBind(SurfaceElementId::CHANNEL_ENCODER, MixerparameterAction::CHANGE, SELECTED_CHANNEL);

			config->SurfaceBind(SurfaceElementId::CHANNEL_SOLO, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SOLO);
			config->SurfaceBind(SurfaceElementId::CHANNEL_MUTE, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_MUTE);
			config->SurfaceBind(SurfaceElementId::CHANNEL_LEVEL, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_VOLUME);
			config->SurfaceBind(SurfaceElementId::MAIN_LEVEL, MixerparameterAction::CHANGE, CHANNEL_VOLUME, int(X32_VCHANNEL_BLOCK::MAIN));
		}

        if (config->IsModelX32Core())
		{
            //config->SurfaceBind(SurfaceElementId::SCENE_SETUP, MixerparameterAction::, SELECTED_CHANNEL);

            config->SurfaceBind(SurfaceElementId::CHANNEL_ENCODER, MixerparameterAction::CHANGE, SELECTED_CHANNEL);
            config->SurfaceBind(SurfaceElementId::CORE_LCD, MixerparameterAction::LCD_Channel, NONE);
            config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_1, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_VOLUME);
            config->SurfaceBind(SurfaceElementId::ASSIGN_ENCODER_2, MixerparameterAction::CHANGE, CHANNEL_VOLUME, int(X32_VCHANNEL_BLOCK::MAIN));

            // DEBUG
            config->SurfaceBind(SurfaceElementId::ASSIGN_3, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SOLO);
			config->SurfaceBind(SurfaceElementId::ASSIGN_4, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_MUTE);
			config->SurfaceBind(SurfaceElementId::ASSIGN_5, MixerparameterAction::CLEAR_SOLO, NONE);
			config->SurfaceBind(SurfaceElementId::ASSIGN_6, MixerparameterAction::SET_TO_INDEX, LCD_CONTRAST, 40);
        }
	}

	if (config->IsModelAnyWing())
	{
		if (config->IsModelWingCompact())
		{
			config->SurfaceBind(SurfaceElementId::WING_CH1_12, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::WING_1_12));
			config->SurfaceBind(SurfaceElementId::WING_CH13_24, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::WING_13_24));
			config->SurfaceBind(SurfaceElementId::WING_CH25_36, MixerparameterAction::SET_TO_INDEX, BANKING_INPUT, (uint)(OMCBankId::WING_25_36));

			config->SurfaceBind(SurfaceElementId::ASSIGN_1, MixerparameterAction::TOGGLE, DISPLAY_LEFT);
			config->SurfaceBind(SurfaceElementId::ASSIGN_2, MixerparameterAction::TOGGLE, DISPLAY_RIGHT);
			config->SurfaceBind(SurfaceElementId::ASSIGN_4, MixerparameterAction::TOGGLE, DISPLAY_UP);
			config->SurfaceBind(SurfaceElementId::ASSIGN_6, MixerparameterAction::TOGGLE, DISPLAY_DOWN);
		}
	}
}

void Surface::LoadMainFaderSurfaceBinding()
{
    // Main Fader

    if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32ROrRack())
    {
        config->SurfaceBind(SurfaceElementId::BOARD_R_SELECT_MAIN, MixerparameterAction::SET_TO_INDEX, SELECTED_CHANNEL, int(X32_VCHANNEL_BLOCK::MAIN));
        config->SurfaceBind(SurfaceElementId::BOARD_R_SOLO_MAIN, MixerparameterAction::TOGGLE, CHANNEL_SOLO, int(X32_VCHANNEL_BLOCK::MAIN));
        config->SurfaceBind(SurfaceElementId::BOARD_R_LCD_MAIN, MixerparameterAction::LCD_Channel, NONE, int(X32_VCHANNEL_BLOCK::MAIN));
        config->SurfaceBind(SurfaceElementId::BOARD_R_MUTE_MAIN, MixerparameterAction::TOGGLE, CHANNEL_MUTE, int(X32_VCHANNEL_BLOCK::MAIN));
        config->SurfaceBind(SurfaceElementId::BOARD_R_FADER_MAIN, MixerparameterAction::SET, CHANNEL_VOLUME, int(X32_VCHANNEL_BLOCK::MAIN));
    } 
    else if (config->IsModelAnyWing())
    {
        if (config->IsModelWingCompact())
        {
            config->SurfaceBind(SurfaceElementId::WING_LCD_13, MixerparameterAction::LCD_Channel, NONE, int(X32_VCHANNEL_BLOCK::MAIN));
            config->SurfaceBind(SurfaceElementId::WING_SELECT_13, MixerparameterAction::SET_TO_INDEX, SELECTED_CHANNEL, int(X32_VCHANNEL_BLOCK::MAIN));
            config->SurfaceBind(SurfaceElementId::WING_SOLO_13, MixerparameterAction::TOGGLE, CHANNEL_SOLO, int(X32_VCHANNEL_BLOCK::MAIN));
            config->SurfaceBind(SurfaceElementId::WING_MUTE_13, MixerparameterAction::TOGGLE, CHANNEL_MUTE, int(X32_VCHANNEL_BLOCK::MAIN));
            config->SurfaceBind(SurfaceElementId::WING_FADER_13, MixerparameterAction::SET, CHANNEL_VOLUME, int(X32_VCHANNEL_BLOCK::MAIN));
        }
    }
}

//#####################################################################################################################
//
// ########     ###    ##    ## ##     ## #### ##    ##  ######   
// ##     ##   ## ##   ###   ## ##    ##   ##  ###   ## ##    ##  
// ##     ##  ##   ##  ####  ## ##   ##    ##  ####  ## ##        
// ########  ##     ## ## ## ## #####      ##  ## ## ## ##   #### 
// ##     ## ######### ##  #### ##   ##    ##  ##  #### ##    ##  
// ##     ## ##     ## ##   ### ##    ##   ##  ##   ### ##    ##  
// ########  ##     ## ##    ## ##     ## #### ##    ##  ######   
//
//#####################################################################################################################

void Surface::InitBanks()
{
	if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32R())
	{
		uint channel_strip_size = 8;

		InitBank_Channelstrip(new X32FaderBank(OMCBankId::CH1_8, "Channel 1-8", channel_strip_size), 0);
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::CH9_16, "Channel 9-16", channel_strip_size), 8);
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::CH17_24, "Channel 17-24", channel_strip_size), 16);
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::CH25_32, "Channel 25-32", channel_strip_size), 24);
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::AUX_USB, "AUX/USB", channel_strip_size), (uint)(X32_VCHANNEL_BLOCK::AUX));
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::FX_RET, "FX Return", channel_strip_size), (uint)(X32_VCHANNEL_BLOCK::FXRET));
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::BUS1_8, "Bus 1-8", channel_strip_size), (uint)(X32_VCHANNEL_BLOCK::BUS));
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::BUS9_16, "Bus 9-16", channel_strip_size), ((uint)(X32_VCHANNEL_BLOCK::BUS)) + 8);
		InitBank_Channelstrip_DCA(new X32FaderBank(OMCBankId::DCA, "DCA", channel_strip_size), (uint)(X32_VCHANNEL_BLOCK::DCA));
		InitBank_Channelstrip(new X32FaderBank(OMCBankId::MATRIX_MAIN, "Matrix/Main", channel_strip_size), (uint)(X32_VCHANNEL_BLOCK::MATRIX));
		InitBank_DMX(new X32FaderBank(OMCBankId::REMOTE1, "Remote1", channel_strip_size), 0);
		InitBank_DMX(new X32FaderBank(OMCBankId::REMOTE2, "Remote2", channel_strip_size), 8);
		InitBank_Flex(new X32FaderBank(OMCBankId::FLEX1, "Flex1", channel_strip_size));
		InitBank_Flex(new X32FaderBank(OMCBankId::FLEX2, "Flex2", channel_strip_size));
		InitBank_Flex(new X32FaderBank(OMCBankId::FLEX3, "Flex3", channel_strip_size));
	}
	else if (config->IsModelWingCompact())
	{
		uint channel_strip_size = 12;

		InitBank_Channelstrip_WING(new X32FaderBank(OMCBankId::WING_1_12, "Channel 1-12", channel_strip_size), 0);
		InitBank_Channelstrip_WING(new X32FaderBank(OMCBankId::WING_13_24, "Channel 1-12", channel_strip_size), 12);
		InitBank_Channelstrip_WING(new X32FaderBank(OMCBankId::WING_25_36, "Channel 1-12", channel_strip_size), 24);
	}
}

void Surface::InitBank_Channelstrip_WING(X32FaderBank* bank, uint offset)
{
    for (uint i = 0; i < 12; i++)
    	{
			bank->channelstrip[i]->lcd->FillBindingParameter(MixerparameterAction::LCD_Channel, NONE, i + offset);
			bank->channelstrip[i]->select->FillBindingParameter(MixerparameterAction::SET_TO_INDEX, SELECTED_CHANNEL, i + offset);
			bank->channelstrip[i]->solo->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_SOLO, i + offset);
			// bank->channelstrip[i]->vumeter->FillBindingParameter(MixerparameterAction::VUMETER, NONE, i + offset);
			bank->channelstrip[i]->mute->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_MUTE, i + offset);
			bank->channelstrip[i]->fader->FillBindingParameter(MixerparameterAction::SET, CHANNEL_VOLUME, i + offset);
		}

	banks[(uint)(bank->GetID())] = bank;
}

void Surface::InitBank_Channelstrip(X32FaderBank* bank, uint offset)
{
    for (uint i = 0; i < 8; i++)
    {
        SetChannelstripBinding(bank, i, i + offset);
    }

	banks[(uint)(bank->GetID())] = bank;
}

void Surface::SetChannelstripBinding(X32FaderBank *bank, uint i, uint chanIndex)
{
    bank->channelstrip[i]->select->FillBindingParameter(MixerparameterAction::SET_TO_INDEX, SELECTED_CHANNEL, chanIndex);
    bank->channelstrip[i]->vumeter->FillBindingParameter(MixerparameterAction::VUMETER, NONE, chanIndex);
    bank->channelstrip[i]->solo->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_SOLO, chanIndex);
    bank->channelstrip[i]->lcd->FillBindingParameter(MixerparameterAction::LCD_Channel, NONE, chanIndex);
    bank->channelstrip[i]->mute->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_MUTE, chanIndex);
    bank->channelstrip[i]->fader->FillBindingParameter(MixerparameterAction::SET, CHANNEL_VOLUME, chanIndex);
}

void Surface::InitBank_Channelstrip_DCA(X32FaderBank* bank, uint offset)
{
    for (uint i = 0; i < 8; i++)
    {
        bank->channelstrip[i]->select->FillBindingParameter(MixerparameterAction::TOGGLE, config->MpCalcId(DCA_GROUP_1_MASTER, i), 0);
		//bank->channelstrip[i]->vumeter->FillBindingParameter(MixerparameterAction::VUMETER, NONE, i + offset);
        bank->channelstrip[i]->solo->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_SOLO, i + offset);
		bank->channelstrip[i]->lcd->FillBindingParameter(MixerparameterAction::LCD_Channel, NONE, i + offset);
        bank->channelstrip[i]->mute->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_MUTE, i + offset);
        bank->channelstrip[i]->fader->FillBindingParameter(MixerparameterAction::SET, CHANNEL_VOLUME, i + offset);
    }

	banks[(uint)(bank->GetID())] = bank;
}

void Surface::InitBank_DMX(X32FaderBank* bank, uint offset)
{
    for (uint i = 0; i < 8; i++)
    {
        //bank->channelstrip[i]->select->FillBindingParameter(MixerparameterAction::SET_TO_INDEX, SELECTED_CHANNEL, i + offset);
		//bank->channelstrip[i]->vumeter->FillBindingParameter(MixerparameterAction::VUMETER, NONE, i + offset);
        //bank->channelstrip[i]->solo->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_SOLO, i + offset);
		bank->channelstrip[i]->lcd->FillBindingParameter(MixerparameterAction::LCD_Artnet, DMX_ARTNET_VALUE, i + offset);
        //bank->channelstrip[i]->mute->FillBindingParameter(MixerparameterAction::TOGGLE, CHANNEL_MUTE, i + offset);
        bank->channelstrip[i]->fader->FillBindingParameter(MixerparameterAction::DMX, DMX_ARTNET_VALUE, i + offset);
    }

	banks[(uint)(bank->GetID())] = bank;
}

// create a empty bank for flexible use
void Surface::InitBank_Flex(X32FaderBank* bank)
{
    for (uint i = 0; i < 8; i++)
    {
        bank->channelstrip[i]->select->FillBindingParameter(MixerparameterAction::NONE, NONE, 0);
		bank->channelstrip[i]->vumeter->FillBindingParameter(MixerparameterAction::NONE, NONE, 0);
        bank->channelstrip[i]->solo->FillBindingParameter(MixerparameterAction::NONE, NONE, 0);
		bank->channelstrip[i]->lcd->FillBindingParameter(MixerparameterAction::NONE, NONE, 0);
        bank->channelstrip[i]->mute->FillBindingParameter(MixerparameterAction::NONE, NONE, 0);
        bank->channelstrip[i]->fader->FillBindingParameter(MixerparameterAction::NONE, NONE, 0);
    }

	banks[(uint)(bank->GetID())] = bank;
}

void Surface::LoadBank(OMCBankTarget target, OMCBankId id)
{
	if (id == OMCBankId::None)
	{
		return;
	}

	helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Load Bank %d to section %d", id, target);

	X32FaderBank* bank_to_load = banks[(uint)id];

	if (bank_to_load == 0)
	{
		return;
	}

	if (target == OMCBankTarget::InputSection)
	{
		for (uint i = 0; i < 8; i++)
		{
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_L_SELECT_1 + i), bank_to_load->channelstrip[i]->select);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_L_VUMETER_1 + i), bank_to_load->channelstrip[i]->vumeter);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_L_SOLO_1 + i), bank_to_load->channelstrip[i]->solo);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_L_LCD_1 + i), bank_to_load->channelstrip[i]->lcd);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_L_MUTE_1 + i), bank_to_load->channelstrip[i]->mute);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_L_FADER_1 + i), bank_to_load->channelstrip[i]->fader);
		}
	}

	if (target == OMCBankTarget::InputSection2)
	{
		for (uint i = 0; i < 8; i++)
		{
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_M_SELECT_1 + i), bank_to_load->channelstrip[i]->select);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_M_VUMETER_1 + i), bank_to_load->channelstrip[i]->vumeter);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_M_SOLO_1 + i), bank_to_load->channelstrip[i]->solo);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_M_LCD_1 + i), bank_to_load->channelstrip[i]->lcd);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_M_MUTE_1 + i), bank_to_load->channelstrip[i]->mute);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_M_FADER_1 + i), bank_to_load->channelstrip[i]->fader);
		}
	}

	if (target == OMCBankTarget::BusSection)
	{
		for (uint i = 0; i < 8; i++)
		{
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_R_SELECT_1 + i), bank_to_load->channelstrip[i]->select);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_R_VUMETER_1 + i), bank_to_load->channelstrip[i]->vumeter);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_R_SOLO_1 + i), bank_to_load->channelstrip[i]->solo);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_R_LCD_1 + i), bank_to_load->channelstrip[i]->lcd);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_R_MUTE_1 + i), bank_to_load->channelstrip[i]->mute);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::BOARD_R_FADER_1 + i), bank_to_load->channelstrip[i]->fader);
		}
	}

	if (target == OMCBankTarget::WING_COMPACT)
	{
		for (uint i = 0; i < 12; i++)
		{
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::WING_LCD_1 + i), bank_to_load->channelstrip[i]->lcd);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::WING_SELECT_1 + i), bank_to_load->channelstrip[i]->select);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::WING_SOLO_1 + i), bank_to_load->channelstrip[i]->solo);
			//config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::WING_VUMETER_1 + i), bank_to_load->channelstrip[i]->vumeter);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::WING_MUTE_1 + i), bank_to_load->channelstrip[i]->mute);
			config->SurfaceBindParameter((SurfaceElementId)((uint)SurfaceElementId::WING_FADER_1 + i), bank_to_load->channelstrip[i]->fader);
		}
	}

    bankloaded.insert({target, id});
}

void Surface::LoadAssignBank(X32AssignBankId bankId)
{
	OMCAssignBank* bank_to_load = config->GetAssignBank(bankId);

	helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Load %s", bank_to_load->GetName().c_str());

    for (auto const& [id, binding] : *(bank_to_load->bindingMap))
    {
		config->SurfaceBindParameter(id, binding);
	}
}

OMCBankId Surface::GetLoadedBankId(OMCBankTarget target)
{
    if (bankloaded.count(target))
    {
        return bankloaded.at(target);
    }

    return OMCBankId::None;
}

X32FaderBank* Surface::GetLoadedBank(OMCBankTarget target)
{
    OMCBankId id = GetLoadedBankId(target);
    return GetBank(id);
}

X32FaderBank* Surface::GetBank(OMCBankId id)
{
    return banks[(uint)id];
}

// reset the banks to default blank
void Surface::ResetBank(OMCBankId id)
{
    banks[(uint)id]->Reset();
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedIncrement(uint8_t pct) {
    uint8_t num_leds_to_light = 0;
/*
    if (pct <= 50) {
        // Scale 0-50 to 0-6 LEDs
        num_leds_to_light = (uint8_t)((float)pct / 50.0f * 6.0f);
    } else {
        // Scale 51-100 to 7-12 LEDs (6 more LEDs)
        // From 51 to 100, there are 50 steps.
        // We need to add (pct - 50) steps mapped to the remaining 6 LEDs.
        // (pct - 50) / 50.0f * 6.0f
        num_leds_to_light = 6 + (uint8_t)(((float)(pct - 50) / 50.0f) * 6.0f);

        if (num_leds_to_light > 12) {
            num_leds_to_light = 12;
        }
    }
*/
    num_leds_to_light = (uint8_t)((float)pct / 100.0f * 12.5f);
    if (num_leds_to_light > 13) {
        num_leds_to_light = 13;
    }

    uint16_t led_mask = 0;
    if (num_leds_to_light > 0) {
        led_mask = (1U << num_leds_to_light) - 1;
    }

    return led_mask;
}


uint16_t Surface::CalcEncoderRingLedDirect(uint8_t num_leds_to_light)
{
    if (num_leds_to_light > 13)
    {
        num_leds_to_light = 13;
    }

    uint16_t led_mask = 0;
    if (num_leds_to_light > 0)
    {
        led_mask = (1U << num_leds_to_light) - 1;
    }

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedDecrement(uint8_t pct) {
    uint8_t num_leds_to_light = 0;
/*
    if (pct <= 50) {
        // Scale 0-50 to 0-6 LEDs
        num_leds_to_light = (uint8_t)((float)pct / 50.0f * 6.0f);
    } else {
        // Scale 51-100 to 7-12 LEDs (6 more LEDs)
        // From 51 to 100, there are 50 steps.
        // We need to add (pct - 50) steps mapped to the remaining 6 LEDs.
        // (pct - 50) / 50.0f * 6.0f
        num_leds_to_light = 6 + (uint8_t)(((float)(pct - 50) / 50.0f) * 6.0f);

        if (num_leds_to_light > 12) {
            num_leds_to_light = 12;
        }
    }
*/
    num_leds_to_light = (uint8_t)((float)pct / 100.0f * 12.5f);
    if (num_leds_to_light > 13) {
        num_leds_to_light = 13;
    }

    uint16_t led_mask = 0;
    
    // Bits von rechts nach links setzen
    for (uint8_t i = 0; i < num_leds_to_light; ++i) {
        led_mask |= (1U << (12 - i));
    }

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedPosition(uint8_t pct) {
    uint8_t led_index = (uint8_t)(((float)pct / 100.0f) * 12.0f + 0.5f); // +0.5f für Rundung

    if (led_index > 12) {
        led_index = 12;
    }

    return (1U << led_index);
}

uint16_t Surface::CalcEncoderRingLedDbfs(float dbfs, bool onlyPosition) {
    uint16_t led_mask = 0;

   // if (config->IsModelX32Rack()){
        // X32Rack: Channel Level, Mail LR Level

        // LEDs dBfs
        // 1 -50
        // 2 -40
        // 3 -30
        // 4 -24
        // 5 -18
        // 6 -12
        // 7 -9
        // 8 -6
        // 9 -3
        // 10 0
        // 11 3
        // 12 6
        // 13 10


        if (dbfs > -60) { 
            uint8_t led_index = 0;

            if (dbfs >= 10) led_index = 12;
            else if (dbfs >= 6) led_index = 11;
            else if (dbfs >= 3) led_index = 10;
            else if (dbfs >= 0) led_index = 9;
            else if (dbfs >= -3) led_index = 8;
            else if (dbfs >= -6) led_index = 7;
            else if (dbfs >= -9) led_index = 6;
            else if (dbfs >= -12) led_index = 5;
            else if (dbfs >= -18) led_index = 4;
            else if (dbfs >= -24) led_index = 3;
            else if (dbfs >= -30) led_index = 2;
            else if (dbfs >= -40) led_index = 1;
            else if (dbfs > -50) led_index = 0;

            if (onlyPosition){
                led_mask = (1U << led_index);
            } else {
                led_mask = (1U << (led_index + 1)) -1;
            }
        } 

   // } else {
    
        // led_index = (uint8_t)(((float)pct / 100.0f) * 12.0f + 0.5f); // +0.5f für Rundung

        // if (led_index > 12) {
        //     led_index = 12;
        // }
    //}

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedBalance(uint8_t pct) {
    uint16_t led_mask = 0;

    if (pct < 50) {
        float scale = (float)pct / 50.0f; // Skaliert 0-49 auf 0-0.98
        // (scale * 6) = Anzahl der LEDs, die an sind.
        uint8_t num_on_left_side = (uint8_t)roundf(scale * 6.5f);

        // Sicherstellen, dass mindestens Bit 6 an ist, wenn pct < 50
        if (num_on_left_side < 1) num_on_left_side = 1;

        // Setze die Bits von Bit 0 bis zum berechneten Index
        for (int i = 0; i < num_on_left_side; ++i) {
            if (i <= 6) { // Sicherstellen, dass wir im Bereich Bits 0-6 bleiben
                led_mask |= (1U << i);
            }
        }

        // invert LED-mask
        led_mask ^= 0xFFFF;
        led_mask &= 0b0000000001111111;

    } else { // pct >= 50
        // Skaliere (pct - 50) von 1-50 auf die Anzahl der LEDs, die von Bit 6 nach rechts zusätzlich an sein sollen.
        float scale = (float)(pct - 50) / 50.0f; // Skaliert 51-100 auf 0.02-1
        uint8_t num_on_right_side = (uint8_t)roundf(1.0f + (scale * 6.5f)); // 1 für Bit 6, plus 6 weitere LEDs

        // Sicherstellen, dass mindestens Bit 6 an ist, wenn pct > 50
        if (num_on_right_side < 1) num_on_right_side = 1;

        // Setze die Bits von Bit 6 bis zum berechneten Index
        for (int i = 0; i < num_on_right_side; ++i) {
            if ((6 + i) <= 12) { // Sicherstellen, dass wir im Bereich Bits 6-12 bleiben
                led_mask |= (1U << (6 + i));
            }
        }
    }

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedWidth(uint8_t pct) {
    if (pct == 0) {
        return (1U << 6); // Setzt nur Bit 6
    }

    float scaled_value = (float)(pct - 1) / 99.0f; // Skaliert 1-100 auf 0-1

    // Anzahl der zusätzlichen LEDs, die auf der linken Seite von Bit 6 aus eingeschaltet werden sollen.
    // Max 6 LEDs (Bits 0-5).
    uint8_t num_left_additional_leds = (uint8_t)roundf(scaled_value * 6.0f);
    // Anzahl der zusätzlichen LEDs, die auf der rechten Seite von Bit 6 aus eingeschaltet werden sollen.
    // Max 7 LEDs (Bits 7-12).
    uint8_t num_right_additional_leds = (uint8_t)roundf(scaled_value * 6.0f);

    uint16_t led_mask = (1U << 6); // Starte mit Bit 6 gesetzt

    // Schalte die zusätzlichen LEDs auf der linken Seite ein
    for (int i = 0; i < num_left_additional_leds; ++i) {
        if ((6 - (i + 1)) >= 0) { // Sicherstellen, dass der Index nicht negativ wird
            led_mask |= (1U << (6 - (i + 1)));
        }
    }

    // Schalte die zusätzlichen LEDs auf der rechten Seite ein
    for (int i = 0; i < num_right_additional_leds; ++i) {
        if ((6 + (i + 1)) <= 12) { // Sicherstellen, dass der Index nicht über 15 hinausgeht
            led_mask |= (1U << (6 + (i + 1)));
        }
    }

    // Bei 100% sollen alle Bits gesetzt sein (0x1FFF).
    // Die Berechnung oben sollte dies bereits erreichen, aber eine explizite Prüfung schadet nicht.
    if (pct == 100) {
        return 0x1FFF; // Alle 13 Bits setzen
    }

    return led_mask;
}

void Surface::SetBrightnessAllBoards(uint8_t brightness) {
    SetBrightness(X32_BOARD_MAIN, brightness);
    SetBrightness(X32_BOARD_L, brightness);
    SetBrightness(X32_BOARD_M, brightness);
    SetBrightness(X32_BOARD_R, brightness);
}

// boardId = 0, 1, 4, 5, 8
// index = 0 ... 8
// brightness = 0 ... 255
void Surface::SetBrightness(uint8_t boardId, uint8_t brightness) {
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('C'); // class: C = Controlmessage
    message.AddDataByte('B'); // index
    message.AddDataByte(brightness);
    surfaceController->SendData(&message, true);
}

void Surface::SetContrastAllBoards(uint8_t contrast) {
    SetContrast(X32_BOARD_MAIN, contrast);
    SetContrast(X32_BOARD_L, contrast);
    SetContrast(X32_BOARD_M, contrast);
    SetContrast(X32_BOARD_R, contrast);
}

// boardId = 0, 1, 4, 5, 8
// contrast = 0 ... 255
void Surface::SetContrast(uint8_t boardId, uint8_t contrast) {
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('C'); // class: C = Controlmessage
    message.AddDataByte('C'); // index
    message.AddDataByte(contrast & 0x3F);
    surfaceController->SendData(&message, true);
}





// set 7-Segment display on X32 Rack
// dot = 128
void Surface::SetX32RackDisplayRaw(uint8_t p_value2, uint8_t p_value1){
    SurfaceMessage message;
    message.AddDataByte(0x80);
    message.AddDataByte('D'); // Display
    message.AddDataByte(0x80);
    message.AddDataByte(p_value2); 
    message.AddDataByte(p_value1);
    surfaceController->SendData(&message, true);
}

// set 7-Segment display on X32 Rack
// dot = 128 or 256
void Surface::SetX32RackDisplay(uint8_t vChannelIndex){
    uint8_t vChannelNumber = vChannelIndex + 1;
    if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::NORMAL)) {
        uint8_t segment_l = vChannelNumber < 10 ? 0 : int2segment((uint8_t)(vChannelNumber/10));
        
        SetX32RackDisplayRaw(segment_l, int2segment(vChannelNumber % 10));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::AUX)) {
        SetX32RackDisplayRaw(int2segment('A'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::AUX));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::FXRET)) {
        SetX32RackDisplayRaw(int2segment('F'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::FXRET));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::BUS)) {
        uint8_t number = vChannelNumber - (uint)X32_VCHANNEL_BLOCK::BUS;
        SetX32RackDisplayRaw(int2segment((uint8_t)(number/10)), int2segment(number % 10));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::MATRIX)) {
        SetX32RackDisplayRaw(int2segment('M'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::MATRIX));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::DCA)) {
        SetX32RackDisplayRaw(int2segment('D'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::DCA));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::MAIN)) {
        SetX32RackDisplayRaw(int2segment('M'), int2segment(' '));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::MAINSUB)) {
        SetX32RackDisplayRaw(int2segment('M'), int2segment(5));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::SPECIAL)) {
        SetX32RackDisplayRaw(int2segment(5), int2segment(' '));
    }
}

uint8_t Surface::int2segment(int8_t p_value){
    switch (p_value){
        case 0:
            return 63;
        case 1:
            return 6;
        case 2:
            return 91;
        case 3:
            return 79;
        case 4:
            return 102;
        case 5:
            return 109;
        case 6:
            return 125;
        case 7:
            return 7;
        case 8:
            return 127;
        case 9:
            return 111;
        case 'a':
        case 'A':
            return 119;
        case 'b':
        case 'B':
            return 124;
        case 'd':
        case 'D':
            return 94;
        case 'f':
        case 'F':
            return 1 + 32 + 64 + 16;
        case 'm':
        case 'M':
            return 55;
        case ' ':
            return 0;
        default:
            return 0;
    }
}


void Surface::SetMeterLed(uint8_t boardId, uint8_t index, uint8_t leds)
{
    surfaceController->SetMeterLed(boardId, index, leds);
}


// preamp = 8-bit bitwise (bit 0=Sig, 1=-30dB ... 6=-3dB, 7=Clip)
// meter = 32-bit bitwise (bit 0=-45dB ... 15=-4, 16=-2, 19=Clip, 20+=unused)
void Surface::SetMeterLedMain_Rack(uint8_t preamp, uint32_t meterL, uint32_t meterR, uint32_t meterSolo)
{
    SurfaceMessage message;
    message.AddDataByte(0x80); // start message for specific boardId
    message.AddDataByte('M'); // class: M = Meter
    message.AddDataByte(0); // index
    message.AddDataByte(preamp);

    // Example for Big Meters on X32Rack (!different scale to X32Full/Compact)
    //message.AddDataByte(0b11110000);  // first nibble shifted 4 to left
    //message.AddDataByte(0b11111111);  // bit 4..12 shifted 4 to right
    //message.AddDataByte(0b10110111);  // last bits are crazy splitted :-/

    message.AddDataByte((uint8_t)(meterL<<4));
    message.AddDataByte((uint8_t)(meterL>>4));
    message.AddDataByte((((uint8_t)(meterL>>12))&0b10000111) | (((uint8_t)(meterL>>11))&0b00110000));
    message.AddDataByte(0);
    message.AddDataByte((uint8_t)(meterR<<4));
    message.AddDataByte((uint8_t)(meterR>>4));
    message.AddDataByte((((uint8_t)(meterR>>12))&0b10000111) | (((uint8_t)(meterR>>11))&0b00110000));
    message.AddDataByte(0x00);
    message.AddDataByte((uint8_t)(meterSolo<<4));
    message.AddDataByte((uint8_t)(meterSolo>>4));
    message.AddDataByte((((uint8_t)(meterSolo>>12))&0b10000111) | (((uint8_t)(meterSolo>>11))&0b00110000));
    message.AddDataByte(0x00);
    surfaceController->SendData(&message, true);
}

void Surface::SetMeterLedMain_Producer(uint8_t preamp, uint8_t dynamics, uint32_t meterL, uint32_t meterR, uint32_t meterSolo)  {
    
    // TODO
    // meter scale is from Rack
    // dynamics is like Full/COmpact
    // --> has to be tested
    
}

// leds = 8-bit bitwise (bit 0=-60dB ... 4=-6dB, 5=Clip, 6=Gate, 7=Comp)
// leds = 32-bit bitwise (bit 0=-57dB ... 22=-2, 23=-1, 24=Clip)
void Surface::SetMeterLedMain_FullOrCompact(uint8_t preamp, uint8_t dynamics, uint32_t meterL, uint32_t meterR, uint32_t meterSolo) {
    // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
    // 0x4C, index, leds.b[]
    SurfaceMessage message;
    message.AddDataByte(0x80 + 1); // start message for specific boardId
    message.AddDataByte('M'); // class: M = Meter
    message.AddDataByte(0); // index
    message.AddDataByte(preamp);
    message.AddDataByte(dynamics);
    message.AddDataByte((uint8_t)meterL);
    message.AddDataByte((uint8_t)(meterL>>8));
    message.AddDataByte((uint8_t)(meterL>>16));
    message.AddDataByte(0x00);
    message.AddDataByte((uint8_t)meterR);
    message.AddDataByte((uint8_t)(meterR>>8));
    message.AddDataByte((uint8_t)(meterR>>16));
    message.AddDataByte(0x00);
    message.AddDataByte((uint8_t)meterSolo);
    message.AddDataByte((uint8_t)(meterSolo>>8));
    message.AddDataByte((uint8_t)(meterSolo>>16));
    message.AddDataByte(0x00);
    surfaceController->SendData(&message, true);
}

// boardId = 0, 1, 4, 5, 8
// index
// ledMode = 0=increment, 1=absolute position, 2=balance l/r, 3=width/spread, 4=decrement, 5=direct -> the number of leds to turn on (max. 13)
// ledPct = percentage 0...100
// backlight = enable or disable
void Surface::SetEncoderRing(uint8_t boardId, uint8_t index, uint8_t ledMode, uint8_t ledPct, bool backlight) {
    // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
    // 0x52, index, leds.w[]
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('R'); // class: R = Ring
    message.AddDataByte(index); // index

    uint16_t leds = 0;
    switch (ledMode) {
        case 0: // standard increment-method
            leds = CalcEncoderRingLedIncrement(ledPct);
            break;
        case 1: // absolute position
            leds = CalcEncoderRingLedPosition(ledPct);
            break;
        case 2: // balance left/right
            leds = CalcEncoderRingLedBalance(ledPct);
            break;
        case 3: // spread/width
            leds = CalcEncoderRingLedWidth(ledPct);
            break;
        case 4: // decrement-method
            leds = CalcEncoderRingLedDecrement(ledPct);
            break;
        case 5: // direct number of leds (max 13)
            leds = CalcEncoderRingLedDirect(ledPct);
            break;
        case 6: // direct position of led (max 13)
            leds = (1U << (ledPct-1));
            break;
    }
    message.AddDataByte(leds & 0xFF);
    if (backlight) {
        message.AddDataByte(((leds & 0x7F00) >> 8) + 0x80); // turn backlight on
    }else{
        message.AddDataByte(((leds & 0x7F00) >> 8)); // turn backlight off
    }
    surfaceController->SendData(&message, true);
}

void Surface::SetLcd(LcdData* p_data, uint textcount)
{
    surfaceController->SetLcd(p_data, textcount);
}

void Surface::FaderReset()
{
    if (surfaceController) {
        surfaceController->FaderReset();
    }
}

void Surface::SetFader(uint8_t boardId, uint8_t index, uint16_t position) {
    if (surfaceController) {
        surfaceController->SetFader(boardId, index, position);
    }
}

void Surface::FaderMoved(uint8_t boardId, uint8_t index, uint16_t value)
{
    if (surfaceController) {
        surfaceController->FaderMoved(boardId, index, value);
    }
}

void Surface::Touchcontrol() {
    if (surfaceController) {
        surfaceController->Touchcontrol();
    }
}

void Surface::SetLed(SurfaceElementId buttonOrLed, bool state, bool blink)
{
    if (surfaceController) {
        surfaceController->SetLed(buttonOrLed, state, blink);
    }
}

}