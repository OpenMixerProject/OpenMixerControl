#include "ctrl-client.h"

#include "main.h"
#include "version.h"
#include "config.h"
#include "helper.h"
#include "state.h"

#include "page-meters.h"
#include "page-rta.h"
#include "page-home.h"
#include "page-config.h"
#include "page-gate.h"
#include "page-dynamics.h"
#include "page-eq.h"
#include "page-sends.h"
#include "page-main.h"
#include "page-routing.h"
#include "page-routing-fpga.h"
#include "page-routing-channels.h"
#include "page-routing-dsp.h"
#include "page-library.h"
#include "page-effects.h"
#include "page-setup.h"
#include "page-setup-card.h"
#include "page-setup-surface.h"
#include "page-debug.h"
#include "page-about.h"
#include "page-scenes.h"
#include "page-prototypegui.h"
#include "lcd-menu.h"

#include "surfaceelement.h"
#include "surfacebindingparameter.h"
#include "surface-controller.h"

#include "eez/ui.h"

#include "artnet.h"

namespace OMC
{

CtrlClient::CtrlClient(X32BaseParameter* basepar) : X32Base(basepar)
{
    surface = new Surface(basepar);
    lcdmenu = new LcdMenu(basepar, surface); // only used for X32Core
	osc_client = new OscClient(basepar);
	artnet = new Artnet(basepar);
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

void CtrlClient::Init()
{
	helper->Log("##############\n");
	helper->Log("### Client ###\n");
	helper->Log("##############\n");

	if (config->IsClientMode())
	{
		osc_client->Init();

		config->SetCallbackSet(OnOscSendToServerCallbackSet, this);
		config->SetCallbackChange(OnOscSendToServerCallbackChange, this);
		config->SetCallbackToogle(OnOscSendToServerCallbackToogle, this);
		config->SetCallbackReset(OnOscSendToServerCallbackReset, this);
	}

	helper->Log("Init Artnet\n");
    artnet->Init();

	helper->Log("Init Surface\n");
    surface->Init(OnSurfaceCallback, this);

    if (config->IsModelX32Core() || config->IsModelM32C())
	{
		// only necessary if LVGL is not used
		helper->Log("Starting Timers...\n");
		init10msTimer_NonGUI();

		// sync the Surface
		#ifdef BUILD_DEBUG
		helper->Log("Sync Surface\n");
		#endif
		syncSurface(true);

		if (config->IsModelX32Core() || config->IsModelM32C())
        {
            helper->Log("Init LCD Menu\n");
            lcdmenu->OnInit();
        }

		helper->Log("Press Ctrl+C to terminate program.\n");
		while (1) {
			sleep(10); // Basically sleep forever :-) Timers do their job
		}
	}
	else
	{
		helper->Log("Init LVGL\n");
		lv_init();

		#ifdef TARGET_PC_SDL2

			helper->Log("bodyless mode (Development Simulator) startet");
			state->bodyless = true;
			
			display = lv_sdl_window_create(DISPLAY_RESOLUTION_X, DISPLAY_RESOLUTION_Y);		
			lv_sdl_window_set_title(display, "OpenX32 - omc - Development Simulator");
			keyboard = lv_sdl_keyboard_create();
			//mouse = lv_sdl_mouse_create();
			//mouse_wheel = lv_sdl_mousewheel_create();

			// call this before "ui_init()"
			ui_create_groups();

			// set group for your input device
			lv_group_set_default(groups.grp_KEY);
			lv_indev_set_group(keyboard, groups.grp_KEY);
		
		#else
			
			helper->Log("Init FBDEV\n");
			const char * device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
			display = lv_linux_fbdev_create();

			if(display == NULL) {
				helper->Log("could not create display!");
				return;
			}

			lv_linux_fbdev_set_file(display, device);	

		#endif

		#ifdef BUILD_DEBUG
		helper->Log("Init Timer 10ms\n");
		#endif
		lv_timer_create(timer10msCallbackLvgl, 10, NULL);

		#ifdef BUILD_DEBUG
		helper->Log("Init Timer 50ms\n");
		#endif
		lv_timer_create(timer50msCallbackLvgl, 50, NULL);

		#ifdef BUILD_DEBUG
		helper->Log("Init Timer 100ms\n");
		#endif
		lv_timer_create(timer100msCallbackLvgl, 100, NULL);

		#ifdef BUILD_DEBUG
		helper->Log("Init Timer 1000ms\n");
		#endif
		lv_timer_create(timer1000msCallbackLvgl, 1000, NULL);

		// initialize GUI created by EEZ
		#ifdef BUILD_DEBUG
		helper->Log("Init EEZ GUI\n");
		#endif
		ui_init();

		// InitPagesAndGUI() has to be called after ui_init()!
		#ifdef BUILD_DEBUG
		helper->Log("InitPagesAndGUI()\n");
		#endif
		InitPagesAndGUI();

		// trigger first update of display header
		#ifdef BUILD_DEBUG
		helper->Log("Refresh Selected Channel\n");
		#endif
		config->Refresh(SELECTED_CHANNEL);

		// // set IP-Address in GUI
		// helper->Log("Show IP\n");
		// String ip = helper->getIpAddress();
		// lv_label_set_text_fmt(objects.header_ip, "IP: %s", ip.c_str());

		// sync the Page
		#ifdef BUILD_DEBUG
		helper->Log("Sync Active Page\n");
		#endif
		config->Refresh(ACTIVE_PAGE);

		// sync the Surface
		#ifdef BUILD_DEBUG
		helper->Log("Sync Surface\n");
		#endif
		syncSurface(true);

		// LVGL loop
		#ifdef BUILD_DEBUG
		helper->Log("Start LVGL Loop\n");
		#endif
		uint32_t idle_time;
		while (1)
		{
			idle_time = lv_timer_handler();
			usleep(idle_time * 1000);
		}
	}
}

const char * CtrlClient::getenv_default(const char * name, const char * default_val)
{
    const char * value = getenv(name);
    return value ? value : default_val;
}

void CtrlClient::Tick10ms()
{
	if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32R() ||
		config->IsModelAnyWing()) 
	{
		surface->Touchcontrol();	
	}

    // read incoming data from surface and handle it
	surface->ProcessUartDataSurface();
	surface->Tick10ms();

    // sync if any Mixerparameter has changed
	if (config->HasAnyParameterChanged())
	{
		helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "artnet->Sync()");
		artnet->Sync();

		syncGuiOrLcd();
    }

    if (config->HasAnyParameterChanged() || config->HasAnySurfaceBindingChanged())
	{
		// sync GUI(s) last, to get visual response after the hardware is synced!
		syncSurface(false);
	}

	// communication with OSC-Servers via UDP
	osc_client->UdpHandleCommunication();
}

void CtrlClient::Tick50ms()
{
	helper->DEBUG_TIMER(DEBUGLEVEL_TRACE, "50ms");

	// Update VU-Meters
	UpdateMeters();

	// update Dimmerkernel
	artnet->Tick();
}

void CtrlClient::Tick100ms()
{
    surface->Tick100ms();

    // Refresh IP address display periodically (every 5s @ 100ms tick = 50 ticks)
	// Might be worth updating upon a Linux Networking hook but this was simplest.
	// Combine with the GUI Init and extract into separate function?
    static uint_fast8_t ipRefreshCounter = 0; //only counting to 50 so 16bit not needed, let compiler pick
    ipRefreshCounter++;
    if (ipRefreshCounter >= 50)
    {
        ipRefreshCounter = 0;
        String currentIpAddress = helper->getIpAddress();

		// only set ip address if it has changed
		if (ipAddress != currentIpAddress)
		{
			ipAddress = currentIpAddress;

			if (config->HasDisplay())
			{
			}
		}
    }

	// DEBUG Row in GUI-Header
	static float dspLoadHistory[2][20];
	static uint8_t dspLoadHistoryPointer = 0;
	if (config->GetBool(DEBUG_HEADER) && config->HasDisplay())
	{
		// calculate mean-value and show the current DSP-load
		dspLoadHistory[0][dspLoadHistoryPointer] = state->dspLoad[0];
		dspLoadHistory[1][dspLoadHistoryPointer] = state->dspLoad[1];
		dspLoadHistoryPointer++;
		if (dspLoadHistoryPointer >= 20) {
			dspLoadHistoryPointer = 0;
		}

		float dspLoadMean[2] = {0, 0};
		for (uint8_t i = 0; i < 20; i++) {
			dspLoadMean[0] += dspLoadHistory[0][i];
			dspLoadMean[1] += dspLoadHistory[1][i];
		}
		dspLoadMean[0] /= 20.0f;
		dspLoadMean[1] /= 20.0f;

		// show DSP debug infos
		lv_label_set_text_fmt(objects.header_statustext, "DSP1 L: %.0f V: v%.2f G: %.0f DSP2 L: %.1f %% V: v%.2f G: %.0f H: %.0f free", 
			(double)dspLoadMean[0], (double)state->dspVersion[0], (double)state->dspAudioGlitchCounter[0], /*mixer->dsp->spi->GetDspTxQueueLength(0),*/
			(double)dspLoadMean[1], (double)state->dspVersion[1], (double)state->dspAudioGlitchCounter[1], (double)state->dspFreeHeapWords[1]/*, mixer->dsp->spi->GetDspTxQueueLength(1)*/
		);
	}
}

void CtrlClient::Tick1000ms()
{
	if (config->HasDisplay())
	{
		// Display Time
		timestamp = time(NULL);
		datetime = *localtime(&timestamp);
		strftime(time_str, 50, "%H:%M:%S", &datetime);
		lv_label_set_text(objects.header_time, time_str);
	}
}

//#####################################################################################################################
//
// ########     ###     ######   ########  ######  
// ##     ##   ## ##   ##    ##  ##       ##    ## 
// ##     ##  ##   ##  ##        ##       ##       
// ########  ##     ## ##   #### ######    ######  
// ##        ######### ##    ##  ##             ## 
// ##        ##     ## ##    ##  ##       ##    ## 
// ##        ##     ##  ######   ########  ######  
//
//#####################################################################################################################

void CtrlClient::InitPagesAndGUI()
{
	// Show OMC Version and builddate in GUI Header -> OMC vX.X.X build on 01.01.1999 
	lv_label_set_text_fmt(objects.header_omc_version, "OMC %s build on %s at %s", GIT_VERSION, __DATE__, __TIME__);

	PageBaseParameter* pagebasepar = new PageBaseParameter(app, config, state, helper, surface);
	
	pages[X32_PAGE::HOME] = new PageHome(pagebasepar);
	pages[X32_PAGE::CONFIG] = new PageConfig(pagebasepar);
	pages[X32_PAGE::GATE] = new PageGate(pagebasepar);
	pages[X32_PAGE::COMPRESSOR] = new PageDynamics(pagebasepar);
	pages[X32_PAGE::EQ] = new PageEq(pagebasepar);
	pages[X32_PAGE::SENDS] = new PageSends(pagebasepar);
	pages[X32_PAGE::MAIN] = new PageMain(pagebasepar);
	pages[X32_PAGE::METERS] = new PageMeters(pagebasepar);
	pages[X32_PAGE::RTA] = new PageRta(pagebasepar);
	pages[X32_PAGE::ROUTING] = new PageRouting(pagebasepar);
	pages[X32_PAGE::ROUTING_FPGA] = new PageRoutingFpga(pagebasepar);
	pages[X32_PAGE::ROUTING_DSP1] = new PageRoutingChannels(pagebasepar);
	pages[X32_PAGE::ROUTING_DSP2] = new PageRoutingDsp(pagebasepar);
	pages[X32_PAGE::SETUP] = new PageSetup(pagebasepar);
	pages[X32_PAGE::SETUP_CARD] = new PageSetupCard(pagebasepar);
	pages[X32_PAGE::SETUP_SURFACE] = new PageSetupSurface(pagebasepar);
	pages[X32_PAGE::ABOUT] = new PageAbout(pagebasepar);
	pages[X32_PAGE::DEBUG] = new PageDebug(pagebasepar);
	pages[X32_PAGE::PROTOTYPEGUI] = new PagePrototypeGui(pagebasepar);
	pages[X32_PAGE::LIBRARY] = new PageLibrary(pagebasepar);
	pages[X32_PAGE::EFFECTS] = new PageEffects(pagebasepar);
	pages[X32_PAGE::SCENES] = new PageScenes(pagebasepar);
	
	for (const auto& [key, value] : pages)
	{
		value->Init();
	}	
}

bool CtrlClient::ShowNextPage()
{
	X32_PAGE activePage = (X32_PAGE)config->GetUint(ACTIVE_PAGE);

	X32_PAGE nextPage = pages[activePage]->GetNextPage();
	if (nextPage != X32_PAGE::NONE)
	{
		config->Set(ACTIVE_PAGE, (uint)nextPage);
		return true;
	} 
	
	return false;
}

bool CtrlClient::ShowPrevPage()
{
	X32_PAGE activePage = (X32_PAGE)config->GetUint(ACTIVE_PAGE);

	X32_PAGE prevPage = pages[activePage]->GetPrevPage();
	if (prevPage != X32_PAGE::NONE)
	{
		config->Set(ACTIVE_PAGE, (uint)prevPage);
		return true;
	}

	return false;
}

//#####################################################################################################################
//
//  ######  ##    ## ##    ##  ######  
// ##    ##  ##  ##  ###   ## ##    ## 
// ##         ####   ####  ## ##       
//  ######     ##    ## ## ## ##       
//       ##    ##    ##  #### ##       
// ##    ##    ##    ##   ### ##    ## 
//  ######     ##    ##    ##  ######
//
//#####################################################################################################################


// sync mixer state to GUI
void CtrlClient::syncGuiOrLcd() {

	//####################################
	//#     X32 Core - Sync Lcd
	//####################################

	if (config->IsModelX32Core()){
		if (state->x32core_lcdmode_setup) {
			lcdmenu->OnChange(false);
		}
		
		// return, because X32Core has no GUI
		return;
	}
	
	//####################################
	//#     Update Active Page
	//####################################

	Page* activePage = pages.at((X32_PAGE)(config->GetUint(ACTIVE_PAGE)));
	activePage->Change();

	//####################################
	//#     Show Active Page 
	//#
	//#  Has to be done after updating the current active page, so we get the call.
	//#  If done in other order, the changed Mixerparemter array is emptied and we never change the page!
	//####################################

	if (config->HasParameterChanged(ACTIVE_PAGE)){
		Page* newPage = pages.at((X32_PAGE)config->GetUint(ACTIVE_PAGE));
		newPage->Show();
	}

	//####################################
	//#     Update General Header
	//####################################

	uint chanIndex = config->GetUint(SELECTED_CHANNEL);
	if (config->HasParameterChanged(SELECTED_CHANNEL) ||
		config->HasParametersChanged({
			SELECTED_CHANNEL,
			CHANNEL_NAME,
			CHANNEL_COLOR}, chanIndex)
		)
	{
		lv_color_t color;

		switch (config->GetUint(CHANNEL_COLOR, chanIndex))
		{
			case SURFACE_COLOR_BLACK:
				color = lv_color_make(0, 0, 0);
				break;
			case SURFACE_COLOR_RED:
				color = lv_color_make(255, 0, 0);
				break;
			case SURFACE_COLOR_GREEN:
				color = lv_color_make(0, 255, 0);
				break;
			case SURFACE_COLOR_YELLOW:
				color = lv_color_make(255, 255, 0);
				break;
			case SURFACE_COLOR_BLUE:
				color = lv_color_make(0, 0, 255);
				break;
			case SURFACE_COLOR_PINK:
				color = lv_color_make(255, 0, 255);
				break;
			case SURFACE_COLOR_CYAN:
				color = lv_color_make(0, 255, 255);
				break;
			case SURFACE_COLOR_WHITE:
				color = lv_color_make(255, 255, 255);
				break;
		}

		lv_label_set_text(objects.current_channel_number, config->GetString(CHANNEL_NAME, chanIndex).c_str());
		lv_label_set_text(objects.current_channel_name, config->GetString(CHANNEL_NAME_INTERN, chanIndex).c_str());
		lv_obj_set_style_bg_color(objects.current_channel_color, color, 0);
	}
}

// sync mixer state to Surface
void CtrlClient::syncSurface(bool fullSync)
{
	// ######################################
	//
	// Check, if banking has changed
	//
	// ######################################

	if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32R())
	{
		if (config->HasParameterChanged(BANKING_INPUT))
		{
			OMCBankId bankToSwitchTo = (OMCBankId)(config->GetUint(BANKING_INPUT));
 
			if (bankToSwitchTo != OMCBankId::FLEX1)
			{
				// stop DCA Spill
				for (uint i = 0; i < DCA_GROUPS; i++)
				{
					config->Set(config->MpCalcId(DCA_GROUP_1_MASTER, i), false);
				}
			}

			if (config->IsModelX32FullOrM32())
			{
				OMCBankId bank1;
				OMCBankId bank2;

				switch(bankToSwitchTo)
				{
					case OMCBankId::CH1_16:
						bank1 = OMCBankId::CH1_8;
						bank2 = OMCBankId::CH9_16;
						break;
					case OMCBankId::CH17_32:
						bank1 = OMCBankId::CH17_24;
						bank2 = OMCBankId::CH25_32;
						break;
					case OMCBankId::AUX_USB_FX_RET:
						bank1 = OMCBankId::AUX_USB;
						bank2 = OMCBankId::FX_RET;
						break;
					case OMCBankId::BUS1_16:
						bank1 = OMCBankId::BUS1_8;
						bank2 = OMCBankId::BUS9_16;
						break;
					case OMCBankId::REMOTE1:
						bank1 = OMCBankId::REMOTE1;
						bank2 = OMCBankId::REMOTE2;
						break;
					case OMCBankId::FLEX1:
						bank1 = OMCBankId::FLEX1;
						bank2 = OMCBankId::FLEX2;
						break;
					default:
						break;
				}

				surface->LoadBank(OMCBankTarget::InputSection, bank1);
				surface->LoadBank(OMCBankTarget::InputSection2, bank2);
			}
			else
			{
				surface->LoadBank(OMCBankTarget::InputSection, (OMCBankId)(config->GetUint(BANKING_INPUT)));
			}
		}

		if (config->HasParameterChanged(BANKING_BUS))
		{
			OMCBankId bank = (OMCBankId)(config->GetUint(BANKING_BUS));

			// special translations for X32 Full: banking over input AND bus section
			switch(bank)
			{
				case OMCBankId::CH17_32:
					bank = OMCBankId::CH17_24;
					break;
				case OMCBankId::AUX_USB_FX_RET:
					bank = OMCBankId::AUX_USB;
					break;
				default:
					break;
			}

			surface->LoadBank(OMCBankTarget::BusSection, bank);
		}

		if (config->HasParameterChanged(BANKING_ASSIGN))
		{
			X32AssignBankId bankId = (X32AssignBankId)(config->GetUint(BANKING_ASSIGN));

			surface->LoadAssignBank(bankId);
		}

		// ######################################
		//
		//   DCA Spill
		//
		// ######################################

		vector<MP_ID> filter;
		for (uint i = 0; i < DCA_GROUPS; i++)
    	{
       		filter.push_back(config->MpCalcId(DCA_GROUP_1_MASTER, i));
		}
		if (config->HasParametersChanged(filter))
		{
			// loop through all DCA groups
			for (uint i = 0; i < DCA_GROUPS; i++)
			{
				MP_ID dcaGroupId = config->MpCalcId(DCA_GROUP_1_MASTER, i);
		
				if (config->HasParameterChanged(dcaGroupId))
				{
					if (config->GetBool(dcaGroupId))
					{
						// DCA Spill

						helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "DCA Spill");

						uint nextSurfaceChannelStrip = 0;

						// reset the banks to default blank
						surface->ResetBank(OMCBankId::FLEX1);
						surface->ResetBank(OMCBankId::FLEX2);
						surface->ResetBank(OMCBankId::FLEX3);

						// loop through all channels
						for (uint chanIndex = 0; chanIndex < MAX_VCHANNELS; chanIndex++)
						{
							X32FaderBank* banktoUse = (nextSurfaceChannelStrip < 8) ? surface->GetBank(OMCBankId::FLEX1) : surface->GetBank(OMCBankId::FLEX2);

							if (config->GetBool(config->MpCalcId(DCA_GROUP_1, i), chanIndex))
							{
								// channel is part of the spilled DCA Group -> bind it to the next free channel strip in the bank
								surface->SetChannelstripBinding(banktoUse, nextSurfaceChannelStrip % 8, chanIndex);
							 	
								nextSurfaceChannelStrip++;
							}	
							
							if (config->IsModelX32FullOrM32() && nextSurfaceChannelStrip > 16)
							{
								break;
							}
							if (config->IsModelX32CompactOrProducerOrM32R() && nextSurfaceChannelStrip > 8)
							{
								break;
							}
						}

						preSpillLoadedBank = (OMCBankId)config->GetUint(BANKING_INPUT);
						config->Set(BANKING_INPUT, (uint)OMCBankId::FLEX1);

						// trigger blinking of DCA bank button
						config->Refresh(BANKING_BUS);

						break;
					}
					else
					{
						// DCA "Unspill"

						helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "DCA Unspill");

						if (config->GetUint(BANKING_INPUT) == (uint)OMCBankId::FLEX1)
						{
							// load bank that was loaded before the DCA spill
							config->Set(BANKING_INPUT, (uint)preSpillLoadedBank);
						}
						preSpillLoadedBank = OMCBankId::None;

						// trigger unblinking of DCA button
						config->Refresh(BANKING_BUS);	
					}
				}
			}
		}
	}

	if (config->IsModelAnyWing())
	{
		if (config->HasParameterChanged(BANKING_INPUT))
		{
			OMCBankId bankToSwitchTo = (OMCBankId)(config->GetUint(BANKING_INPUT));
 
			// if (bankToSwitchTo != OMCBankId::FLEX1)
			// {
			// 	// stop DCA Spill
			// 	for (uint i = 0; i < DCA_GROUPS; i++)
			// 	{
			// 		config->Set(config->MpCalcId(DCA_GROUP_1_MASTER, i), false);
			// 	}
			// }

			if (config->IsModelWingCompact())
			{
				surface->LoadBank(OMCBankTarget::WING_COMPACT, bankToSwitchTo);
			}
		}

		// if (config->HasParameterChanged(BANKING_ASSIGN))
		// {
		// 	X32AssignBankId bankId = (X32AssignBankId)(config->GetUint(BANKING_ASSIGN));

		// 	LoadAssignBank(bankId);
		// }

		// // ######################################
		// //
		// //   DCA Spill
		// //
		// // ######################################

		// vector<MP_ID> filter;
		// for (uint i = 0; i < DCA_GROUPS; i++)
    	// {
       	// 	filter.push_back(config->MpCalcId(DCA_GROUP_1_MASTER, i));
		// }
		// if (config->HasParametersChanged(filter))
		// {
		// 	// loop through all DCA groups
		// 	for (uint i = 0; i < DCA_GROUPS; i++)
		// 	{
		// 		MP_ID dcaGroupId = config->MpCalcId(DCA_GROUP_1_MASTER, i);
		
		// 		if (config->HasParameterChanged(dcaGroupId))
		// 		{
		// 			if (config->GetBool(dcaGroupId))
		// 			{
		// 				// DCA Spill

		// 				helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "DCA Spill");

		// 				uint nextSurfaceChannelStrip = 0;

		// 				// reset the banks to default blank
		// 				banks[(uint)X32BankId::FLEX1]->Reset();
		// 				banks[(uint)X32BankId::FLEX2]->Reset();
		// 				banks[(uint)X32BankId::FLEX3]->Reset();

		// 				// loop through all channels
		// 				for (uint chanIndex = 0; chanIndex < MAX_VCHANNELS; chanIndex++)
		// 				{
		// 					X32FaderBank* banktoUse = (nextSurfaceChannelStrip < 8) ? banks[(uint)X32BankId::FLEX1] : banks[(uint)X32BankId::FLEX2];

		// 					if (config->GetBool(config->MpCalcId(DCA_GROUP_1, i), chanIndex))
		// 					{
		// 						// channel is part of the spilled DCA Group -> bind it to the next free channel strip in the bank
		// 						SetChannelstripBinding(banktoUse, nextSurfaceChannelStrip % 8, chanIndex);
							 	
		// 						nextSurfaceChannelStrip++;
		// 					}	
							
		// 					if (config->IsModelX32FullOrM32() && nextSurfaceChannelStrip > 16)
		// 					{
		// 						break;
		// 					}
		// 					if (config->IsModelX32CompactOrProducerOrM32R() && nextSurfaceChannelStrip > 8)
		// 					{
		// 						break;
		// 					}
		// 				}

		// 				preSpillLoadedBank = (X32BankId)config->GetUint(BANKING_INPUT);
		// 				config->Set(BANKING_INPUT, (uint)X32BankId::FLEX1);

		// 				// trigger blinking of DCA bank button
		// 				config->Refresh(BANKING_BUS);

		// 				break;
		// 			}
		// 			else
		// 			{
		// 				// DCA "Unspill"

		// 				helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "DCA Unspill");

		// 				if (config->GetUint(BANKING_INPUT) == (uint)X32BankId::FLEX1)
		// 				{
		// 					// load bank that was loaded before the DCA spill
		// 					config->Set(BANKING_INPUT, (uint)preSpillLoadedBank);
		// 				}
		// 				preSpillLoadedBank = X32BankId::None;

		// 				// trigger unblinking of DCA button
		// 				config->Refresh(BANKING_BUS);	
		// 			}
		// 		}
		// 	}
		// }
	}

	// ######################################
	//
	//   LCD Contrast and Brightness
	//
	// ######################################


	if (config->IsModelAnyXM32())
	{
		if (config->HasParameterChanged(LCD_CONTRAST))
		{
			uint contrast = config->GetUint(LCD_CONTRAST);
			helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Set LCD Contrast to %d", contrast);
			
			surface->SetContrast(X32_BOARD_EXTRA, contrast);
			surface->SetContrast(X32_BOARD_L, contrast);
			surface->SetContrast(X32_BOARD_M, contrast);
			surface->SetContrast(X32_BOARD_R, contrast);
		}

		if (config->HasParameterChanged(LED_BRIGHTNESS))
		{
			uint brightness = config->GetUint(LED_BRIGHTNESS);
			helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Set LED Brightness to %d", brightness);
			
			surface->SetBrightness(X32_BOARD_EXTRA, brightness);
			surface->SetBrightness(X32_BOARD_L, brightness);
			surface->SetBrightness(X32_BOARD_M, brightness);
			surface->SetBrightness(X32_BOARD_R, brightness);
		}
	}

	// ######################################
	//
	//   X32 Rack Channel Indicator
	//
	// ######################################

	if (config->IsModelX32Rack())
	{
		if (config->HasParameterChanged(SELECTED_CHANNEL))
		{
			setLedChannelIndicator_Rack();
		}
	}

	// ###########################################
	//
	// Sync all the bound Surfaceelements
	//
	// ###########################################

	for (auto const& [key, value] : *(config->GetSurfaceBinding()))
    {
		// ignore nullptr
		if (value == 0)
		{
			continue;
		}

		SurfaceElementId element_id = key;
		SurfaceBindingParameter* binding_parameter = value;

		// Filter Surfacelements with no visual/physical feedback
		switch(element_id)
		{
			case SurfaceElementId::DISPLAY_ENCODER_1:
			case SurfaceElementId::DISPLAY_ENCODER_2:
			case SurfaceElementId::DISPLAY_ENCODER_3:
			case SurfaceElementId::DISPLAY_ENCODER_4:
			case SurfaceElementId::DISPLAY_ENCODER_5:
			case SurfaceElementId::DISPLAY_ENCODER_6:
			case SurfaceElementId::DISPLAY_ENCODER_BUTTON_1:
			case SurfaceElementId::DISPLAY_ENCODER_BUTTON_2:
			case SurfaceElementId::DISPLAY_ENCODER_BUTTON_3:
			case SurfaceElementId::DISPLAY_ENCODER_BUTTON_4:
			case SurfaceElementId::DISPLAY_ENCODER_BUTTON_5:
			case SurfaceElementId::DISPLAY_ENCODER_BUTTON_6:
			case SurfaceElementId::UP:
			case SurfaceElementId::DOWN:
			case SurfaceElementId::LEFT:
			case SurfaceElementId::RIGHT:
				config->RemoveSurfaceBindingChanged(element_id);
				continue;
				break;
			default:
				break;
		}

		// calcluate Mixerparameter ID and Index
		MP_ID parameter_id = config->ParameterCalcId(binding_parameter);
		uint parameter_index = config->ParameterCalcIndex(binding_parameter);

		// full sync
		bool hasChanged = fullSync;

		// Check if the bound Mixerparameter has changed
		if (!hasChanged)
		{
			switch (binding_parameter->mp_action)
			{
				case MixerparameterAction::CLEAR_SOLO:
					hasChanged = config->HasParameterChanged(CHANNEL_SOLO) || config->HasParameterChanged(SOLO_ACTIVE);
					break;
				case MixerparameterAction::LCD_Channel:
					switch(config->GetUint(CHANNEL_LCD_MODE))
					{
						case 0:
							if (config->HasParametersChanged({CHANNEL_PANORAMA, CHANNEL_NAME, CHANNEL_COLOR, CHANNEL_COLOR_INVERTED	}, binding_parameter->mp_index) ||
								config->HasParameterChanged(CHANNEL_LCD_MODE)
							)
							{
								hasChanged = true;
							}
							break;
						case 1:
							if (config->HasParametersChanged({CHANNEL_PHASE_INVERT, CHANNEL_VOLUME, CHANNEL_PANORAMA, CHANNEL_GAIN,	CHANNEL_GATE_TRESHOLD,
									CHANNEL_DYNAMICS_TRESHOLD, CHANNEL_PHANTOM, CHANNEL_NAME, CHANNEL_COLOR, CHANNEL_COLOR_INVERTED }, binding_parameter->mp_index) ||
								config->HasParametersChanged({MP_CAT::CHANNEL_EQ}, binding_parameter->mp_index) || 
								config->HasParameterChanged(CHANNEL_LCD_MODE)
							)
							{
								hasChanged = true;
							}
							break;
					}
					break;
				case MixerparameterAction::LCD_Assign:
					{
						// LCD 1 -> Encoder 1, BUtton 5, Button 9
						if (element_id == SurfaceElementId::ASSIGN_LCD_1)
						{
							hasChanged =
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_ENCODER_1) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_ENCODER_1) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_5) ||	
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_5) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_9) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_9);
						}

						if (element_id == SurfaceElementId::ASSIGN_LCD_2)
						{
							hasChanged =
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_ENCODER_2) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_ENCODER_2) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_6) ||	
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_6) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_10)||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_10);
						}

						if (element_id == SurfaceElementId::ASSIGN_LCD_3)
						{
							hasChanged =
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_ENCODER_3) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_ENCODER_3) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_7) ||	
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_7) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_11) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_11);
						}

						if (element_id == SurfaceElementId::ASSIGN_LCD_4)
						{
							hasChanged =
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_ENCODER_4) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_ENCODER_4) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_8) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_8) ||
								config->HasSurfaceBindingChanged(SurfaceElementId::ASSIGN_12) ||
								config->HasBoundParameterChanged(SurfaceElementId::ASSIGN_12);
						}
					}
					break;
				case MixerparameterAction::LCD_Artnet:
					hasChanged = config->HasParameterChanged(DMX_ARTNET_VALUE, binding_parameter->mp_index);
					break;
				case MixerparameterAction::SET__MP_INDIRECT__SELECTED_CHANNEL:
				case MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL:
					hasChanged |= config->HasParameterChanged((MP_ID)binding_parameter->mp_index);
					[[fallthrough]]; // intentionally no break!
				case MixerparameterAction::SET_SELECTED_CHANNEL:
				case MixerparameterAction::TOGGLE_SELECTED_CHANNEL:
				case MixerparameterAction::CHANGE_SELECTED_CHANNEL:
					hasChanged |= config->HasParameterChanged(SELECTED_CHANNEL);
					[[fallthrough]]; // intentionally no break!
				default:
					hasChanged |= config->HasParameterChanged(parameter_id, parameter_index);
					break;
			}
		}
		
		/*
		 Check if the binding has changed		
		*/

		if(config->HasSurfaceBindingChanged(element_id))
		{
			hasChanged = true;
			config->RemoveSurfaceBindingChanged(element_id);
		}

		/*
		 Now sync the Surfaceelements
		*/

		if (hasChanged && config->HasSurfaceElement(element_id))
		{
			SurfaceElement* element = config->GetSurfaceElement(element_id);

			helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Caluclated Mixerparamter: %s Index: %d", config->GetParameter(parameter_id)->GetName().c_str(), parameter_index);

			// ####################################################
			//
			//                      FADER
			//
			// ####################################################
			if (element->element_type == SurfaceElementType::Fader)
			{
				if (parameter_id == NONE)
				{
					// set to -infinity
					surface->SetFader(element->GetBoard(), element->GetIndex(), 0);

					continue;
				}

				u_int16_t faderPosition = 0;

				if (parameter_id == MP_ID::DMX_ARTNET_VALUE)
				{
					faderPosition = helper->DMX2Fadervalue(config->GetUint(parameter_id, parameter_index));
				}
				else 
				{
					faderPosition = helper->Dbfs2Fader(config->GetFloat(parameter_id, parameter_index));
				}

				surface->SetFader(element->GetBoard(), element->GetIndex(), faderPosition);
			}
			// ####################################################
			//
			//                   BUTTON / LED
			//
			// ####################################################
			else if (element->element_type == SurfaceElementType::Button || element->element_type == SurfaceElementType::Led)
			{
				if (parameter_id == NONE)
				{
					// set dark
					surface->SetLed(element_id, false, false);

					continue;
				}

				bool ledOn = false;
				bool ledBlink = false;

				if (binding_parameter->mp_action == MixerparameterAction::CLEAR_SOLO)
				{
					ledBlink = config->GetBool(SOLO_ACTIVE);
				}
				if (binding_parameter->mp_action == MixerparameterAction::SET_TO_INDEX)
				{					
					ledOn = config->GetInt(parameter_id, parameter_index) == binding_parameter->mp_index;

					// let DCA Bank Led blink on DCA Spill
					if (
						preSpillLoadedBank != OMCBankId::None &&
						(
							(parameter_id == BANKING_BUS && binding_parameter->mp_index == (uint)OMCBankId::DCA) ||
							(
								(
									parameter_id == DCA_GROUP_1_MASTER || 
									parameter_id == DCA_GROUP_2_MASTER || 
									parameter_id == DCA_GROUP_3_MASTER || 
									parameter_id == DCA_GROUP_4_MASTER || 
									parameter_id == DCA_GROUP_5_MASTER || 
									parameter_id == DCA_GROUP_6_MASTER || 
									parameter_id == DCA_GROUP_7_MASTER || 
									parameter_id == DCA_GROUP_8_MASTER
								) &&
							 	config->GetBool(parameter_id)
							)
						)
					)
					{
						ledBlink = true;
					}
				}
				else if (binding_parameter->mp_action == MixerparameterAction::SET__MP_INDIRECT__SELECTED_CHANNEL)
				{
					ledOn = config->GetInt(parameter_id, parameter_index) == binding_parameter->extra_value;
				}
				else
				{
					ledOn = config->GetBool(parameter_id, parameter_index);
				}

				// let the LED blink (if the Mixerparamter says it to do)
				if (ledOn && config->GetParameter(parameter_id)->GetBlink())
				{
					ledBlink = ledOn;
				}				

				surface->SetLed(element_id, ledOn, ledBlink);
			}
			// ####################################################
			//
			//                     ENCODER
			//
			// ####################################################
			else if (element->element_type == SurfaceElementType::Encoder)
			{
				if (parameter_id == NONE)
				{
					// set dark
					surface->SetEncoderRing(element->GetBoard(), element->GetIndex(), 0, 0, 0);

					continue;
				}

				switch(config->GetParameter(parameter_id)->GetUOM())
				{
					case MP_UOM::HZ:
						{
							float max = config->GetParameter(parameter_id)->GetMax();
							float min = config->GetParameter(parameter_id)->GetMin();
							float max_zerobased = (max-min);

							float wert = config->GetFloat(parameter_id, parameter_index);

				            uint position = (uint)((13+1)-std::pow(13, (1.0 - (wert / max_zerobased))));
                    
                			surface->SetEncoderRing(element->GetBoard(), element->GetIndex(), 6, position, 1);
						}
						break;
					case MP_UOM::PANORAMA:
						surface->SetEncoderRing(element->GetBoard(), element->GetIndex(), 2, (config->GetFloat(parameter_id, parameter_index) + 100.0f)/2.0f, 1);
						break;
					default:
						surface->SetEncoderRing(element->GetBoard(), element->GetIndex(), 0, config->GetPercent(parameter_id, parameter_index), 1);
				}
			}
			// ####################################################
			//
			//                       LCD
			//
			// ####################################################
			else if (element->element_type == SurfaceElementType::Lcd)
			{
				if(!state->surface_disable_lcd_update)
				{
					if (binding_parameter->mp_action == MixerparameterAction::LCD_Channel)
					{
						SetLcdFromChannel(element->GetBoard(), element->GetIndex(), parameter_index);
					}
					else if (binding_parameter->mp_action == MixerparameterAction::LCD_Assign)
					{
						SetLcdFromAssign(element->GetBoard(), element->GetIndex(), element_id);
					}
					else if (binding_parameter->mp_action == MixerparameterAction::LCD_Artnet)
					{
						SetLcdFromArtnet(element->GetBoard(), element->GetIndex(), parameter_index);
					}
					else
					{
						// empty
						SetLcdDark(element->GetBoard(), element->GetIndex());
					}
				}
			}
		}
	}
}


	


	// if (config->IsModelX32Core()) {

	// 	if (config->HasParameterChanged(SELECTED_CHANNEL)){
	// 		setLedChannelIndicator_Core();
	// 	}

	// 	if (!state->x32core_lcdmode_setup && (
	// 			config->HasParametersChanged({CHANNEL_SOLO,CHANNEL_MUTE,CHANNEL_COLOR,CHANNEL_NAME}, chanIndex) ||
	// 			config->HasParameterChanged(SELECTED_CHANNEL)
	// 		)
	// 	)
	// 	{
	// 		SetLcdFromChannel(X32_BOARD_MAIN, 0, chanIndex);
	// 	}

	// 	// Volume
	// 	if (config->HasParametersChanged({CHANNEL_VOLUME, CHANNEL_MUTE}, chanIndex))
	// 	{
	// 		surface->SetEncoderRingDbfs(X32_BOARD_MAIN, 0, config->GetFloat(CHANNEL_VOLUME, chanIndex), config->GetBool(CHANNEL_MUTE, chanIndex), 1);
	// 	}

	// 	// Main Channel
	// 	if (config->HasParametersChanged({CHANNEL_VOLUME, CHANNEL_MUTE}, (uint)X32_VCHANNEL_BLOCK::MAIN))
	// 	{
	// 		surface->SetEncoderRingDbfs(1, 1, config->GetFloat(CHANNEL_VOLUME, (uint)X32_VCHANNEL_BLOCK::MAIN), config->GetBool(CHANNEL_MUTE, (uint)X32_VCHANNEL_BLOCK::MAIN), 1);
	// 	}
	// }


void CtrlClient::SetLcdFromArtnet(uint8_t p_boardId, uint8_t lcdIndex, uint8_t artnetIndex)
{
	using enum MP_ID;

    LcdData* data = new LcdData();
	uint textcount = 0;
	uint textIndex = 0;

	data->boardId = p_boardId;
	data->color = (uint)X32_COLOR::YELLOW;
	data->lcdIndex = lcdIndex;
	data->icon.icon = 0;
	data->icon.x = 0;
	data->icon.y = 0;

	// Channel Name
	data->texts[textIndex].text = "DMX" + String(artnetIndex + 1);
	data->texts[textIndex].size = 0x20;
	data->texts[textIndex].x = 0;
	data->texts[textIndex].y = 0;

	textIndex++;

	// Value
	//float value = artnet->GetValue(artnetIndex);
	float value = config->GetFloat(DMX_ARTNET_VALUE, artnetIndex);
	data->texts[textIndex].text = String(value, 0) + " / " + String(value/2.55f, 1) + "%";
	data->texts[textIndex].size = 0;
	data->texts[textIndex].x = 0;
	data->texts[textIndex].y = 20;

	textIndex++;

	// Value
	if (value < 32){
		data->texts[textIndex].text = "";
	} else if (value < 64){
		data->texts[textIndex].text = "O";
	} else if (value < 96){
		data->texts[textIndex].text = "OO";
	} else if (value < 128){
		data->texts[textIndex].text = "OOO";
	} else if (value < 160){
		data->texts[textIndex].text = "OOOO";
	} else if (value < 192){
		data->texts[textIndex].text = "OOOOO";
	} else if (value < 224){
		data->texts[textIndex].text = "OOOOOO";
	} else if (value < 240){
		data->texts[textIndex].text = "OOOOOOO";
	} else {
		data->texts[textIndex].text = "OOOOOOOO";
	}
	data->texts[textIndex].size = 0;
	data->texts[textIndex].x = 0;
	data->texts[textIndex].y = 51;

	textcount = textIndex + 1;

	surface->SetLcd(data, textcount);
	delete data;
}

void CtrlClient::SetLcdFromChannel(uint8_t p_boardId, uint8_t lcdIndex, uint8_t channelIndex)
{
	using enum MP_ID;

    LcdData* data = new LcdData();
	uint textcount = 0;

	if (config->IsModelAnyWing() && lcdIndex == 12) // Fader 13 (zero based index)
	{
		lcdIndex = 0x54; // Header on User LCD
	}
	
	switch(config->GetUint(CHANNEL_LCD_MODE))
	{
		case 0:
			{
				uint textIndex = 0;

				data->boardId = p_boardId;
				data->color = config->GetUint(CHANNEL_COLOR, channelIndex) | (config->GetUint(CHANNEL_COLOR_INVERTED, channelIndex) * SURFACE_COLOR_INVERTED);
				data->lcdIndex = lcdIndex;
				data->icon.icon = 0;
				data->icon.x = 0;
				data->icon.y = 0;

				// Volume / Panorama

				float balance = config->GetFloat(CHANNEL_PANORAMA, channelIndex);
				
				char balanceText[8] = "-------";
				if (balance < -70){
					balanceText[0] = '|';
				} else if (balance < -40){
					balanceText[1] = '|';
				} else if (balance < -10){
					balanceText[2] = '|';
				} else if (balance > 70){
					balanceText[6] = '|';
				} else if (balance > 40){
					balanceText[5] = '|';
				} else if (balance > 10){
					balanceText[4] = '|';
				} else {
					balanceText[3] = '|';
				}
				data->texts[textIndex].text = balanceText;    
				data->texts[textIndex].size = 0;
				data->texts[textIndex].x = 0;
				data->texts[textIndex].y = 0;

				textIndex++;

				// Channel Name
				data->texts[textIndex].text = config->GetString(CHANNEL_NAME, channelIndex);
				data->texts[textIndex].size = 0x20;
				data->texts[textIndex].x = 0;
				data->texts[textIndex].y = 20;

				textIndex++;

				// Channel Internal Name
				data->texts[textIndex].text = config->GetString(CHANNEL_NAME_INTERN, channelIndex);
				data->texts[textIndex].size = 0;
				data->texts[textIndex].x = 35;
				data->texts[textIndex].y = 51;

				textcount = textIndex + 1;
			}
			break;
		case 1:
			{
				data->boardId = p_boardId;
				data->color = config->GetUint(CHANNEL_COLOR, channelIndex) | (config->GetUint(CHANNEL_COLOR_INVERTED, channelIndex) * SURFACE_COLOR_INVERTED );
				data->lcdIndex = lcdIndex;
				data->icon.icon = 0;
				data->icon.x = 0;
				data->icon.y = 0;

				// Gain / Lowcut
				data->texts[0].text = String(config->GetFloat(CHANNEL_GAIN, channelIndex), 1) + String("dB ") + String(config->GetFloat(CHANNEL_LOWCUT_FREQ, channelIndex), 0) + String("Hz");
				data->texts[0].size = 0;
				data->texts[0].x = 3;
				data->texts[0].y = 0;

				// Phanton / Invert / Gate / Dynamics / EQ active
				data->texts[1].text =
					String(config->GetBool(CHANNEL_PHANTOM, channelIndex) ? "48V " : "    ") +
					String(config->GetBool(CHANNEL_PHASE_INVERT, channelIndex) ? "@ " : "  ") +
					String(config->GetFloat(CHANNEL_GATE_TRESHOLD, channelIndex) > -80.0f ? "G " : "  ") +
					String(config->GetFloat(CHANNEL_DYNAMICS_TRESHOLD, channelIndex) < 0.0f ? "D " : "  ") +
					// TODO String(mixer->GetEq(channelIndex) ? "E " : "  ");
					String(true ? "E" : " ");
				data->texts[1].size = 0;
				data->texts[1].x = 10;
				data->texts[1].y = 15;

				// Volume / Panorama

				float balance = config->GetFloat(CHANNEL_PANORAMA, channelIndex);
				
				char balanceText[8] = "-------";
				if (balance < -70){
					balanceText[0] = '|';
				} else if (balance < -40){
					balanceText[1] = '|';
				} else if (balance < -10){
					balanceText[2] = '|';
				} else if (balance > 70){
					balanceText[6] = '|';
				} else if (balance > 40){
					balanceText[5] = '|';
				} else if (balance > 10){
					balanceText[4] = '|';
				} else {
					balanceText[3] = '|';
				}

				float volume = config->GetFloat(CHANNEL_VOLUME, channelIndex);
				if (volume > -100) {
					data->texts[2].text = String(volume, 1) + String("dB ") + String(balanceText);
				}else{
					data->texts[2].text = String("-oodB ") + String(balanceText);
				}
				data->texts[2].size = 0;
				data->texts[2].x = 8;
				data->texts[2].y = 30;

				// vChannel Name
				data->texts[3].text = config->GetString(CHANNEL_NAME, channelIndex);
				data->texts[3].size = 0;
				data->texts[3].x = 0;
				data->texts[3].y = 48;

				textcount = 4;
			}
			break;
	}

	surface->SetLcd(data, textcount);
	delete data;
}

void CtrlClient::SetLcdFromAssign(uint8_t p_boardId, uint8_t lcdIndex, SurfaceElementId element_id)
{
	helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "SetLcdFromAssign()");

    LcdData* data = new LcdData();
	uint textcount = 0;

	data->boardId = p_boardId;
	data->lcdIndex = lcdIndex;
	data->color = SURFACE_COLOR_WHITE | SURFACE_COLOR_INVERTED;
	data->icon.icon = 0;
	data->icon.x = 0;
	data->icon.y = 0;

	data->texts[0].size = 0;
	data->texts[0].x = 0;
	data->texts[0].y = 0;

	data->texts[1].size = 0;
	data->texts[1].x = 0;
	data->texts[1].y = 15;

	data->texts[2].size = 0;
	data->texts[2].x = 0;
	data->texts[2].y = 30;

	data->texts[3].size = 0;
	data->texts[3].x = 0;
	data->texts[3].y = 48;

	textcount = 4;

	// thats the most data we can send -> over all in summary not more than 4*12 = 48 characters
	// data->texts[0].text = "llllllllllll";
	// data->texts[1].text = "llllllllllll";
	// data->texts[2].text = "llllllllllll";
	// data->texts[3].text = "llllllllllll";

	switch(element_id)
	{
		case SurfaceElementId::ASSIGN_LCD_1:
            GetAssignLcdText(data, SurfaceElementId::ASSIGN_ENCODER_1, SurfaceElementId::ASSIGN_5, SurfaceElementId::ASSIGN_9);
            break;
		case SurfaceElementId::ASSIGN_LCD_2:
		    GetAssignLcdText(data, SurfaceElementId::ASSIGN_ENCODER_2, SurfaceElementId::ASSIGN_6, SurfaceElementId::ASSIGN_10);
			break;
		case SurfaceElementId::ASSIGN_LCD_3:
		    GetAssignLcdText(data, SurfaceElementId::ASSIGN_ENCODER_3, SurfaceElementId::ASSIGN_7, SurfaceElementId::ASSIGN_11);
			break;
		case SurfaceElementId::ASSIGN_LCD_4:
			GetAssignLcdText(data, SurfaceElementId::ASSIGN_ENCODER_4, SurfaceElementId::ASSIGN_8, SurfaceElementId::ASSIGN_12);
			break;
		default:
			break;
	}

	surface->SetLcd(data, textcount);
	delete data;
}

void CtrlClient::GetAssignLcdText(LcdData *data, SurfaceElementId encoder, SurfaceElementId upper_button, SurfaceElementId lower_button)
{
	// Encoder

	MP_ID encoder_parameter_id = config->ParameterCalcId(config->GetSurfaceBinding(encoder));
	uint encoder_parameter_index = config->ParameterCalcIndex(config->GetSurfaceBinding(encoder));
	Mixerparameter* encoder_parameter = config->GetParameter(encoder_parameter_id);
	String encoder_prefix;

    if (encoder_parameter->BelongsToChannel())
    {
		encoder_prefix = config->GetParameter(CHANNEL_NAME)->GetString(encoder_parameter_index) + String(": ");
	}

    data->texts[0].text = encoder_prefix + encoder_parameter->GetNameShort();
    data->texts[1].text = encoder_parameter->GetFormatedValue(encoder_parameter_index);

	// Upper Button

	MP_ID upper_button_parameter_id = config->ParameterCalcId(config->GetSurfaceBinding(upper_button));
	uint upper_button_parameter_index = config->ParameterCalcIndex(config->GetSurfaceBinding(upper_button));
	Mixerparameter* upper_button__parameter = config->GetParameter(upper_button_parameter_id);
	String upper_button_prefix;

	if (upper_button__parameter->BelongsToChannel())
	{
		upper_button_prefix += config->GetParameter(CHANNEL_NAME)->GetString(upper_button_parameter_index) + String(": ");
	}

    data->texts[2].text = upper_button_prefix + config->GetParameter(upper_button_parameter_id)->GetNameShort();

	// Lower Button

	MP_ID lower_button_parameter_id = config->ParameterCalcId(config->GetSurfaceBinding(lower_button));
	uint lower_button_parameter_index = config->ParameterCalcIndex(config->GetSurfaceBinding(lower_button));
	Mixerparameter* lower_button__parameter = config->GetParameter(lower_button_parameter_id);
	String lower_button_prefix;

	if (lower_button__parameter->BelongsToChannel())
	{
		lower_button_prefix += config->GetParameter(CHANNEL_NAME)->GetString(lower_button_parameter_index) + String(": ");
	}

    data->texts[3].text = lower_button_prefix + config->GetParameter(lower_button_parameter_id)->GetNameShort();

	helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "GetAssignLcdText() Row 1: %s", data->texts[0].text.c_str());
	helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "GetAssignLcdText() Row 2: %s", data->texts[1].text.c_str());
	helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "GetAssignLcdText() Row 3: %s", data->texts[2].text.c_str());
	helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "GetAssignLcdText() Row 4: %s", data->texts[3].text.c_str());
}



void CtrlClient::SetLcdDark(uint8_t p_boardId, uint8_t lcdIndex)
{
	using enum MP_ID;

    LcdData* data = new LcdData();
	
	data->boardId = p_boardId;
	data->color = (uint)X32_COLOR::BLACK;
	data->lcdIndex = lcdIndex;
	data->icon.icon = 0;
	data->icon.x = 0;
	data->icon.y = 0;

	surface->SetLcd(data, 0);

	delete data;
}

// Update all meters (Gui, Surface, xremote)
void CtrlClient::UpdateMeters()
{
	if (state->surface_disable_meter_update)
	{
		return;
	}

	// ########################################
	//
	//		GUI Meters
	//
	// ########################################

	if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32ROrRack())
	{
		pages[(X32_PAGE)config->GetUint(ACTIVE_PAGE)]->UpdateMeters();
	}

	// ########################################
	//
	//		Surface Meters
	//
	// ########################################

	uint8_t selectedChannel = config->GetUint(SELECTED_CHANNEL);
    

	if (config->IsModelX32Core())
	{
		surface->SetMeterLed(X32_BOARD_MAIN, 0, helper->GetMeter8Info(config->GetInt(CHANNEL_METER_DECAYED_POST_GAIN, selectedChannel)));
	}

	if (config->IsModelX32Rack())
	{
		surface->SetMeterLedMain_Rack(
			helper->GetMeter8Info(config->GetInt(CHANNEL_METER_DECAYED_POST_GAIN, selectedChannel)),
			helper->GetMeter18Info(config->GetInt(MAIN_L_METER_DECAYED_POST_GAIN)),
			helper->GetMeter18Info(config->GetInt(MAIN_R_METER_DECAYED_POST_GAIN)),
			helper->GetMeter18Info(config->GetInt(SUB_METER_DECAYED_POST_GAIN))
		);
	}

	if (config->HasSmallDisplay())
	{
		surface->SetMeterLedMain_Producer(
			helper->GetMeter8Info(config->GetInt(CHANNEL_METER_DECAYED_POST_GAIN, selectedChannel)),
			surfaceCalcDynamicMeter(selectedChannel),
			helper->GetMeter18Info(config->GetInt(MAIN_L_METER_DECAYED_POST_GAIN)),
			helper->GetMeter18Info(config->GetInt(MAIN_R_METER_DECAYED_POST_GAIN)),
			helper->GetMeter18Info(config->GetInt(SUB_METER_DECAYED_POST_GAIN))
		);
	}

	if (config->HasBigDisplay())
	{
		surface->SetMeterLedMain_FullOrCompact(
			helper->GetMeter8Info(config->GetInt(CHANNEL_METER_DECAYED_POST_GAIN, selectedChannel)),
			surfaceCalcDynamicMeter(selectedChannel),		
			helper->GetMeter24Info(config->GetInt(MAIN_L_METER_DECAYED_POST_GAIN)),
			helper->GetMeter24Info(config->GetInt(MAIN_R_METER_DECAYED_POST_GAIN)),
			helper->GetMeter24Info(config->GetInt(SUB_METER_DECAYED_POST_GAIN))
		);
	}

	
	// ########################################
	//
	//		Channels
	//
	// ########################################

	if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32R())
	{
		for (uint8_t i = 0; i < 8; i++)
		{
			SurfaceBindingParameter* binding_board_l = config->GetSurfaceBinding((SurfaceElementId)((uint)SurfaceElementId::BOARD_L_VUMETER_1 + i));
			if (binding_board_l)
			{
				uint8_t meter6info = helper->GetMeter6Info(config->GetInt(CHANNEL_METER_DECAYED_POST_GAIN, binding_board_l->mp_index));
				surface->SetMeterLed(X32_BOARD_L, i, meter6info);
			}

			if (config->IsModelX32Full())
			{
				SurfaceBindingParameter* binding_board_m = config->GetSurfaceBinding((SurfaceElementId)((uint)SurfaceElementId::BOARD_M_VUMETER_1 + i));
				if (binding_board_m)
				{
					uint8_t meter6info = helper->GetMeter6Info(config->GetInt(CHANNEL_METER_DECAYED_POST_GAIN, binding_board_m->mp_index));
					surface->SetMeterLed(X32_BOARD_M, i, meter6info);
				}
			}

			SurfaceBindingParameter* binding_board_r = config->GetSurfaceBinding((SurfaceElementId)((uint)SurfaceElementId::BOARD_R_VUMETER_1 + i));
			if (binding_board_r)
			{
				uint8_t meter6info = helper->GetMeter6Info(config->GetInt(CHANNEL_METER_DECAYED_POST_GAIN, binding_board_r->mp_index));
				surface->SetMeterLed(X32_BOARD_R, i, meter6info);
			}
		}
	}
}


// only X32 Rack
void CtrlClient::setLedChannelIndicator_Rack()
{
	uint chanIdx = config->GetUint(SELECTED_CHANNEL);
	surface->SetLed(SurfaceElementId::LED_IN, (chanIdx <= 31), false);
	surface->SetLed(SurfaceElementId::LED_AUX_FX, (chanIdx >= 32)&&(chanIdx <= 47), false);
	surface->SetLed(SurfaceElementId::LED_BUS, (chanIdx >= 48)&&(chanIdx <= 63), false);
	surface->SetLed(SurfaceElementId::LED_MATRIX, (chanIdx >= 64)&&(chanIdx <= 69), false);
	surface->SetLed(SurfaceElementId::LED_MAIN, (chanIdx >= 70)&&(chanIdx <= 71), false);
	surface->SetLed(SurfaceElementId::LED_DCA, (chanIdx >= 72)&&(chanIdx <= 79), false);
	surface->SetLed(SurfaceElementId::LED_MAIN, (chanIdx == 80), false);

	// set 7-Segment Display
	surface->SetX32RackDisplay(chanIdx);        
}

// only X32 Core
void CtrlClient::setLedChannelIndicator_Core()
{
	uint8_t chanIdx = config->GetUint(SELECTED_CHANNEL);
	surface->SetLed(SurfaceElementId::LED_IN, (chanIdx <= 31), false);
	surface->SetLed(SurfaceElementId::LED_AUX_FX, (chanIdx >= 32)&&(chanIdx <= 47), false);		
	surface->SetLed(SurfaceElementId::LED_BUS, (chanIdx >= 48)&&(chanIdx <= 63), false);
	surface->SetLed(SurfaceElementId::LED_DCA, (chanIdx >= 64)&&(chanIdx <= 69), false);
	surface->SetLed(SurfaceElementId::LED_MATRIX, (chanIdx >= 70)&&(chanIdx <= 79), false);
}

uint8_t CtrlClient::surfaceCalcDynamicMeter(uint8_t channel) {
	// leds = 8-bit bitwise (bit 0=-60dB ... 4=-6dB, 5=Clip, 6=Gate, 7=Comp)
	if (channel < 40) {
		uint32_t meterdata = 0;

		//if (!!RECEIVED_CHANNEL_COMPRESSOR!! < 1.0f) { meterdata |= 0b10000000; };

/*
		float gateValue = (1.0f - !!RECEIVED_CHANNEL_GAIN!!) * 80.0f;
		if (gateValue >= 2.0f)  { meterdata |= 0b00100000; }        
		if (gateValue >= 4.0f)  { meterdata |= 0b00010000; }        
		if (gateValue >= 6.0f)  { meterdata |= 0b00001000; }        
		if (gateValue >= 10.0f) { meterdata |= 0b00000100; }        
		if (gateValue >= 18.0f) { meterdata |= 0b00000010; }        
		if (gateValue >= 30.0f) { meterdata |= 0b00000001; }        

		if (!!RECEIVED_CHANNEL_GAIN!! < 1.0f) { meterdata |= 0b01000000; };
*/
		return meterdata;
	}else{
		return 0; // no dynamic-data for other channels at the moment
	}
}

//#####################################################################################################################
//
//  ######  ##     ## ########  ########    ###     ######  ########      #### ##    ## ########  ##     ## ######## 
// ##    ## ##     ## ##     ## ##         ## ##   ##    ## ##             ##  ###   ## ##     ## ##     ##    ##    
// ##       ##     ## ##     ## ##        ##   ##  ##       ##             ##  ####  ## ##     ## ##     ##    ##    
//  ######  ##     ## ########  ######   ##     ## ##       ######         ##  ## ## ## ########  ##     ##    ##    
//       ## ##     ## ##   ##   ##       ######### ##       ##             ##  ##  #### ##        ##     ##    ##    
// ##    ## ##     ## ##    ##  ##       ##     ## ##    ## ##             ##  ##   ### ##        ##     ##    ##    
//  ######   #######  ##     ## ##       ##     ##  ######  ########      #### ##    ## ##         #######     ##    
//
//#####################################################################################################################


void CtrlClient::OnSurfaceCallback(void* arg, OMC_BOARD board, char command, uint8_t index, uint16_t value)
{
	CtrlClient* ctrl = static_cast<CtrlClient*>(arg);
    ctrl->ProcessSurface(board, command, index, value);
}

void CtrlClient::ProcessSurface(OMC_BOARD board, char command, uint8_t index, uint16_t value)
{
	if (config->HasDisplay())
	{
		lv_label_set_text_fmt(objects.header_debug, "Surface Input: BoardId 0x%02X, Command %c, Index 0x%02X, Value 0x%04X", (uint)board, command, index, value);
	}

	if (command == 'f') // Fader
	{
		// find surfaceelement
		SurfaceElement* fader = config->GetSurfaceElementFader(board, index);
		if (fader == 0)
		{
			helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Fader is not defined!");
			return;
		}

		SurfaceBindingParameter* bindingParameter = config->GetSurfaceBinding(fader->GetId());
		if (bindingParameter == 0) { return; }
				
		switch (bindingParameter->mp_action)
		{
			case MixerparameterAction::SET:
				config->Set(bindingParameter->mp_id, helper->Fadervalue2dBfs(value), bindingParameter->mp_index);
				surface->FaderMoved((uint)board, index, value);
				break;
			case MixerparameterAction::DMX:
				config->Set(bindingParameter->mp_id, helper->Fadervalue2DMX(value), bindingParameter->mp_index);
				surface->FaderMoved((uint)board, index, value);
				break;
			default:
				break;
		}
	}
	else if (command == 'b') // Button
	{
		// find surfaceelement
		SurfaceElement* button;
		if(config->IsModelAnyXM32())
		{
			button = config->GetSurfaceElementButton_XM32(board, value);
		}
		else if (config->IsModelAnyWing())
		{
			button = config->GetSurfaceElementButton_Wing(board, index);
		}
		if (button == 0) 
		{
			helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Button is not defined!");
			return; 
		}

		SurfaceBindingParameter* bindingParameterButton = config->GetSurfaceBinding(button->GetId());

		bool isButtonPressed = false;
		if (config->IsModelAnyXM32())
		{
			isButtonPressed = (value >> 7) == 1;
		}
		else if (config->IsModelAnyWing())
		{
			isButtonPressed = (value==1);
		}

		// Logic for double button press
		if (isButtonPressed) {
			if (buttonPressed == 0) {
				buttonPressed = button;
			} else {
				secondbuttonPressed = button;
				helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "DoubleButtonPress: Button1 \"%s\", Button2 \"%s\"", buttonPressed->GetName().c_str(), secondbuttonPressed->GetName().c_str());
			}
		} else {
			if (buttonPressed == button) {
				buttonPressed = 0;				
			}
			if (secondbuttonPressed == button) {
				secondbuttonPressed = 0;
			}
		}

		helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Button \"%s\" %s",
			button->GetName().c_str(),
			isButtonPressed ? "pressed" : "released"
		);

		
		if (bindingParameterButton != 0 && bindingParameterButton->mp_action != MixerparameterAction::NONE)
		{
			MP_ID parameter_id = config->ParameterCalcId(bindingParameterButton);
			uint parameter_index = config->ParameterCalcIndex(bindingParameterButton);

			Mixerparameter* parameter = config->GetParameter(parameter_id);

			// Member Assign Mode (e.g. Mute Groups, DCA Groups)
			bool memberAssingMode = 
					config->IsModelX32FullOrCompactOrProducerOrM32OrM32R() 		&&
					config->GetBool(parameter->GetAssignMembersIf()) 	&&
					config->GetUint(ACTIVE_PAGE) == (uint)X32_PAGE::CONFIG;

			if (isButtonPressed)
			{
				// Member Assign Mode (e.g. Mute Groups, DCA Groups)
				if (memberAssingMode)
				{
					// Bind Select button to destination Mixerparameter
					for (uint i = 0; i < 8; i++)
					{
						// Board L / InputSection
						uint chanIndex_L = surface->GetLoadedBank(OMCBankTarget::InputSection)->channelstrip[i]->select->mp_index;
						config->SurfaceBind(config->CalcSurfaceElementId(SurfaceElementId::BOARD_L_SELECT_1, i),
											MixerparameterAction::TOGGLE, parameter->GetAssignMembersTo(), chanIndex_L);

						if (config->IsModelX32FullOrM32())
						{
							// Board M / InputSection2
							uint chanIndex_M = surface->GetLoadedBank(OMCBankTarget::InputSection2)->channelstrip[i]->select->mp_index;
							config->SurfaceBind(config->CalcSurfaceElementId(SurfaceElementId::BOARD_M_SELECT_1, i),
												MixerparameterAction::TOGGLE, parameter->GetAssignMembersTo(), chanIndex_M);	
						}
						
						// not for Board R if DCA bank is loaded
						if (config->GetUint(BANKING_BUS) != (uint) OMCBankId::DCA)
						{
							// Board R / BusSection
							uint chanIndex_R = surface->GetLoadedBank(OMCBankTarget::BusSection)->channelstrip[i]->select->mp_index;
							config->SurfaceBind(config->CalcSurfaceElementId(SurfaceElementId::BOARD_R_SELECT_1, i),
												MixerparameterAction::TOGGLE, parameter->GetAssignMembersTo(), chanIndex_R);
						}

						// Master Fader
						config->SurfaceBind(SurfaceElementId::BOARD_R_SELECT_MAIN,
											MixerparameterAction::TOGGLE, parameter->GetAssignMembersTo(), int(X32_VCHANNEL_BLOCK::MAIN));

					}					
				}
				// Normal Mode
				else
				{
					switch (bindingParameterButton->mp_action)
					{
						case MixerparameterAction::REFRESH:
							config->Refresh(parameter_id, parameter_index);
							break;
						case MixerparameterAction::TOGGLE:
							config->Toggle(parameter_id, parameter_index);
							break;
						case MixerparameterAction::TOGGLE_SELECTED_CHANNEL:
							config->Toggle(parameter_id, parameter_index);
							break;
						case MixerparameterAction::PUSH:
						case MixerparameterAction::SET:
						case MixerparameterAction::SET_SELECTED_CHANNEL:
							config->Set(parameter_id, 1, parameter_index);
							break;
						case MixerparameterAction::SET_TO_INDEX:
							{
								// Set value to the bound index value.
								float value_to_set = bindingParameterButton->mp_index;

								if (secondbuttonPressed != 0)
								{
									// Second button was pressed, while holding the first one

									SurfaceBindingParameter* bindingParameterButtonOne = config->GetSurfaceBinding(buttonPressed->GetId());

									if (config->IsModelX32CompactOrProducerOrM32R())
									{
										// ######################################
										// Banking input section into bus section
										// ######################################
										if (bindingParameterButtonOne->mp_id == BANKING_INPUT && bindingParameterButton->mp_id == BANKING_INPUT)
										{
											// both buttons belong to input banking --> so load the bank into the bus section
											config->Set(BANKING_BUS, value_to_set, parameter_index);
										}
									} 
									else if (config->IsModelX32FullOrM32())
									{
										// TODO https://github.com/OpenMixerProject/OpenX32/issues/61

										// #######################################
										// Banking Channels 17-24 into bus section
										// #######################################
										if (bindingParameterButtonOne->mp_id == BANKING_INPUT && bindingParameterButton->mp_id == BANKING_INPUT)
										{
											// both buttons belong to input banking --> so load the bank into the bus section										
											config->Set(BANKING_BUS, value_to_set, parameter_index);
										}

									}
								}
								else
								{
									config->Set(parameter_id, value_to_set, parameter_index);
								}
							}
							break;
						case MixerparameterAction::CHANGE:
						case MixerparameterAction::CHANGE_SELECTED_CHANNEL:
						case MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL:
							config->Change(parameter_id, 1, parameter_index); // every button press does one stepsize "up"
							break;
						case MixerparameterAction::RESET:
						case MixerparameterAction::RESET_SELECTED_CHANNEL:
							config->Reset(parameter_id, parameter_index);
							break;
						case MixerparameterAction::CLEAR_SOLO:
                            config->Toggle(CLEAR_SOLO_COMMAND);
							break;
						case MixerparameterAction::CUSTOM:
						{
							Page* curent_page = pages[(X32_PAGE)config->GetUint(MP_ID::ACTIVE_PAGE)];
							curent_page->ChangeCustomButton(button->GetId());
						}
						case MixerparameterAction::DMX:
							// a button is pressed
							break;
						default:
							break;
					}
				}
			}
			else
			{
				// Button was released	

				// Member Assign Mode (e.g. Mute Groups)
				if (memberAssingMode)
				{
					// Reload current banking
					config->Refresh(BANKING_INPUT);
					config->Refresh(BANKING_BUS);
					surface->LoadMainFaderSurfaceBinding();
				}
				// Normal Mode
				else
				{
					switch (bindingParameterButton->mp_action)
					{
						case MixerparameterAction::PUSH:
							config->Set(parameter_id, 0, parameter_index);
							break;
						default:
							break;
					}
				}
			}
		}
		else if (buttonPressed && buttonPressed->GetId() == SurfaceElementId::TALK_A)
		{
			// DEBUG VKeyboard
			{
				String chan = config->GetParameter(CHANNEL_NAME)->GetString(config->GetUint(SELECTED_CHANNEL));

				for (uint i = 0; i < (chan.length() <= 8 ? chan.length() : 8); i++)
				{
					LcdData* data = new LcdData();
					uint textcount = 0;
					uint textIndex = 0;

					data->boardId = OMC_BOARD::X32_BOARD_R;
					data->color = (uint)X32_COLOR::WHITE | SURFACE_COLOR_INVERTED;
					data->lcdIndex = i;
					data->icon.icon = 0;
					data->icon.x = 0;
					data->icon.y = 0;

					textIndex++;

					data->texts[textIndex].text = String(chan[i]);
					data->texts[textIndex].size = 0x20;
					data->texts[textIndex].x = 45;
					data->texts[textIndex].y = 22;
					
					textcount = textIndex + 1;

					surface->SetLcd(data, textcount);
					delete data;
				}
			}
		}
		else
		{
			helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Button is not bound.");
		}
	}
	else if (command == 'e') // Encoder
	{
		// find encoder
		SurfaceElement* encoder = config->GetSurfaceElementEncoder(board, index);
		if (encoder == 0)
		{
			helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Encoder is not defined!");
			return; 
		}
		int8_t amount = 0;

		if (value > 0 && value < 128)
		{
			amount = (uint8_t)value;
		}
		else if (value > 128 && value < 256) 
		{
			amount = -(256 - (uint8_t)(value));
		}

		helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Encoder \"%s\" turned: \"%d\"",
			encoder->GetName().c_str(),
			amount
		);

		SurfaceBindingParameter* bindingParameterEncoder = config->GetSurfaceBinding(encoder->GetId());

		if (bindingParameterEncoder != 0 && bindingParameterEncoder->mp_action != MixerparameterAction::NONE)
		{
			MP_ID parameter_id = config->ParameterCalcId(bindingParameterEncoder);
			uint parameter_index = config->ParameterCalcIndex(bindingParameterEncoder);

			switch (bindingParameterEncoder->mp_action)
			{
				case MixerparameterAction::CHANGE:
				case MixerparameterAction::CHANGE_SELECTED_CHANNEL:
				case MixerparameterAction::CHANGE__MP_INDIRECT__SELECTED_CHANNEL:
					config->Change(parameter_id, amount, parameter_index);
					break;
				case MixerparameterAction::CUSTOM:
					{
						Page* curent_page = pages[(X32_PAGE)config->GetUint(MP_ID::ACTIVE_PAGE)];
						curent_page->ChangeCustomEncoder(encoder->GetId(), amount);
					}
					break;
				case MixerparameterAction::DMX:
					// an encoder is turned
					break;
				default:
					break;
			}
		}
		else
		{
			helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Encoder is not bound.");
		}

	}
	else 
	{
		helper->DEBUG_X32CTRL(DEBUGLEVEL_TRACE, "unknown message: %s\n",
			(String("Board: ") + String(board) + " Class: " + String(command) + " Index: " + String(index) + " Value: " + String(value)).c_str()
		);
	}
}





	// TODO: Implement on X32Core

	// if (isButtonPressed)
	// {
	// 		case X32_BTN_SCENE_SETUP:
	// 			if (config->IsModelX32Core()) {
	// 				state->x32core_lcdmode_setup = !state->x32core_lcdmode_setup;
	// 				if (state->x32core_lcdmode_setup) {
	// 					surface->SetLedByEnum(X32_BTN_SCENE_SETUP, 1);
	// 					lcdmenu->OnShow();
	// 				} else {
	// 					surface->SetLedByEnum(X32_BTN_SCENE_SETUP, 0);
	// 					// trigger switch to channel lcd
	// 					config->Refresh(SELECTED_CHANNEL);
	// 				}
	// 			}
	// 			break;
	// 		case X32_BTN_CHANNEL_ENCODER:
	// 			if (config->IsModelX32Core()) {
	// 				if (state->x32core_lcdmode_setup) {
	// 					lcdmenu->OnLcdEncoderPressed();
	// 				}
	// 			}
	// 			break;
	// }


//###########################################################################################################################
//# 
//#   #######   ######   ######        ######     ###    ##       ##       ########     ###     ######  ##     ##  ######  
//#  ##     ## ##    ## ##    ##      ##    ##   ## ##   ##       ##       ##     ##   ## ##   ##    ## ##    ##  ##    ## 
//#  ##     ## ##       ##            ##        ##   ##  ##       ##       ##     ##  ##   ##  ##       ##   ##   ##       
//#  ##     ##  ######  ##            ##       ##     ## ##       ##       ########  ##     ## ##       #####      ######  
//#  ##     ##       ## ##            ##       ######### ##       ##       ##     ## ######### ##       ##   ##         ## 
//#  ##     ## ##    ## ##    ##      ##    ## ##     ## ##       ##       ##     ## ##     ## ##    ## ##    ##  ##    ## 
//#   #######   ######   ######        ######  ##     ## ######## ######## ########  ##     ##  ######  ##     ##  ######  
//#
//###########################################################################################################################

void CtrlClient::OnOscSendToServerCallbackSet(void* arg, MP_ID parameterId, WString::String strValue, float floatValue, uint index)
{
	CtrlClient* ctrl = static_cast<CtrlClient*>(arg);
    ctrl->osc_client->UdpSendToServerSet(parameterId, strValue, floatValue, index);
}

void CtrlClient::OnOscSendToServerCallbackChange(void* arg, MP_ID parameterId, int amount, uint index)
{
	CtrlClient* ctrl = static_cast<CtrlClient*>(arg);
    ctrl->osc_client->UdpSendToServerChange(parameterId, amount, index);
}

void CtrlClient::OnOscSendToServerCallbackToogle(void* arg, MP_ID parameterId, uint index)
{
	CtrlClient* ctrl = static_cast<CtrlClient*>(arg);
    ctrl->osc_client->UdpSendToServerToogle(parameterId, index);
}

void CtrlClient::OnOscSendToServerCallbackReset(void* arg, MP_ID parameterId, uint index)
{
	CtrlClient* ctrl = static_cast<CtrlClient*>(arg);
    ctrl->osc_client->UdpSendToServerReset(parameterId, index);
}

//################################################################################
//# 
//#  ########   #######  ########  ##    ## ##       ########  ######   ######  
//#  ##     ## ##     ## ##     ##  ##  ##  ##       ##       ##    ## ##    ## 
//#  ##     ## ##     ## ##     ##   ####   ##       ##       ##       ##       
//#  ########  ##     ## ##     ##    ##    ##       ######    ######   ######  
//#  ##     ## ##     ## ##     ##    ##    ##       ##             ##       ## 
//#  ##     ## ##     ## ##     ##    ##    ##       ##       ##    ## ##    ## 
//#  ########   #######  ########     ##    ######## ########  ######   ######  
//# 
//################################################################################


// Key was pressed in the bodyless mode
void CtrlClient::SimulatorButton()
{
	#ifdef TARGET_PC_SDL2
	uint32_t key = lv_indev_get_key(keyboard);

	helper->Log("Simulatorbutton: %d\n", key);

	switch (key)
	{
		using enum X32_PAGE;

		case 49:
			// HOME
			config->Set(ACTIVE_PAGE, (uint)HOME);
			break;
		case 50:
			// METERS
			config->Set(ACTIVE_PAGE, (uint)METERS);
			break;
		case 51:
			config->Set(ACTIVE_PAGE, (uint)ROUTING);
			break;
		case 52:
			config->Set(ACTIVE_PAGE, (uint)LIBRARY);
			break;
		case 53:
			config->Set(ACTIVE_PAGE, (uint)EFFECTS);
			break;
		case 54:
			config->Set(ACTIVE_PAGE, (uint)SETUP);
			break;
		case 55:
			//ShowPage(MONITOR);
			break;
		case 56:
			//ShowPage(SCENES);
			break;
		case 40899:
			config->Toggle(DISPLAY_UTILITY);
			break;
		case 57:
			config->Toggle(DISPLAY_MUTE_GROUP);
			break;
		case 48:
			config->Set(ACTIVE_PAGE, (uint)DEBUG);
			break;
		case 17:
			config->Refresh(DISPLAY_UP);
			break;
		case 18:
			config->Refresh(DISPLAY_DOWN);
			break;
		case 20:
			config->Refresh(DISPLAY_LEFT);
			break;
		case 19:
			config->Refresh(DISPLAY_RIGHT);
			break;
		case 113: // Q
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x09, 1); // Encoder 1 up
			//ProcessSurface(X32_BOARD_MAIN, 'e', 0x0D, 1); // Encoder 1 up
			break;
		case 119: // W
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0A, 1); // Encoder 2 up
			//ProcessSurface(X32_BOARD_MAIN, 'e', 0x0E, 1); // Encoder 2 up
			break;
		case 101: // E
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0B, 1); // Encoder 3 up
			break;
		case 114: // R
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0C, 1); // Encoder 4 up
			break;
		case 116: // T
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0D, 1); // Encoder 5 up
			break;
		case 122: // Z
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0E, 1); // Encoder 6 up
			break;
		case 97: // A
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x09, 255); // Encoder 1 down
		 	break;
		case 115: // S
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0A, 255); // Encoder 2 down
			break;
		case 100: // D
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0B, 255); // Encoder 3 down
			break;
		case 102: // F
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0C, 255); // Encoder 4 down
			break;
		case 103: // G
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0D, 255); // Encoder 5 down
			break;
		case 104: // H
			ProcessSurface(X32_BOARD_MAIN, 'e', 0x0E, 255); // Encoder 6 down
			break;
		case 121: // Y
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x18 + 0x80); // Encoder Button 1 press
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x18); // Encoder Button 1 release
			break;
		case 120: // X
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x19 + 0x80); // Encoder Button 2 press
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x19); // Encoder Button 2 release
			break;
		case 99: // C
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1A + 0x80); // Encoder Button 3 press
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1A); // Encoder Button 3 release
			break;
		case 118: // V
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1B + 0x80); // Encoder Button 4 press
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1B); // Encoder Button 4 release
			break;
		case 98: // B
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1C + 0x80); // Encoder Button 5 press
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1C); // Encoder Button 5 release
			break;
		case 110: // N
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1D + 0x80); // Encoder Button 6 press
			ProcessSurface(X32_BOARD_MAIN, 'b', 0, 0x1D); // Encoder Button 6 release
			break;
		case 0xFFFFFF9F:
			config->Set(ACTIVE_PAGE, (uint)SCENES);
			break;
		case 46274: // Button REMOTE DAW
			// ProcessSurface(X32_BOARD_R, 'b', 0, 0x00 + 0x80);
			// ProcessSurface(X32_BOARD_R, 'b', 0, 0x00);
			ProcessSurface(X32_BOARD_L, 'f', 0, 0x000C);
			break;
	}
	#endif
}

}
