/*
							   =#%@@@@@###%@@@@%-
						   =*###+               :#@*
						+****.                      :%-
					  #++++             ############  :%-          @@@@@@@@@@@@
					.+===             ###======++==*##  *+       @@@*#%%#****#@@@
				   -=-+               #+======+=.*===*#         @@#**@.*@******%@
				  -=-+                ##======*  #====+#.      @@****@  @******@@
				  +:-                  ##+====#  #***==+#+   =@@**@@@@  @****%@@
				 =*=*                   -#*===#  ## #===+## @@@***@ @@  @@#*@@@
	  @@@@@@       ..                     ##+*#- ## #***==#= @@@@@@ @@   +@@@   @@@@@@@  @@@@@
	@@@    @@@                             ##= + ## ## #+==#- @@ @@ @@ = @@@    @@  @@  @@   @@
	@@      .@@#@@@@@@@  @@@ @@@ @@@@@@@@   .# # ## .= #-#++#= @ @  @@ * @*       @@*        @@
	@@       @@ @@    @@ @@@@@@@  @@   @@      # =#  = + *::=#   @  @+ *           -@@@   @@@=
	@@@    @@@  @@:   @@ @@       @@   @@      #  : .- : *::-#   @  +  #             @@ @@    @@
	  @@@@@@    @@@@@@@   @@@@@@  @@   @@@  =# # ## :+ #-#++#+ @ @  @@.* @@     @@@@@@  @@@@@@@@
				@@                         ##+ * ## ## #+==#+ @@@@@ @@ = @@@
				@@                        ##+= = ## #***==#+ @@***@ @@   #@@@
				   :                    :#*==+#: ## #===+## @@@***@ @@  @@#*@@#
				  .%+                  ##+====#  #***==+#=   +@@**@@@@  @****%@@
					%.                ##======*  #====*#  .*-  @@****@  @******@@
					 %=               #*======+: *===*#   +-=+  @@#**@ -@******@@
					  -@-             +##+=====+++=*##  ==-=-    @@@#%@@#****%@@@
						*@*             =###########  -===*        @@@@@@@@@@@@
						   @@%.                   .::=++*
							 .#@@%%*-.    .:=+**##***+.
								  .-+%%%%%%#***=-.

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

  Parts of this software with kind permission of Music Tribe. Thank you!
*/

#pragma once

#include "main.h"

#include "defines.h"
#include "version.h"
#include "omc.h"

#define DOCTEST_CONFIG_IMPLEMENT // we start doctest within our own main()
#include "../lib_ext/doctest/doctest/doctest.h"

// including the GUI built by EEZ-Studio
#include "eez/actions.h"
// #include "eez/fonts.h"
// #include "eez/images.h"
#include "eez/screens.h"
// #include "eez/structs.h"
// #include "eez/styles.h"
#include "eez/ui.h"
// #include "eez/vars.h"




namespace OMC
{
	OpenMixerControl* omc;
	State* state;
	CLI::App* app;

	timer_t timerid_10ms;
	struct sigevent sev_10ms;
	struct itimerspec trigger_10ms;
	uint8_t vtimercounter = 0;

	void timer1000msCallbackLvgl(_lv_timer_t* lv_timer) { 
		omc->Tick1000ms();
	}

	void timer100msCallbackLvgl(_lv_timer_t* lv_timer) { 
		omc->Tick100ms();
	}

	void timer50msCallbackLvgl(_lv_timer_t* lv_timer) { 
		omc->Tick50ms();
	}

	void timer10msCallbackLvgl(_lv_timer_t* lv_timer) {
		ui_tick(); omc->Tick10ms();
	}
	void timer10msCallbackLinux(int timer) {
		
		omc->Tick10ms();

		// virtual timer for triggering every 50ms and 100ms
		vtimercounter++;
		if (vtimercounter % 5 == 0)
		{
			omc->Tick50ms();
		}
		if (vtimercounter % 10 == 0)
		{
			omc->Tick100ms();
		}
		if (vtimercounter >= 100)
		{
			omc->Tick1000ms();
			vtimercounter = 0;
		}
	}

	void init10msTimer_NonGUI() {
		// Set up the signal handler
		struct sigaction sa;
		sa.sa_handler = timer10msCallbackLinux;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
			perror("sigaction");
		}

		// Set up the sigevent structure for the timer
		sev_10ms.sigev_notify = SIGEV_SIGNAL;
		sev_10ms.sigev_signo = SIGRTMIN;
		sev_10ms.sigev_value.sival_ptr = &timerid_10ms;

		// Create the timer
		if (timer_create(CLOCK_REALTIME, &sev_10ms, &timerid_10ms) == -1) {
			perror("timer_create");
		}
		
		trigger_10ms.it_value.tv_sec = 0;
		trigger_10ms.it_value.tv_nsec = TIMER_10MS;
		trigger_10ms.it_interval.tv_sec = 0;
		trigger_10ms.it_interval.tv_nsec = TIMER_10MS;

		// Arm the timer
		if (timer_settime(timerid_10ms, 0, &trigger_10ms, NULL) == -1) {
			perror("timer_settime");
		}
	}

	void LoadRoutingDefault(Config* config)
	{
		// FPGA	

		// XLR-inputs 1-32 -> DSP-inputs 1-32
		for (uint8_t ch = 0; ch < 32; ch++) {
			config->Set(ROUTING_FPGA, ch + 1, ch + 72);
		}
		// AUX-inputs 1-8 -> DSP-inputs 33-40
		for (uint8_t ch = 0; ch < 8; ch++) {
			config->Set(ROUTING_FPGA, ch + 65, ch + 104);
		}

		// ---------------------------------------------

		// DSP Return 1-16 -> XLR-outputs 1-16
		for (uint8_t ch = 0; ch < 16; ch++) {
			config->Set(ROUTING_FPGA, ch + 72 + 1, ch);
		}
		// DSP Return 17-32 -> P16-output 1-16
		for (uint8_t ch = 0; ch < 16; ch++) {
			config->Set(ROUTING_FPGA, ch + 72 + 16 + 1, ch + 16);
		}
		// DSP Return 33-40 -> AUX-outputs 1-6 + ControlRoom L/R
		for (uint8_t ch = 0; ch < 8 ; ch++) {
			config->Set(ROUTING_FPGA, ch + 72 + 32 + 1, ch + 64);
		}

		// ---------------------------------------------

		// XLR-inputs 1-32 -> Card 1-32
		for (uint8_t ch = 0; ch < 32; ch++) {
			config->Set(ROUTING_FPGA, ch + 1, ch + 32);
		}

		// DSP1

		for (uint8_t i = 0; i < 40; i++)
		{
			// DSP1 Routing Sources
			//
			// 0:       OFF
			// 1..32:   Input 1-32 (from FPGA)
			// 33..38:  AuxIn 1-6 (from FPGA)
			// 39:      Talkback intern (from FPGA)
			// 40:      Talkback extern (from FPGA)
			// 41..56:  MixBus 1-16
			// 57..62:  Matrix 1-6
			// 63:      MainL
			// 64:      MainR
			// 65:      MainSub
			// 66:      Monitor L
			// 67:      Monitor R
			// 68:      TalkBack
			// 69..92:  DSP2 Return 1-24 (from DSP2)

			// connect FPGA2DSP-Source 1-40 to all 40 Mixing Channels (1-32 + AUX 1-8)
			config->Set(ROUTING_DSP_INPUT, DSP_BUF_IDX_DSPCHANNEL + i, i);
			config->Set(ROUTING_DSP_INPUT_TAPPOINT, to_underlying(DSP_TAP::INPUT), i);
		}

		// connect MainLeft on even and MainRight on odd channels as PostFader
		for (uint8_t i = 0; i < 40; i++)
		{
			// connect MainLeft on even and MainRight on odd channels as PostFader
			config->Set(ROUTING_DSP_OUTPUT, DSP_BUF_IDX_MAINLEFT + (i % 2), i);
			config->Set(ROUTING_DSP_OUTPUT_TAPPOINT, to_underlying(DSP_TAP::POST_FADER), i);
		}
		// connect mixbus-channels 1-8 to DSP2-FX-Channels 1-8 as PostFader
		for (uint8_t i = 0; i < 8; i++)
		{
			config->Set(ROUTING_DSP_OUTPUT, DSP_BUF_IDX_MIXBUS + i, 40 + i);
			config->Set(ROUTING_DSP_OUTPUT_TAPPOINT, to_underlying(DSP_TAP::POST_FADER), 40 + i);
		}

		// connect MainLeft to RTA
		uint indexRTA = (MAX_DSP1_TO_FPGA_CHANNELS + MAX_DSP1_TO_DSP2_CHANNELS) - 1;
		config->Set(ROUTING_DSP_OUTPUT, DSP_BUF_IDX_MAINLEFT, indexRTA) ;
		config->Set(ROUTING_DSP_OUTPUT_TAPPOINT, to_underlying(DSP_TAP::POST_FADER), indexRTA);
	}

	void LoadVChannelLayout(Config* config)
	{
		// Channel 1-32
		for (int i=0; i<=31; i++)
		{
			config->Set(CHANNEL_COLOR, SURFACE_COLOR_YELLOW, i);
		}

		// AUX 1-6 / USB
		for (uint8_t i=0; i<=7;i++)
		{
			uint8_t index = (uint)X32_VCHANNEL_BLOCK::AUX + i;

			config->Set(CHANNEL_NAME, String("AUX") + String(i+1), index);
			config->Set(CHANNEL_COLOR, SURFACE_COLOR_GREEN, index);
		}

		// FX Returns 1-8
		for (uint i=0; i<=7;i++)
		{
			uint8_t index = (uint)X32_VCHANNEL_BLOCK::FXRET + i;
			
			config->Set(CHANNEL_NAME, String("FX RET") + String(i+1), index);
			config->Set(CHANNEL_COLOR, SURFACE_COLOR_BLUE, index);
		}

		// Bus 1-16
		for (uint8_t i=0; i<=15;i++)
		{
			uint8_t index = (uint)X32_VCHANNEL_BLOCK::BUS + i;
		
			config->Set(CHANNEL_NAME, String("BUS") + String(i+1), index);
			config->Set(CHANNEL_COLOR, SURFACE_COLOR_CYAN, index);
		}

		// Matrix 1-6 / Special / SUB
		for (uint8_t i=0; i<=7;i++)
		{
			uint8_t index = (uint)X32_VCHANNEL_BLOCK::MATRIX + i;
			
			if(i <=5)
			{
				config->Set(CHANNEL_NAME, String("MATRIX") + String(i+1), index);
				config->Set(CHANNEL_COLOR, SURFACE_COLOR_PINK, index);
			}
			else if (i == 6)
			{
				config->Set(CHANNEL_NAME, String("SPECIAL"), index);
				config->Set(CHANNEL_COLOR, SURFACE_COLOR_RED, index);
			}
			else if (i == 7)
			{
				config->Set(CHANNEL_NAME, String("M/C"), index);
				config->Set(CHANNEL_COLOR, SURFACE_COLOR_RED, index);
			};
		}

		// DCA 1-8
		for (uint8_t i=0; i<=7;i++)
		{
			uint8_t index = (uint)X32_VCHANNEL_BLOCK::DCA + i;

			config->Set(CHANNEL_NAME, String("DCA") + String(i+1), index);
			config->Set(CHANNEL_COLOR, SURFACE_COLOR_PINK, index);
		}

		// Main Channel
		{
			config->Set(CHANNEL_NAME, String("Main"),(uint)X32_VCHANNEL_BLOCK::MAIN);
			config->Set(CHANNEL_COLOR, SURFACE_COLOR_WHITE, (uint)X32_VCHANNEL_BLOCK::MAIN);
		}
	}



	int startup(int argc, char* argv[])
	{
		// run Doctest if "--test" is the first commandline argument
		if (argc > 1 && !strcmp(argv[1], "--test"))
		{
			// remove "--test" commandline, as is would confuse doctest
			argv[1] = "";

			doctest::Context context;
			context.applyCommandLine(argc, argv);

			int res = context.run(); // run

			return res; // propagate the result of the tests and exit
		}


		srand(time(NULL));
		state = new State();
		Helper* helper = new Helper();

		app = new CLI::App();
		app->description("Open Mixer Control");
		argv = app->ensure_utf8(argv);

		// Command line options
		app->add_option("--client", "Run as Client and connect to IP")
			->option_text("127.0.0.1");
		app->add_flag("--osc-doc", "Print OSC-Documentation")
			->configurable(false);
		app->add_flag("--version", "Get the version number, builddate and a nice logo")
			->configurable(false);
		app->add_flag("-p,--print", "Print configuration and exit")
			->configurable(false);
		app->add_option("--L", "Bitstream file for the Lattice FPGA")
			->option_text("FILE")
			//->default_str("lattice.bit")
			->configurable(false);
		app->add_option("--X", "Bitstream file for the Xilinx FPGA")
			->option_text("FILE")
			//->default_str("xilinx.bit")
			->configurable(false);
		app->add_option("--D1", "Bitstream file for the DSP1")
			->option_text("FILE")
			//->default_str("dsp1.ldr")
			->configurable(false);
		app->add_option("--D2", "Bitstream file for the DSP2")
			->option_text("FILE")
			//->default_str("dsp2.ldr")
			->configurable(false);

		// debugging commandline option	
		app->add_flag("-b,--bodyless", state->bodyless, "Enables a special mode to run omc in a different enviroment than a X32 mixer.")
				->configurable(false)
				->group("Debug");

		app->add_flag("-r,--raspi", state->raspi, "Enables Raspi mode.")
				->configurable(false)
				->group("Debug");

		vector<string> debug_parameters;
		app->add_option("-d,--debug", debug_parameters, "Prints debugging information to stdout. You can specify one or multiple of the following flags: ADDA DMX DSP1 DSP2 FPGA FX GUI INI MIXER OSC STATE SPI SURFACE TIMER UART X32CTRL")
				->configurable(false)
				->group("Debug")
				->expected(1,-1)
				->option_text("FLAG FLAG ...");

		bool verbose = false;
		bool trace = false;
		const char* catDebug = "Debug";
		const char* catDebugSurface = "Debug Surface";
		app->add_flag("--verbose", verbose, "Print more debug messages")
			->configurable(false)
			->group(catDebug)
			->expected(0,1);

		app->add_flag("--trace", trace, "Print all possible debug messages")
			->configurable(false)
			->group(catDebug)
			->expected(0,1);

		app->add_flag("--fpga-spi-speed", state->fpga_spi_speed, "SPI clockrate for bitstream loading and normal data transfer to/from FPGA")
			->configurable(false)
			->group(catDebug)
			->expected(0,1)
			->default_val<uint32_t>(SPI_FPGA_SPEED_HZ);

		app->add_flag("--dsps-spi-config-speed", state->dsp_spi_config_speed, "SPI clockrate for bitstream loading to DSPs")
			->configurable(false)
			->group(catDebug)
			->expected(0,1)
			->default_val<uint32_t>(SPI_DSP_CONF_SPEED_HZ);

		app->add_flag("--dsps-spi-speed", state->dsp_spi_speed, "SPI clockrate for normal data transfer to/from DSPs")
			->configurable(false)
			->group(catDebug)
			->expected(0,1)
			->default_val<uint32_t>(SPI_DSP_SPEED_HZ);

		app->add_flag("--dsps-disable-activity-light", state->dsp_disable_activity_light, "Disable DSPs activity light via SPI switching command")
			->configurable(false)
			->group(catDebug)
			->expected(0,1);

		app->add_flag("--dsps-disable-readout", state->dsp_disable_readout, "Disable DSPs readout")
			->configurable(false)
			->group(catDebug)
			->expected(0,1);

		app->add_flag("--surface-disable-lcd-update", state->surface_disable_lcd_update, "Disable LCD update")
			->configurable(false)
			->group(catDebugSurface)
			->expected(0,1);

		app->add_flag("--surface-disable-meter-update", state->surface_disable_meter_update, "Disable VU-Meter update")
			->configurable(false)
			->group(catDebugSurface)
			->expected(0,1);
		
		app->get_config_formatter_base()->quoteCharacter('"', '"');

		CLI11_PARSE(*app, argc, argv);

		if(app->get_option("--print")->as<bool>()) {
			std::cout << app->config_to_str(true, false);
			return 0;
		}

		if (app->count("--version") > 0)
		{

			printf("  ____                   __  __ _                _____            _             _ \n");
			printf(" / __ \\                 |  \\/  (_)              / ____|          | |           | |\n");
			printf("| |  | |_ __   ___ _ __ | \\  / |___  _____ _ __| |     ___  _ __ | |_ _ __ ___ | |\n");
			printf("| |  | | '_ \\ / _ \\ '_ \\| |\\/| | \\ \\/ / _ \\ '__| |    / _ \\| '_ \\| __| '__/ _ \\| |\n");
			printf("| |__| | |_) |  __/ | | | |  | | |>  <  __/ |  | |___| (_) | | | | |_| | | (_) | |\n");
			printf(" \\____/| .__/ \\___|_| |_|_|  |_|_/_/\\_\\___|_|   \\_____\\___/|_| |_|\\__|_|  \\___/|_|\n");
			printf("       | |                                                                        \n");
			printf("       |_|                                                                        \n");
			printf("\n");
			printf("%s build on %s %s\n\n%s\n", GIT_VERSION, __DATE__, __TIME__, OMC_URL);
			return 0;
		}

		//##################################################################################
		//#
		//# 	Read hardware configuration, before any more classes are constructed!
		//#
		//##################################################################################

		// first try to find what we are: Fullsize, Compact, Producer, Rack or Core
		helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "Reading hardware config...");
		char model[12];
		char serial[15];
		char date[16];
		char cfg[5];

		#ifdef TARGET_XM32
		
		helper->ReadConfig("/etc/x32.conf", "MDL=", model, 12);
		helper->ReadConfig("/etc/x32.conf", "SN=", serial, 15);
		helper->ReadConfig("/etc/x32.conf", "DATE=", date, 16);
		helper->ReadConfig("/etc/x32.conf", "CFG", cfg, 5);
		
		helper->Log("Detected model: %s with Serial %s built on %s\n", model, serial, date);

		#elifdef TARGET_WING

		// DEBUG
		strcpy(model, "WINGC"); // asume WING Compact for now
		strcpy(serial, "DEV_NO_SERIAL");
		strcpy(date, "DEV_NO_DATE");

		// TODO
		// helper->ReadConfig("/etc/wing.conf", "MDL=", model, 12);
		// helper->ReadConfig("/etc/wing.conf", "SN=", serial, 15);
		// helper->ReadConfig("/etc/wing.conf", "DATE=", date, 16);
		// helper->ReadConfig("/etc/wing.conf", "CFG", cfg, 5);

		#endif

		String model_str = String(model);
		if (state->bodyless)
		{
			model_str ="X32C";
			//model_str ="WINGC";
		}
		else if (state->raspi)
		{
			model_str = "X32RACK";
		}

		bool runAsClient = app->count("--client") > 0;
		Config* config = new Config(model_str, helper, runAsClient);

		// #### CONFIG ############################################################################
		//
		if(!config->LoadConfig(0))
		{
			helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "Load Default Routing");
			LoadRoutingDefault(config);

			helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "Load Default Channel Layout");
			LoadVChannelLayout(config);

			helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "Load Default Banks");
			// Input
			if (config->IsModelX32FullOrM32())
			{
				config->Set(BANKING_INPUT, (uint)OMCBankId::CH1_16);
			}
			else if (config->IsModelX32CompactOrProducerOrM32R())
			{
				config->Set(BANKING_INPUT, (uint)OMCBankId::CH1_8);
			}
			else if (config->IsModelWingCompact())
			{
				config->Set(BANKING_INPUT, (uint)OMCBankId::WING_1_12);
			}
			
			// Buses
			if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32R())
			{
				config->Set(BANKING_BUS, (uint)OMCBankId::DCA);
			}

			helper->DEBUG_INI(DEBUGLEVEL_NORMAL, "no default configfile found, creating one");
			config->Save(0);
		}

		//
		// #### CONFIG #############################################################################

		// Print OSC-Doku
		if (app->count("--osc-doc") > 0)
		{
			config->PrintOscDoc();
			exit(0);
		}

		if (debug_parameters.size() > 0) {
			for(uint8_t i=0; i<debug_parameters.size(); i++) {
				if (debug_parameters[i] == "ALL") { helper->SetDebugAll(); }
				if (debug_parameters[i] == "ADDA") { helper->DEBUG_ADDA(true); }
				if (debug_parameters[i] == "DMX") { helper->DEBUG_DMX(true); }
				if (debug_parameters[i] == "DSP1") { helper->DEBUG_DSP1(true); }
				if (debug_parameters[i] == "DSP2") { helper->DEBUG_DSP2(true); }
				if (debug_parameters[i] == "FPGA") { helper->DEBUG_FPGA(true); }
				if (debug_parameters[i] == "GUI") { helper->DEBUG_GUI(true); }
				if (debug_parameters[i] == "MIXER") { helper->DEBUG_MIXER(true); }
				if (debug_parameters[i] == "SPI") { helper->DEBUG_SPI(true); }
				if (debug_parameters[i] == "SURFACE") { helper->DEBUG_SURFACE(true); }
				if (debug_parameters[i] == "UART") { helper->DEBUG_UART(true); }
				if (debug_parameters[i] == "X32CTRL") { helper->DEBUG_X32CTRL(true); }
				if (debug_parameters[i] == "OSC") { helper->DEBUG_OSC(true); }
				if (debug_parameters[i] == "INI") { helper->DEBUG_INI(true); }
				if (debug_parameters[i] == "STATE") { helper->DEBUG_STATE(true); }
				if (debug_parameters[i] == "TIMER") { helper->DEBUG_TIMER(true); }
				if (debug_parameters[i] == "FX") { helper->DEBUG_FX(true); }
			}

			if (trace  == true) {
				helper->SetDebugLevel(DEBUGLEVEL_TRACE);
			} else if (verbose == true) {
				helper->SetDebugLevel(DEBUGLEVEL_VERBOSE);
			} else {
				helper->SetDebugLevel(DEBUGLEVEL_NORMAL);
			}
		} else {
			helper->SetDebugLevel(DEBUGLEVEL_OFF);
		}

		X32BaseParameter* basepar = new X32BaseParameter(app, config, state, helper);

		omc = new OpenMixerControl(basepar);

		helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "omc->Init()");
		omc->Init();  // initialize the whole thing

		exit(0);
	}
}

void action_action_key(lv_event_t * e)
{
	#ifdef TARGET_PC_SDL2
	OMC::omc->SimulatorButton();
	#endif
}

int main(int argc, char* argv[])
{
	return OMC::startup(argc, argv);
}