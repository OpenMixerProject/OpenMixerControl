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

#include "mixerparameter.h"

#include "../lib_ext/doctest/doctest/doctest.h"

namespace OMC
{
    String Mixerparameter::FormatValue_Intern(float value_float, uint index, bool isResetLabel)
    {
        using enum MP_UOM;

        if (unitOfMeasurement == PERCENT) {
            value_float *= 100.0f;
        }

        switch (unitOfMeasurement)
        {
            case TAPPOINT:
            case EQ_TYPE:
            case FPGA_ROUTING:
            case DSP_ROUTING:
            case CHANNEL_LCD_MODE:
            case CARD_NUMBER_OF_CHANNELS:
            case CARD_SDCARD:
            case CARD_AUDIO_SOURCE:
                return GetUnitOfMesaurement(false, index, isResetLabel);
            case ZERO_BASED_INDEX__START_BY_ONE:
                return String(value_float + 1, 0);
            case HZ:
                if (value_float >= 1000.0f) {
                    return String(value_float/1000.0f, 2) + " kHz";
                }else{
                    return String(value_float, decimal_places) + " Hz";
                }
            case SECONDS:
                if (value_float >= 60.0f) {
                    return String(value_float/60.0f, 0) + " min " + String(fmod(value_float, 60.0f), 0) + " s";
                }else if (value_float >= 1.0f) {
                    return String(value_float, 0) + " s";
                }else{
                    return String(value_float * 1000.0f, 0) + " ms";
                }
            case PANORAMA:
                if (value_float < 0) {
                    return "L" + String(abs(value_float), decimal_places) + GetUnitOfMesaurement(false, index, isResetLabel);
                }else if (value_float > 0) {
                    return "R" + String(value_float, decimal_places) + GetUnitOfMesaurement(false, index, isResetLabel);
                }else{
                    return "<C>";
                }
            case BANKING_EQ:
                return GetUnitOfMesaurement(false, index, isResetLabel);
                break;
            default:
                return String(value_float, decimal_places) + GetUnitOfMesaurement(false, index, isResetLabel);
        }            
    }

    String Mixerparameter::GetUnitOfMesaurement(bool spaceInFront, uint index, bool isResetLabel)
    {
        String result = spaceInFront ? " " : "";

        switch(unitOfMeasurement)
        {
            using enum MP_UOM;
            
            case DB:
                result += "dB";
                break;
            case DBFS:
                result += "dbFS";
                break;
            case HZ:
                result += "Hz"; // this will automatically be converted to kHz for values >= 1000 in FormatValue_Intern()
                break;
            case PERCENT:
                result += "%";
                break;
            case MS:
                result += "ms";
                break;
            case SECONDS:
                result += "s"; // this will automatically be converted to ms or minutes
                break;
            case BANKING_EQ:
                switch ((uint) (isResetLabel ? value_standard : value[index]))
                {
                    case 0:
                        result += "LOW";
                        break;
                    case 1:
                        result += "LOW MID";
                        break;
                    case 2:
                        result += "HIGH MID";
                        break;
                    case 3:
                        result += "HIGH";
                        break;
                    default:
                        result += "Unknown EQ Bank";
                        break;
                }
                break;
            case EQ_TYPE:
                switch ((uint) (isResetLabel ? value_standard : value[index]))
                {
                    case 0:
                        result += "Allpass";
                        break;
                    case 1:
                        result += "PEQ";
                        break;
                    case 2:
                        result += "LShv";
                        break;
                    case 3:
                        result += "HShv";
                        break;
                    case 4:
                        result += "Bandp";
                        break;
                    case 5:
                        result += "Notch";
                        break;
                    case 6:
                        result += "HCut";
                        break;
                    case 7:
                        result += "LCut";
                        break;
                    default:
                        result += "???";
                        break;
                }
                break;
            case FPGA_ROUTING:
                {
                    uint chan = (uint) (isResetLabel ? value_standard : value[index]);
                    switch (chan)
                    {
                        case FPGA_INPUT_IDX_OFF:
                            result += "OFF";
                            break;
                        case FPGA_INPUT_IDX_XLR ... (FPGA_INPUT_IDX_CARD - 1):
                            result += String("XLR In ") + chan;
                            break;
                        case FPGA_INPUT_IDX_CARD ... (FPGA_INPUT_IDX_AUX - 1):
                            result += String("Card In ") + (chan - FPGA_INPUT_IDX_CARD + 1) ;
                            break;
                        case FPGA_INPUT_IDX_AUX ... (FPGA_INPUT_IDX_TALKBACK_INT - 1):
                            result += String("AUX In ") + (chan - FPGA_INPUT_IDX_AUX + 1);
                            break;
                        case FPGA_INPUT_IDX_TALKBACK_INT:
                            result += String("TB Int");
                            break;
                        case FPGA_INPUT_IDX_TALKBACK_EXT:
                            result += String("TB Ext");
                            break;
                        case FPGA_INPUT_IDX_DSP_RETURN ... (FPGA_INPUT_IDX_AES50A - 8 - 1):
                            result += String("DSP Out ") + (chan - FPGA_INPUT_IDX_DSP_RETURN + 1);
                            break;
                        case FPGA_INPUT_IDX_DSP_RETURN + 32 ... (FPGA_INPUT_IDX_AES50A - 1):
                            result += String("DSP AuxOut ") + (chan - (FPGA_INPUT_IDX_DSP_RETURN + 32) + 1);
                            break;
                        case FPGA_INPUT_IDX_AES50A ... (FPGA_INPUT_IDX_AES50B - 1):
                            result += String("AES50A In ") + (chan - FPGA_INPUT_IDX_AES50A + 1);
                            break;
                        case FPGA_INPUT_IDX_AES50B ... (FPGA_INPUT_IDX_AES50B + 48 - 1):
                            result += String("AES50B In ") + (chan - FPGA_INPUT_IDX_AES50B + 1);
                            break;
                        default:
                            result += "???";
                    }
                }
                break;
            case DSP_ROUTING:
                {
                    uint chan = (uint) (isResetLabel ? value_standard : value[index]);
                    switch (chan)
                    {
                        case DSP_BUF_IDX_OFF:
                            result += "OFF";
                            break;
                        case DSP_BUF_IDX_DSPCHANNEL ... (DSP_BUF_IDX_AUX - 1):
                            result += String("FPGA -> DSP In ") + chan;
                            break;
                        case DSP_BUF_IDX_AUX ... (DSP_BUF_IDX_DSP2_FXRET - 1):
                            result += String("FPGA -> DSP In ") + chan;
                            break;
                        case DSP_BUF_IDX_DSP2_FXRET ... (DSP_BUF_IDX_MIXBUS - 1):
                            result += String("DSP2 -> FX Return ") + (chan - DSP_BUF_IDX_DSP2_FXRET + 1);
                            break;
                        case DSP_BUF_IDX_MIXBUS ... (DSP_BUF_IDX_DSP2_FXINS - 1):
                            result += String("Mixbus ") + (chan - DSP_BUF_IDX_MIXBUS + 1);
                            break;
                        case DSP_BUF_IDX_DSP2_FXINS ... (DSP_BUF_IDX_MAINLEFT - 1):
                            result += String("DSP2 -> FX Insert ") + (chan - DSP_BUF_IDX_DSP2_FXINS + 1);
                            break;
                        case DSP_BUF_IDX_MAINLEFT:
                            result += String("Main L");
                            break;
                        case DSP_BUF_IDX_MAINRIGHT:
                            result += String("Main R");
                            break;
                        case DSP_BUF_IDX_MAINSUB:
                            result += String("Sub");
                            break;
                        case DSP_BUF_IDX_MATRIX ... (DSP_BUF_IDX_DSP2_AUX - 1):
                            result += String("Matrix ") + (chan - DSP_BUF_IDX_MATRIX + 1);
                            break;
                        case DSP_BUF_IDX_DSP2_AUX:
                            result += String("Linux Audio L");
                            break;
                        case DSP_BUF_IDX_DSP2_AUX + 1:
                            result += String("Linux Audio R");
                            break;
                        case DSP_BUF_IDX_DSP2_AUX + 2:
                            result += String("Surround Center");
                            break;
                        case DSP_BUF_IDX_DSP2_AUX + 3:
                            result += String("Surround BackLeft");
                            break;
                        case DSP_BUF_IDX_DSP2_AUX + 4:
                            result += String("Surround BackRight");
                            break;
                        case DSP_BUF_IDX_DSP2_AUX + 5:
                            result += String("Surround LFE");
                            break;
                        case DSP_BUF_IDX_DSP2_AUX + 6:
                            result += String("Oscillator 1");
                            break;
                        case DSP_BUF_IDX_DSP2_AUX + 7:
                            result += String("Oscillator 2");
                            break;
                        case DSP_BUF_IDX_MONLEFT:
                            result += String("Monitor L");
                            break;
                        case DSP_BUF_IDX_MONRIGHT:
                            result += String("Monitor R");
                            break;
                        case DSP_BUF_IDX_TALKBACK:
                            result += String("Talkback");
                            break;
                        default:
                            result += "???";
                    }
                }
                break;
            case TAPPOINT:
                switch ((DSP_TAP) (isResetLabel ? value_standard : value[index]))
                {
                    case DSP_TAP::INPUT:
                        result += "Input";
                        break;
                    case DSP_TAP::POST_EQ:
                        result += "Post EQ";
                        break;
                    case DSP_TAP::POST_FADER:
                        result += "Post Fader";
                        break;
                    case DSP_TAP::PRE_EQ:
                        result += "Pre EQ";
                        break;
                    case DSP_TAP::PRE_FADER:
                        result += "Pre Fader";
                        break;
                }
                break;
            case CHANNEL_LCD_MODE:
                switch((uint) (isResetLabel ? value_standard : value[index]))
                {
                    case 0:
                        result += "Lite";
                        break;
                    case 1:
                        result += "Detail";
                        break;
                }
                break;
            case CARD_NUMBER_OF_CHANNELS:
                switch((uint) (isResetLabel ? value_standard : value[index]))
                {
                    case CARD_CHANNELMODE_32IN_32OUT:
                        result += String("32/32");
                        break;
                    case CARD_CHANNELMODE_16IN_16OUT:
                        result += String("16/16");
                        break;
                    case CARD_CHANNELMODE_32IN_8OUT:
                        result += String("32/8");
                        break;
                    case CARD_CHANNELMODE_8IN_32OUT:
                        result += String("8/32");
                        break;
                    case CARD_CHANNELMODE_8IN_8OUT:
                        result += String("8/8");
                        break;
                    case CARD_CHANNELMODE_2IN_2OUT:
                        result += String("2/2");
                        break;
                }
                break;
            case CARD_SDCARD:
                switch((uint) (isResetLabel ? value_standard : value[index]))
                {
                    case 0:
                        result += String("#1");
                        break;
                    case 1:
                        result += String("#2");
                        break;
                }
                break;
            case CARD_AUDIO_SOURCE:
                switch((uint) (isResetLabel ? value_standard : value[index]))
                {
                    case 0:
                        result += String("USB");
                        break;
                    case 1:
                        result += String("CARD");
                        break;
                }
                break;
            default:
                result += "";
        }	

        return result;
    }



    // ######## ########  ######  ########  ######  
    //    ##    ##       ##    ##    ##    ##    ## 
    //    ##    ##       ##          ##    ##       
    //    ##    ######    ######     ##     ######  
    //    ##    ##             ##    ##          ## 
    //    ##    ##       ##    ##    ##    ##    ## 
    //    ##    ########  ######     ##     ######  

    TEST_CASE("Mixerparameter with 1 instance")
    {
        Mixerparameter* p = new Mixerparameter(MP_ID::ACTIVE_PAGE, MP_CAT::GLOBAL, "Active Page", 1);
        p->DefMinMaxStandard_Uint(0, 20, 1);

        SUBCASE("GetInt() non exiting instance")
        {
            CHECK(p->GetInt(25) == 0);
        }

        SUBCASE("GetUint() non exiting instance")
        {
            CHECK(p->GetUint(25) == 0);
        }

        SUBCASE("Set on non exiting instance")
        {
            CHECK_THROWS(p->Set((uint)200, 5));
        }

        SUBCASE("GetUint")
        {
            REQUIRE_NOTHROW(p->Set(10));
            CHECK(p->GetUint() == 10);
        }
    }

    TEST_CASE("Mixerparameter - float value check")
    {
        Mixerparameter* p = new Mixerparameter(MP_ID::CHANNEL_VOLUME, MP_CAT::CHANNEL, "Ch Vol", 24);
        p->DefMinMaxStandard_Float(0, 100, 60);
        p->DefStepsize(0.25);

        SUBCASE("Standardvalue is set after init")
        {
            CHECK(p->GetFloat() == 60.0f);
        }

        SUBCASE("Standardvalue is set after reset")
        {
            p->Set(120.0f);
            p->Reset();
            CHECK(p->GetFloat() == 60.0f);
        }

        SUBCASE("value is set")
        {
            p->Set(90.0f);
            CHECK(p->GetFloat() == 90.0f);
        }

        SUBCASE("value set to > max")
        {
            p->Set(120.0f);
            CHECK(p->GetFloat() == 100.0f);
        }

        SUBCASE("value set to < min")
        {
            p->Set(-10.0f);
            CHECK(p->GetFloat() == 0.0f);
        }

        SUBCASE("one stepsize up")
        {
            p->Set(10.0f);
            p->Change(1);
            CHECK(p->GetFloat() == 10.25f);
        }

        SUBCASE("one stepsize down")
        {
            p->Set(10.0f);
            p->Change(-1);
            CHECK(p->GetFloat() == 9.75f);
        }

        SUBCASE("readonly")
        {
            p->DefReadonly();

            SUBCASE("IsReadonly()")
            {
                CHECK(p->IsReadonly());
            }
            
            SUBCASE("increase value")
            {
                CHECK_THROWS(p->Change(1));
            }

            SUBCASE("decrease value")
            {
                CHECK_THROWS(p->Change(-1));
            }

            SUBCASE("set value")
            {
                CHECK_THROWS(p->Set(0.5f));
            }
        }
    }

    TEST_CASE("Mixerparameter")
    {
        Mixerparameter* p = new Mixerparameter(MP_ID::CHANNEL_VOLUME, MP_CAT::CHANNEL, "Ch Vol", 24);
        p->DefMinMaxStandard_Float(0, 100, 60);
    
        CHECK(p->GetId() == MP_ID::CHANNEL_VOLUME);
        CHECK(p->GetCategory() == MP_CAT::CHANNEL);
        CHECK(p->BelongsToChannel());
        CHECK_FALSE(p->BelongsToFX());
        CHECK(p->GetName() == "Ch Vol");

        SUBCASE("NoConfig")
        {
            p->DefNoConfig();

            bool result = p->IsNoConfig();
            CHECK(result == true);
        }

        SUBCASE("GetName")
        {
            CHECK(p->GetName() == "Ch Vol");
        }
    }

    TEST_CASE("Mixerparameter - Set and Get uint8_t (like Meterdata)")
    {
        // create Mixerparaemter
        Mixerparameter* p = new Mixerparameter(MP_ID::CHANNEL_METER_DECAYED_POST_GAIN, MP_CAT::CHANNEL, "MeterTest", MAX_VCHANNELS);
        p->DefMinMaxStandard_Uint(0, 0b11111111, 0)
        ->DefSilent();

        // Set Mixerparameter and Read back
        uint8_t metervalue = GENERATE(0, 0b11111111);
        p->Set(metervalue);
        uint8_t metervalue_received = (uint8_t)p->GetUint();

        // check that values are the same
        CHECK(metervalue == metervalue_received);
    }
}