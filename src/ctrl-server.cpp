#include "ctrl-server.h"

#include "version.h"

#include "external.h" // all external includes

#include "x32config.h"
#include "helper.h"
#include "state.h"
#include "osc-server.h"
#include "wsm.h"

CtrlServer::CtrlServer(X32BaseParameter* basepar) : X32Base(basepar)
{
	mixer = new Mixer(basepar);
	osc_server = new OscServer(basepar);
	wsm = new WSM(basepar);
}

// ###########################################################################
// #
// #      #### ##    ## #### ######## 
// #       ##  ###   ##  ##     ##    
// #       ##  ####  ##  ##     ##    
// #       ##  ## ## ##  ##     ##    
// #       ##  ##  ####  ##     ##    
// #       ##  ##   ###  ##     ##    
// #      #### ##    ## ####    ## 
// #
// ###########################################################################

void CtrlServer::Init()
{
	//##################################################################################
	//#
	//# 	Initialize system
	//#
	//##################################################################################

	helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "mixer->Init()");
	mixer->Init();

	helper->DEBUG_X32CTRL(DEBUGLEVEL_VERBOSE, "ocs_server->Init()");
	osc_server->Init();

	helper->DEBUG_X32CTRL(DEBUGLEVEL_VERBOSE, "wsm->Init()");
	wsm->Init();


	//############################################################################

	// Just load a default set of FXes
	// TODO: Save and Load
	/*
	// available FX types:
	========================
	NONE = -1,
	REVERB = 0,
	CHORUS = 1,
	TRANSIENTSHAPER = 2,
	OVERDRIVE = 3,
	DELAY = 4,
	MULTIBANDCOMPRESOR = 5,
	DYNAMICEQ = 6
	*/

	if (config->IsModelAnyXM32())
	{
		mixer->dsp->DSP2_SetFx(0, FX_TYPE::REVERB, 2); // this effect takes lot of DSP-ressources
		mixer->dsp->DSP2_SetFx(1, FX_TYPE::CHORUS, 2);
		mixer->dsp->DSP2_SetFx(2, FX_TYPE::DELAY, 2);
		mixer->dsp->DSP2_SetFx(3, FX_TYPE::NONE, 2);
		mixer->dsp->DSP2_SetFx(4, FX_TYPE::NONE, 2);
		mixer->dsp->DSP2_SetFx(5, FX_TYPE::NONE, 2);
		mixer->dsp->DSP2_SetFx(6, FX_TYPE::NONE, 2);
		mixer->dsp->DSP2_SetFx(7, FX_TYPE::NONE, 2);
	}
	else 

	//############################################################################

	// X32Config
	if(!config->LoadConfig(0))
	{
		// create new ini file
		helper->DEBUG_INI(DEBUGLEVEL_NORMAL, "no default configfile found, creating one");
		
		config->Save(0);
	}
}

//#####################################################################################################################
//
// ######## #### ##     ## ######## ########  
//    ##     ##  ###   ### ##       ##     ## 
//    ##     ##  #### #### ##       ##     ## 
//    ##     ##  ## ### ## ######   ########  
//    ##     ##  ##     ## ##       ##   ##   
//    ##     ##  ##     ## ##       ##    ##  
//    ##    #### ##     ## ######## ##     ## 
//
//#####################################################################################################################

void CtrlServer::Tick10ms(void)
{
	helper->DEBUG_TIMER(DEBUGLEVEL_TRACE, "10ms");

	//#####################################
	//
	//   Freeze changed parameter list
	//
	config->FreezeParameterList();
	//
	//#####################################

	// this stateMachine handles the read and write to and from the two DSPs
	mixer->dsp->CallbackStateMachine();

	// read incoming data from adda-boards and expansion-card
	ProcessUartDataAdda();

	// read incoming data from AES50 devices
	ProcessUartDataAES50();

	// communication with XRemote-clients via UDP (X32-Edit, MixingStation, etc.)
	osc_server->UdpHandleCommunication();

	// communication with Sennheiser Media Control Protocol
	UdpHandleCommunication_WSM();


	// sync if any Mixerparameter has changed
	if (config->HasAnyParameterChanged())
	{
		helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "mixer->Sync()");
		mixer->Sync();

		helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "mixer->card->Sync()");
		mixer->card->Sync();

		//syncXRemote(false);
	}


	//#####################################
	//
	//   Unfreeze changed parameter list
	//
	
	config->SaveResetAndUnfreezeChangedParameterList();
	//
	//#####################################
}

void CtrlServer::Tick100ms(void)
{
	static float dspLoadHistory[2][20];
	static uint8_t dspLoadHistoryPointer = 0;
	static uint8_t startupCounter = 0;

	helper->DEBUG_TIMER(DEBUGLEVEL_TRACE, "100ms");

	// request data from all known clients
	wsm->RequestDataFromClients();

	// DSP-Activity Light
    if (!(state->dsp_disable_activity_light)) {
   	    // toggle the LED on DSP1 and DSP2 to show some activity
        uint32_t value = 2;
		mixer->dsp->spi->QueueDspData(0, 'a', 42, 0, 1, (float*)&value);
        mixer->dsp->spi->QueueDspData(1, 'a', 42, 0, 1, (float*)&value);
    }

	// TODO: send DSP-Load to client
	// // DEBUG Row in GUI-Header
	// if (config->GetBool(DEBUG_HEADER) && config->HasDisplay())
	// {
	// 	// calculate mean-value and show the current DSP-load
	// 	dspLoadHistory[0][dspLoadHistoryPointer] = state->dspLoad[0];
	// 	dspLoadHistory[1][dspLoadHistoryPointer] = state->dspLoad[1];
	// 	dspLoadHistoryPointer++;
	// 	if (dspLoadHistoryPointer >= 20) {
	// 		dspLoadHistoryPointer = 0;
	// 	}

	// 	float dspLoadMean[2] = {0, 0};
	// 	for (uint8_t i = 0; i < 20; i++) {
	// 		dspLoadMean[0] += dspLoadHistory[0][i];
	// 		dspLoadMean[1] += dspLoadHistory[1][i];
	// 	}
	// 	dspLoadMean[0] /= 20.0f;
	// 	dspLoadMean[1] /= 20.0f;

	// 	// show DSP debug infos
	// 	lv_label_set_text_fmt(objects.header_statustext, "DSP1 L: %.1f %% V: v%.2f G: %.0f TxQ: %d DSP2 L: %.1f %% V: v%.2f G: %.0f H: %.0f free TxQ: %d", 
	// 		(double)dspLoadMean[0], (double)state->dspVersion[0], (double)state->dspAudioGlitchCounter[0], mixer->dsp->spi->GetDspTxQueueLength(0),
	// 		(double)dspLoadMean[1], (double)state->dspVersion[1], (double)state->dspAudioGlitchCounter[1], (double)state->dspFreeHeapWords[1], mixer->dsp->spi->GetDspTxQueueLength(1)
	// 	);
	// }

	// send AES50-data to FPGA
	// DeviceTypeAndProperty every 2 seconds, Headamp-Message every 2 seconds (Names every 10 seconds)
	mixer->fpga->AES50Tick();

	if (startupCounter < 100)
	{
		startupCounter++;

		if (startupCounter == 10)
		{
			// the gate, the dynamics and the EQ-settings are not loaded correctly on first load, so load it again after a short time
			config->LoadConfig(0);

			// in the following lines the default configuration is set so that the users of the beta-version
			// can start with a working system

			// route channel 1-4 to effects using post-fader tapping
			for (uint8_t i = 0; i < 8; i++)
			{
				config->Set(ROUTING_DSP_OUTPUT, DSP_BUF_IDX_DSPCHANNEL + (i / 2), 40 + i);
				config->Set(ROUTING_DSP_OUTPUT_TAPPOINT, to_underlying(DSP_TAP::POST_FADER), 40 + i);
			}

			// set AUX7/8 to MONITOR L/R
			config->Set(ROUTING_DSP_OUTPUT, DSP_BUF_IDX_MONLEFT, 38);
			config->Set(ROUTING_DSP_OUTPUT, DSP_BUF_IDX_MONRIGHT, 39);
			config->Set(ROUTING_DSP_OUTPUT_TAPPOINT, to_underlying(DSP_TAP::POST_FADER), 38);
			config->Set(ROUTING_DSP_OUTPUT_TAPPOINT, to_underlying(DSP_TAP::POST_FADER), 39);

			// set volume of FX-return to 0dBfs
			for (int i = 0; i < 8; i++)
			{
				config->Set(CHANNEL_VOLUME, 0, 40 + i);
			}

			// set default FXes in FX slots
			mixer->dsp->DSP2_SetFx(0, FX_TYPE::REVERB, 2); // on first load this effect has a bug, so we have to disable it a bit later
            mixer->dsp->DSP2_SetFx(1, FX_TYPE::CHORUS, 2);
            mixer->dsp->DSP2_SetFx(2, FX_TYPE::DELAY, 2);
            mixer->dsp->DSP2_SetFx(3, FX_TYPE::NONE, 2);
            mixer->dsp->DSP2_SetFx(4, FX_TYPE::NONE, 2);
            mixer->dsp->DSP2_SetFx(5, FX_TYPE::NONE, 2);
            mixer->dsp->DSP2_SetFx(6, FX_TYPE::NONE, 2);
            mixer->dsp->DSP2_SetFx(7, FX_TYPE::NONE, 2);

			// set FX-settings to wet on slot 1-4
			config->Set(FX_REVERB_DRY, 0, 0); // fx-slot 1
			config->Set(FX_REVERB_WET, 1, 0); // fx-slot 1
			config->Set(FX_CHORUS_MIX, 1, 1); // fx-slot 2		
		}

		if (startupCounter == 40) {
			// disable effect as on first start of the effect some parts in
			// the external memory gets corrupted. This needs more debugging
			// for now stop-restart is fine
			mixer->dsp->DSP2_SetFx(0, FX_TYPE::NONE, 2);
			mixer->dsp->DSP2_SetFx(2, FX_TYPE::NONE, 2);
		}

		if (startupCounter == 50) {
			// renable effect
			mixer->dsp->DSP2_SetFx(0, FX_TYPE::REVERB, 2);
			mixer->dsp->DSP2_SetFx(2, FX_TYPE::DELAY, 2);
		}

		if (startupCounter == 60) {
			// unmute ADDA-boards
			mixer->adda->SetMuteAll(false);
		}

		if (startupCounter == 99)
		{
			intialized = true;
		}
	}

	AutoSave();
}

void CtrlServer::AutoSave()
{
	if (intialized)
	{
		if (autosavewait == 0)
		{
			helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "Autosave to Scene 0");

			// TODO: do we need an Autosave indication?
			// lv_label_set_text_fmt(objects.header_statustext, "Autosave in progress...");
			// lv_refr_now(NULL);

			config->Save(0);
			autosavewait = 60 * 10; // 60 Seconds -> Autosave every minute
		}

		autosavewait--;
	}
}

void CtrlServer::ProcessUartDataAdda() {
	// read incoming data from adda-boards and expansion-card
	String newCommand = mixer->adda->Receive();
	
	if (newCommand.length() > 0) {
		helper->DEBUG_ADDA(DEBUGLEVEL_TRACE, "Received ADDA command: %s", newCommand.c_str());

		if (newCommand.indexOf("*9") > -1) {
			// we are receiving an answer from the expansion-card

			// only add to debug-text when not sample-index-update
			if (newCommand.indexOf("*9N22") == -1) {
				mixer->debugText += mixer->debugText + "\n" + newCommand;
				if (mixer->debugText.length() > 1000) {
					mixer->debugText = "";
				}
				helper->DEBUG_ADDA(DEBUGLEVEL_TRACE, mixer->debugText.c_str());
			}

			mixer->card->ProcessCommand(newCommand);
		}
	}
}

void CtrlServer::ProcessUartDataAES50() {
	mixer->fpga->AES50Receive();
}

//#####################################################################################################################
//#####################################################################################################################
//
// 			E V E N T S
//
//#####################################################################################################################
//#####################################################################################################################


// receive data from WSM client
void CtrlServer::UdpHandleCommunication_WSM()
{
    char rxData[500];
    int bytes_available = 0;
    struct sockaddr_in ClientAddr;
    
    // check for bytes in UDP-buffer
    // int result = ioctl(wsm->UdpHandle, FIONREAD, &bytes_available);
    if (bytes_available > 0) {
        //socklen_t wsmClientAddrLen = sizeof(ClientAddr);
        //uint8_t len = recvfrom(wsm->UdpHandle, rxData, bytes_available, MSG_WAITALL, (struct sockaddr *) &ClientAddr, &wsmClientAddrLen);

		String clientIp = inet_ntoa(ClientAddr.sin_addr);
		String message = String(rxData);
		message.replace("\r", "\r\n");

		helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "Sennheiser Media Control Protocoll (%s): %s", clientIp.c_str(), message.c_str());
	}
}




// sync mixer state to GUI
void CtrlServer::syncXRemote(bool syncAll) {
	// //bool fullSync = false;

	// if (syncAll || config->HasParameterChanged(SELECTED_CHANNEL)){ 
	// 	// channel selection has changed - do a full sync
	// 	//fullSync=true; 
	// }
	
	// // DEBUG
	// xremote->SetCard(10); // X-LIVE

	// for(uint8_t i=0; i<(uint)X32_VCHANNELTYPE::NORMAL; i++) {
	// 	//uint8_t chanindex = i;
	// 	//VChannel* chan = mixer->GetVChannel(i);
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_VOLUME)){
	// 	// 	xremote->SetFader(String("ch"), chanindex, mixer->GetVolumeOscvalue(chanindex));
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_BALANCE)){
	// 	// 	xremote->SetPan(chanindex, mixer->vchannel[chanindex]->dspChannel->balance);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_MUTE)){
	// 	// 	xremote->SetMute(chanindex, mixer->vchannel[chanindex]->dspChannel->muted);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_SOLO)){
	// 	// 	xremote->SetSolo(chanindex, mixer->vchannel[chanindex]->dspChannel->solo);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_COLOR)){
	// 	// 	xremote->SetColor(chanindex, mixer->vchannel[chanindex]->color);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_NAME)){
	// 	// 	xremote->SetName(chanindex, mixer->vchannel[chanindex]->name);
	// 	// }
	// }

	// for(uint8_t i=(uint)X32_VCHANNEL_BLOCK::AUX; i<(uint)X32_VCHANNELTYPE::AUX; i++) {
	// 	//uint8_t chanindex = i;
	// 	//VChannel* chan = mixer->GetVChannel(i);
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_VOLUME)){
	// 	// 	xremote->SetFader(String("auxin"), chanindex, mixer->GetVolumeOscvalue(chanindex));
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_BALANCE)){
	// 	// 	xremote->SetPan(chanindex, mixer->vchannel[chanindex]->dspChannel->balance);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_MUTE)){
	// 	// 	xremote->SetMute(chanindex, mixer->vchannel[chanindex]->dspChannel->muted);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_SOLO)){
	// 	// 	xremote->SetSolo(chanindex, mixer->vchannel[chanindex]->dspChannel->solo);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_COLOR)){
	// 	// 	xremote->SetColor(chanindex, mixer->vchannel[chanindex]->color);
	// 	// }
	// 	// if (fullSync || chan->HasChanged(X32_VCHANNEL_CHANGED_NAME)){
	// 	// 	xremote->SetName(chanindex, mixer->vchannel[chanindex]->name);
	// 	// }
	// }
}


