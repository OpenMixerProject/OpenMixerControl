#pragma once

#include "surfacebindingparameter.h"
#include "mixerparameter.h"

using namespace std;

namespace OMC
{

class X32ChannelStrip
{
    public:

        SurfaceBindingParameter* select;
        SurfaceBindingParameter* vumeter;
        SurfaceBindingParameter* solo;
        SurfaceBindingParameter* lcd;
        SurfaceBindingParameter* mute;
        SurfaceBindingParameter* fader;
};

class X32FaderBank
{
    private:

        String _name;
        OMCBankId _id;

    public:

        vector<X32ChannelStrip*> channelstrip;

        X32FaderBank(OMCBankId Id, String Name, uint ChannelStripSize)
        {
            _id = Id;
            _name = Name;

            for(uint i = 0; i < ChannelStripSize; i++)
            {
                X32ChannelStrip* strip = new X32ChannelStrip();

                strip->select = new SurfaceBindingParameter();
                strip->vumeter = new SurfaceBindingParameter();
                strip->solo = new SurfaceBindingParameter();
		        strip->lcd = new SurfaceBindingParameter();
                strip->mute = new SurfaceBindingParameter();
                strip->fader = new SurfaceBindingParameter();

                channelstrip.push_back(strip);
            }

            Reset();
        }

        OMCBankId GetID()
        {
            return _id;
        }

        String GetName()
        {
            return _name;
        }

        uint GetChannelstripSize()
        {
            return channelstrip.size();
        }


        void Reset()
        {
            for(uint i = 0; i < channelstrip.size(); i++)
            {
                channelstrip[i]->select->FillBindingParameter(MixerparameterAction::NONE, MP_ID::NONE, 0);
                channelstrip[i]->vumeter->FillBindingParameter(MixerparameterAction::NONE, MP_ID::NONE, 0);
                channelstrip[i]->solo->FillBindingParameter(MixerparameterAction::NONE, MP_ID::NONE, 0);
		        channelstrip[i]->lcd->FillBindingParameter(MixerparameterAction::NONE, MP_ID::NONE, 0);
                channelstrip[i]->mute->FillBindingParameter(MixerparameterAction::NONE, MP_ID::NONE, 0);
                channelstrip[i]->fader->FillBindingParameter(MixerparameterAction::NONE, MP_ID::NONE, 0);
            }
        }
};

}