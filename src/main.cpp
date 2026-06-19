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

// includes for timer
#include <time.h>
#include <signal.h>

#include "defines.h"
#include "ctrl.h"
#include "version.h"

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


X32Ctrl* ctrl;
State* state;
CLI::App* app;

// LVGL
static lv_display_t *display;
#ifdef TARGET_PC_SDL2
static lv_indev_t *mouse;
static lv_indev_t *mouse_wheel;
static lv_indev_t *keyboard;
#endif

timer_t timerid_10ms;
struct sigevent sev_10ms;
struct itimerspec trigger_10ms;
uint8_t vtimercounter = 0;

void timer100msCallbackLvgl(_lv_timer_t* lv_timer) { 
	ctrl->Tick100ms();
}

void timer50msCallbackLvgl(_lv_timer_t* lv_timer) { 
	ctrl->Tick50ms();
}

void timer10msCallbackLvgl(_lv_timer_t* lv_timer) {
	ui_tick(); ctrl->Tick10ms();
}
void timer10msCallbackLinux(int timer) {
	
	ctrl->Tick10ms();

	// virtual timer for triggering every 50ms and 100ms
	vtimercounter++;
	if (vtimercounter == 5) {
		ctrl->Tick50ms();
	}
	if (vtimercounter >= 10) {
		ctrl->Tick100ms();
		vtimercounter = 0;
	}
}

void init10msTimer_NonGUI(void) {
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

const char * getenv_default(const char * name, const char * default_val)
{
    const char * value = getenv(name);
    return value ? value : default_val;
}


void guiInit(X32Config* config)
{
	lv_init();

	#ifdef TARGET_PC_SDL2

		printf("bodyless mode (Development Simulator) startet\n");
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
		
		const char * device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
		display = lv_linux_fbdev_create();

		if(display == NULL) {
			printf("could not create display!");
			return;
		}

		lv_linux_fbdev_set_file(display, device);	

	#endif

	#ifdef BUILD_DEBUG
	printf("lv_timer_create(timer10msCallbackLvgl, 10, NULL)\n");
	#endif
	lv_timer_create(timer10msCallbackLvgl, 10, NULL);

	#ifdef BUILD_DEBUG
	printf("lv_timer_create(timer50msCallbackLvgl, 50, NULL)\n");
	#endif
	lv_timer_create(timer50msCallbackLvgl, 50, NULL);

	#ifdef BUILD_DEBUG
	printf("lv_timer_create(timer100msCallbackLvgl, 100, NULL)\n");
	#endif
	lv_timer_create(timer100msCallbackLvgl, 100, NULL);

	// initialize GUI created by EEZ
	#ifdef BUILD_DEBUG
	printf("ui_init()\n");
	#endif
	ui_init();

	// InitPagesAndGUI() has to be called after ui_init()!
	#ifdef BUILD_DEBUG
	printf("ctrl->InitPagesAndGUI()\n");
	#endif
	ctrl->InitPagesAndGUI();

	// trigger first update of display header
	#ifdef BUILD_DEBUG
	printf("config->Refresh(SELECTED_CHANNEL)\n");
	#endif
	config->Refresh(SELECTED_CHANNEL);

	// trigger load of banks
	if (config->IsModelX32FullOrM32())
	{
		#ifdef BUILD_DEBUG
		printf("config->Set(BANKING_INPUT, (uint)X32BankId::CH1_16)\n");
		#endif
		config->Set(BANKING_INPUT, (uint)OMCBankId::CH1_16);
	}
	else if (config->IsModelX32CompactOrProducerOrM32R())
	{
		#ifdef BUILD_DEBUG
		printf("config->Set(BANKING_INPUT, (uint)X32BankId::CH1_8)\n");
		#endif
		config->Set(BANKING_INPUT, (uint)OMCBankId::CH1_8);
	}
	else if (config->IsModelWingCompact())
	{
		#ifdef BUILD_DEBUG
		printf("config->Set(BANKING_INPUT, (uint)OMCBankId::WING_1_12)\n");
		#endif
		config->Set(BANKING_INPUT, (uint)OMCBankId::WING_1_12);
	}
	
	if (config->IsModelX32FullOrCompactOrProducerOrM32OrM32R())
	{
		#ifdef BUILD_DEBUG
		printf("config->Refresh(BANKING_BUS)\n");
		#endif
		config->Refresh(BANKING_BUS);
	}

	// sync the Page
	#ifdef BUILD_DEBUG
	printf("config->Refresh(ACTIVE_PAGE)\n");
	#endif
	config->Refresh(ACTIVE_PAGE);

	// sync the Surface
	#ifdef BUILD_DEBUG
	printf("ctrl->syncSurface(true)\n");
	#endif
	ctrl->syncSurface(true);

	// LVGL loop
	#ifdef BUILD_DEBUG
	printf("starting LVGL loop\n");
	#endif
	uint32_t idle_time;
	while (1)
	{
		idle_time = lv_timer_handler();
		usleep(idle_time * 1000);
	}
}


void action_action_key(lv_event_t * e)
{
	#ifdef TARGET_PC_SDL2
	ctrl->SimulatorButton(lv_indev_get_key(keyboard));
	#endif
}

int main(int argc, char* argv[])
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
	app->add_option("--samplerate", "Set Samplerate to 44100 or 48000 kHz")
		->default_val<uint32_t>(48000)
		->check(CLI::IsMember(new set<uint32_t>{41000, 48000}));

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

	if (app->count("--version") > 0) {

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
		//model_str ="X32C";
		model_str ="WINGC";
	}
	else if (state->raspi)
	{
		model_str = "X32RACK";
	}

	X32Config* config = new X32Config(model_str, helper);
	

	config->Set(MP_ID::SAMPLERATE, app->get_option("--samplerate")->as<uint32_t>());

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
	ctrl = new X32Ctrl(basepar);

	helper->DEBUG_X32CTRL(DEBUGLEVEL_NORMAL, "ctrl->Init()");
	ctrl->Init();  // initialize the whole thing and load config

	if (config->IsModelX32Core())
	{
		// only necessary if LVGL is not used
		helper->Log("Starting Timers...\n");
		init10msTimer_NonGUI();

		helper->Log("Press Ctrl+C to terminate program.\n");
		while (1) {
			sleep(10); // Basically sleep forever :-) Timers do their job
		}
	}
	else
	{
		helper->Log("Initializing GUI...\n");
		guiInit(config); // initializes LVGL, FBDEV and starts endless loop
	}

    exit(0);
}