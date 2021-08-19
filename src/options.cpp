#include "options.h"

#include <ini.h>
#include <type_traits>

#include "audio.h"
#include "debugger.h"
#include "symbols.h"
#include "version.h"

options       Options;
const options Default_options;
char          Options_base_path[PATH_MAX];
char          Options_ini_path[PATH_MAX];

mINI::INIStructure Cmdline_ini;
mINI::INIStructure Inifile_ini;

static void usage()
{
	printf("%s %s (%s)\n", VER_TITLE, VER_NUM, VER_NAME);
	printf("Portions (C)2019,2020 Michael Steil et al.\n");
	printf("Portions (C)2021 Stephen Horn et al.\n");
	printf("All rights reserved. License: 2-clause BSD\n\n");

	printf("Usage: x16emu [option] ...\n\n");

	printf("-abufs <number of audio buffers>\n");
	printf("\tIs provided for backward-compatibility with x16emu toolchains,\n");
	printf("\tbut is non-functional in Box16.\n ");

	printf("-bas <app.txt>\n");
	printf("\tInject a BASIC program in ASCII encoding through the\n");
	printf("\tkeyboard.\n");

	printf("-debug <address>\n");
	printf("\tSet a breakpoint in the debugger\n");

	printf("-dump {C|R|B|V}...\n");
	printf("\tConfigure system dump: (C)PU, (R)AM, (B)anked-RAM, (V)RAM\n");
	printf("\tMultiple characters are possible, e.g. -dump CV ; Default: RB\n");

	printf("-echo [{iso|raw}]\n");
	printf("\tPrint all KERNAL output to the host's stdout.\n");
	printf("\tBy default, everything but printable ASCII characters get\n");
	printf("\tescaped. \"iso\" will escape everything but non-printable\n");
	printf("\tISO-8859-1 characters and convert the output to UTF-8.\n");
	printf("\t\"raw\" will not do any substitutions.\n");
	printf("\tWith the BASIC statement \"LIST\", this can be used\n");
	printf("\tto detokenize a BASIC program.\n");

	printf("-hypercall_path <path>\n");
	printf("\tSet the base path for hypercalls (effectively, the current working directory when no SD card is attached).\n");

	printf("-geos\n");
	printf("\tLaunch GEOS at startup.\n");

	printf("-gif <file.gif>[,wait]\n");
	printf("\tRecord a gif for the video output.\n");
	printf("\tUse ,wait to start paused.\n");

	printf("-help\n");
	printf("\tPrint this message and exit.\n");

	printf("-keymap <keymap>\n");
	printf("\tEnable a specific keyboard layout decode table.\n");

	printf("-log {K|S|V}...\n");
	printf("\tEnable logging of (K)eyboard, (S)peed, (V)ideo.\n");
	printf("\tMultiple characters are possible, e.g. -log KS\n");

	printf("-nobinds\n");
	printf("\tDisable most emulator keyboard shortcuts.\n");

	printf("-nosound\n");
	printf("\tDisables audio. Incompatible with -sound.\n");

	printf("-nvram <nvram.bin>\n");
	printf("\tSpecify NVRAM image. By default, the machine starts with\n");
	printf("\tempty NVRAM and does not save it to disk.\n");

	printf("-prg <app.prg>[,<load_addr>]\n");
	printf("\tLoad application from the local disk into RAM\n");
	printf("\t(.PRG file with 2 byte start address header)\n");
	printf("\tThe override load address is hex without a prefix.\n");

	printf("-quality {nearest|linear|best}\n");
	printf("\tScaling algorithm quality\n");

	printf("-ram <ramsize>\n");
	printf("\tSpecify banked RAM size in KB (8, 16, 32, ..., 2048).\n");
	printf("\tThe default is 512.\n");

	printf("-rom <rom.bin>\n");
	printf("\tOverride KERNAL/BASIC/* ROM file.\n");

	printf("-rtc\n");
	printf("\tSet the real-time-clock to the current system time and date.\n");

	printf("-run\n");
	printf("\tStart the -prg/-bas program using RUN or SYS, depending\n");
	printf("\ton the load address.\n");

	printf("-scale {1|2|3|4}\n");
	printf("\tScale output to an integer multiple of 640x480\n");

	printf("-sdcard <sdcard.img>\n");
	printf("\tSpecify SD card image (partition map + FAT32)\n");
	
	printf("-sound <output device>\n");
	printf("\tSet the output device used for audio emulation. Incompatible with -nosound.\n");

	printf("-stds\n");
	printf("\tLoad standard (ROM) symbol files\n");

	printf("-sym <filename>\n");
	printf("\tLoad a symbols file\n");

	printf("-test {0, 1, 2, 3}\n");
	printf("\tImmediately invoke the TEST command with the provided test number.\n");

	printf("-version\n");
	printf("\tPrint additional version information the emulator and ROM.\n");

	printf("-warp\n");
	printf("\tEnable warp mode, run emulator as fast as possible.\n");
	
	printf("-wav <file.wav>[{,wait|,auto}]\n");
	printf("\tRecord a wav for the audio output.\n");
	printf("\tUse ,wait to start paused.\n");
	printf("\tUse ,auto to start paused, but begin recording once a non-zero audio signal is detected.\n");
	printf("\tPOKE $9FB5,2 to start recording.\n");
	printf("\tPOKE $9FB5,1 to capture a single frame.\n");
	printf("\tPOKE $9FB5,0 to pause.\n");
	
	printf("-ymirq\n");
	printf("\tEnable the YM2151's IRQ generation.\n");
	
	printf("-ymstrict\n");
	printf("\tEnable strict enforcement of YM behaviors.\n");
	printf("\n");

	exit(1);
}

// This must match the KERNAL's set!
static constexpr const char *keymaps[] = {
	"en-us",
	"en-gb",
	"de",
	"nordic",
	"it",
	"pl",
	"hu",
	"es",
	"fr",
	"de-ch",
	"fr-be",
	"pt-br",
};

void usage_ram()
{
	printf("The following ram are supported:\n");
	for (int cmp = 8; cmp <= 2048; cmp *= 2) {
		printf("\t%d\n", cmp);
	}
	exit(1);
}

void usage_keymap()
{
	printf("The following keymaps are supported:\n");
	for (size_t i = 0; i < sizeof(keymaps) / sizeof(*keymaps); i++) {
		printf("\t%s\n", keymaps[i]);
	}
	exit(1);
}

static void parse_cmdline(mINI::INIStructure &ini, int argc, char **argv)
{
	argc--;
	argv++;

	while (argc > 0) {
		if (argv[0][0] != '-') {
			usage();
		}
		char *v = argv[0] + 1;
		if (!strcmp(argv[0], "-abufs")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["abufs"] = argv[0];

			argc--;
			argv++;

		} else if (!strcmp(argv[0], "-bas")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["bas"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-debug")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			// Add a breakpoint
			uint32_t bp = strtol(argv[0], NULL, 16);
			debugger_add_breakpoint((uint16_t)(bp & 0xfffF), (uint8_t)(bp >> 16));

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-dump")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["dump"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-echo")) {
			argc--;
			argv++;
			if (argc && argv[0][0] != '-') {

				ini["main"]["echo"] = argv[0];

				argc--;
				argv++;
			} else {
				ini["main"]["echo"] = "cooked";
			}
		} else if (!strcmp(argv[0], "-hypercall_path")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["hypercall_path"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-geos")) {
			argc--;
			argv++;

			ini["main"]["geos"] = "true";

		} else if (!strcmp(argv[0], "-gif")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["gif"] = argv[0];

			argv++;
			argc--;
		} else if (!strcmp(argv[0], "-help")) {
			argc--;
			argv++;

			usage();
		} else if (!strcmp(argv[0], "-keymap")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage_keymap();
			}

			ini["main"]["keymap"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-log")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["log"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-nobinds")) {
			argc--;
			argv++;
			ini["main"]["nobinds"] = "true";

		} else if (!strcmp(argv[0], "-nosound")) {
			argc--;
			argv++;

			ini["main"]["nosound"] = "true";

		} else if (!strcmp(argv[0], "-nvram")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}
			ini["main"]["nvram"] = argv[0];
			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-prg")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["prg"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-quality")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["quality"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-ram")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["ram"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-rom")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["rom"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-rtc")) {
			argc--;
			argv++;
			ini["main"]["rtc"] = "true";

		} else if (!strcmp(argv[0], "-run")) {
			argc--;
			argv++;

			ini["main"]["run"] = "true";

		} else if (!strcmp(argv[0], "-scale")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["scale"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-sdcard")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["sdcard"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-sound")) {
			argc--;
			argv++;

			ini["main"]["sound"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-stds")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] != '-') {
				usage();
			}

			ini["main"]["stds"] = "true";

		} else if (!strcmp(argv[0], "-sym")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			// Add a symbols file
			symbols_load_file(argv[0]);

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-test")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["test"] = argv[0];

			argc--;
			argv++;
		} else if (!strcmp(argv[0], "-version")) {
			printf("%s %s", VER_NUM, VER_NAME);
			argc--;
			argv++;
			exit(0);

		} else if (!strcmp(argv[0], "-warp")) {
			argc--;
			argv++;

			ini["main"]["warp"] = "true";

		} else if (!strcmp(argv[0], "-wav")) {
			argc--;
			argv++;
			if (!argc || argv[0][0] == '-') {
				usage();
			}

			ini["main"]["wav"] = argv[0];

			argv++;
			argc--;
		} else if (!strcmp(argv[0], "-ymirq")) {
			argc--;
			argv++;
			ini["main"]["ymirq"] = "true";

		} else if (!strcmp(argv[0], "-ymstrict")) {
			argc--;
			argv++;
			ini["main"]["ymstrict"] = "true";

		} else {
			usage();
		}
	}
}

static void fixup_directory(char *directory, size_t len)
{
	// If the path specified ended in a directory separator, remove it.
	if (len > 0) {
		const size_t last_char = len - 1;
		const char   term      = directory[last_char];
#if defined(WIN32)
		if (term == '\\' || term == '/') {
			directory[last_char] = '\0';
		}
#else
		if (term == '/') {
			directory[last_char] = '\0';
		}
#endif
	}
}

static void fixup_directory(char *directory)
{
	fixup_directory(directory, strlen(directory));
}

static void apply_directory(char *directory, const std::string &src)
{
	strcpy(directory, src.c_str());
	fixup_directory(directory, src.length());
}

static void set_options(mINI::INIStructure &ini)
{
	if (ini["main"].has("rom")) {
		strcpy(Options.rom_path, ini["main"]["rom"].c_str());
	}

	if (ini["main"].has("ram")) {
		int  kb    = atoi(ini["main"]["ram"].c_str());
		bool found = false;
		for (int cmp = 8; cmp <= 2048; cmp *= 2) {
			if (kb == cmp) {
				found = true;
				break;
			}
		}
		if (!found) {
			usage_ram();
		}
		Options.num_ram_banks = kb / 8;
	}

	if (ini["main"].has("hypercall_path")) {
		apply_directory(Options.hyper_path, ini["main"]["hypercall_path"]);
	}

	if (ini["main"].has("keymap")) {
		bool found = false;
		for (uint8_t i = 0; i < sizeof(keymaps) / sizeof(*keymaps); i++) {
			if (!strcmp(ini["main"]["keymap"].c_str(), keymaps[i])) {
				found          = true;
				Options.keymap = i;
				break;
			}
		}
		if (!found) {
			usage_keymap();
		}
	}

	if (ini["main"].has("prg")) {
		strcpy(Options.prg_path, ini["main"]["prg"].c_str());
	}

	if (ini["main"].has("run")) {
		if (!strcmp(ini["main"]["run"].c_str(), "true")) {
			Options.run_after_load = true;
		}
	}

	if (ini["main"].has("bas")) {
		strcpy(Options.bas_path, ini["main"]["bas"].c_str());
	}

	if (ini["main"].has("geos")) {
		if (!strcmp(ini["main"]["geos"].c_str(), "true")) {
			Options.run_geos = true;
		}
	}

	if (ini["main"].has("test")) {
		Options.test_number = atoi(ini["main"]["test"].c_str());
		Options.run_test    = Options.test_number >= 0;
	}

	if (ini["main"].has("nvram")) {
		strcpy(Options.nvram_path, ini["main"]["nvram"].c_str());
	}

	if (ini["main"].has("sdcard")) {
		strcpy(Options.sdcard_path, ini["main"]["sdcard"].c_str());
	}

	if (ini["main"].has("warp")) {
		if (!strcmp(ini["main"]["warp"].c_str(), "true")) {
			Options.warp_factor = 1;
		}
	}

	if (ini["main"].has("echo")) {
		char const *echo_mode = ini["main"]["echo"].c_str();
		if (!strcmp(echo_mode, "raw")) {
			Options.echo_mode = ECHO_MODE_RAW;
		} else if (!strcmp(echo_mode, "iso")) {
			Options.echo_mode = ECHO_MODE_ISO;
		} else if (!strcmp(echo_mode, "cooked")) {
			Options.echo_mode = ECHO_MODE_COOKED;
		} else if (!strcmp(echo_mode, "none")) {
			Options.echo_mode = ECHO_MODE_NONE;
		} else {
			usage();
		}
	}

	if (ini["main"].has("log")) {
		for (const char *p = ini["main"]["log"].c_str(); *p; p++) {
			switch (tolower(*p)) {
				case 'k':
					Options.log_keyboard = true;
					break;
				case 's':
					Options.log_speed = true;
					break;
				case 'v':
					Options.log_video = true;
					break;
				default:
					usage();
			}
		}
	}

	if (ini["main"].has("dump")) {
		for (const char *p = ini["main"]["dump"].c_str(); *p; p++) {
			switch (tolower(*p)) {
				case 'c':
					Options.dump_cpu = true;
					break;
				case 'r':
					Options.dump_ram = true;
					break;
				case 'b':
					Options.dump_bank = true;
					break;
				case 'v':
					Options.dump_vram = true;
					break;
				default:
					usage();
			}
		}
	}

	if (ini["main"].has("gif")) {
		strcpy(Options.gif_path, ini["main"]["gif"].c_str());
	}

	if (ini["main"].has("wav")) {
		strcpy(Options.wav_path, ini["main"]["wav"].c_str());
	}

	if (ini["main"].has("stds")) {
		if (!strcmp(ini["main"]["stds"].c_str(), "true")) {
			symbols_load_file("kernal.sym", 0);
			symbols_load_file("keymap.sym", 1);
			symbols_load_file("dos.sym", 2);
			symbols_load_file("geos.sym", 3);
			symbols_load_file("basic.sym", 4);
			symbols_load_file("monitor.sym", 5);
			symbols_load_file("charset.sym");
		}
	}

	if (ini["main"].has("scale")) {
		for (const char *p = ini["main"]["scale"].c_str(); *p; p++) {
			switch (tolower(*p)) {
				case '1':
				case '2':
				case '3':
				case '4':
					Options.window_scale = *p - '0';
					break;
				default:
					usage();
			}
		}
	}

	if (ini["main"].has("quality")) {
		char const *q = ini["main"]["quality"].c_str();
		if (!strcmp(q, "nearest")) {
			Options.scale_quality = scale_quality_t::NEAREST;
		} else if (!strcmp(q, "linear")) {
			Options.scale_quality = scale_quality_t::LINEAR;
		} else if (!strcmp(q, "best")) {
			Options.scale_quality = scale_quality_t::BEST;
		} else {
			usage();
		}
	}

	if (ini["main"].has("nosound")) {
		if (!strcmp(ini["main"]["nosound"].c_str(), "true")) {
			if (strlen(Options.audio_dev_name) > 0) {
				usage();
			}

			Options.no_sound = true;
		}
	}

	if (ini["main"].has("sound")) {
		if (Options.no_sound) {
			usage();
		}
		strcpy(Options.audio_dev_name, ini["main"]["sound"].c_str());
	}

	if (ini["main"].has("abufs")) {
		Options.audio_buffers = (int)strtol(ini["main"]["abufs"].c_str(), NULL, 10);
	}

	if (ini["main"].has("rtc")) {
		if (!strcmp(ini["main"]["rtc"].c_str(), "true")) {
			Options.set_system_time = true;
		}
	}

	if (ini["main"].has("nobinds")) {
		if (!strcmp(ini["main"]["nobinds"].c_str(), "true")) {
			Options.no_keybinds = true;
		}
	}

	if (ini["main"].has("ymirq")) {
		if (!strcmp(ini["main"]["ymirq"].c_str(), "true")) {
			Options.ym_irq = true;
		}
	}

	if (ini["main"].has("ymstrict")) {
		if (!strcmp(ini["main"]["ymstrict"].c_str(), "true")) {
			Options.ym_strict = true;
		}
	}
}

static void set_ini(mINI::INIStructure &ini, bool all)
{
	std::stringstream value;

	auto set_option = [&](const char *name, auto option_value, auto default_value) {
		if constexpr (std::is_same<decltype(option_value), char *>::value) {
			if (all || strcmp(option_value, default_value)) {
				ini["main"][name] = option_value;
			}
		} else if constexpr (std::is_same<decltype(option_value), const char *>::value) {
			if (all || strcmp(option_value, default_value)) {
				ini["main"][name] = option_value;
			}
		} else if constexpr (std::is_same<decltype(option_value), bool>::value) {
			if (all || option_value != default_value) {
				ini["main"][name] = option_value ? "true" : "false";
			}
		} else {
			if (all || option_value != default_value) {
				value << option_value;
				ini["main"][name] = value.str();
				value.str(std::string());
			}
		}
	};

	auto echo_mode_str = [](echo_mode_t mode) -> const char * {
		switch (mode) {
			case ECHO_MODE_NONE: return "none";
			case ECHO_MODE_RAW: return "raw";
			case ECHO_MODE_ISO: return "iso";
			case ECHO_MODE_COOKED: return "cooked";
		}
		return "none";
	};

	auto quality_str = [](scale_quality_t q) -> const char * {
		switch (q) {
			case scale_quality_t::NEAREST: return "nearest";
			case scale_quality_t::LINEAR: return "linear";
			case scale_quality_t::BEST: return "best";
		}
		return "nearest";
	};

	set_option("rom", Options.rom_path, Default_options.rom_path);
	set_option("ram", Options.num_ram_banks * 8, Default_options.num_ram_banks * 8);
	set_option("keymap", keymaps[Options.keymap], keymaps[Default_options.keymap]);
	set_option("hypercall_path", Options.hyper_path, Default_options.hyper_path);
	set_option("prg", Options.prg_path, Default_options.prg_path);
	set_option("run", Options.run_after_load, Default_options.run_after_load);
	set_option("bas", Options.bas_path, Default_options.bas_path);
	set_option("geos", Options.run_geos, Default_options.run_geos);
	set_option("test", Options.test_number, Default_options.test_number);
	set_option("nvram", Options.nvram_path, Default_options.nvram_path);
	set_option("sdcard", Options.sdcard_path, Default_options.sdcard_path);
	set_option("warp", Options.warp_factor > 0, Default_options.warp_factor > 0);
	set_option("echo", echo_mode_str(Options.echo_mode), echo_mode_str(Default_options.echo_mode));

	if (all || Options.log_keyboard != Default_options.log_keyboard || Options.log_speed != Default_options.log_speed || Options.log_video != Default_options.log_video) {
		if (Options.log_keyboard) {
			value << "k";
		}
		if (Options.log_speed) {
			value << "s";
		}
		if (Options.log_video) {
			value << "v";
		}
		ini["main"]["log"] = value.str();
		value.str(std::string());
	}

	if (all || Options.dump_cpu != Default_options.dump_cpu || Options.dump_ram != Default_options.dump_ram || Options.dump_bank != Default_options.dump_bank || Options.dump_vram != Default_options.dump_vram) {
		if (Options.dump_cpu) {
			value << "c";
		}
		if (Options.dump_ram) {
			value << "r";
		}
		if (Options.dump_bank) {
			value << "b";
		}
		if (Options.dump_vram) {
			value << "v";
		}
		ini["main"]["dump"] = value.str();
		value.str(std::string());
	}

	set_option("gif", Options.gif_path, Default_options.gif_path);
	set_option("wav", Options.wav_path, Default_options.wav_path);
	set_option("stds", Options.load_standard_symbols, Default_options.load_standard_symbols);
	set_option("scale", Options.window_scale, Default_options.window_scale);
	set_option("quality", quality_str(Options.scale_quality), quality_str(Default_options.scale_quality));
	set_option("nosound", Options.no_sound, Default_options.no_sound);
	set_option("sound", Options.audio_dev_name, Default_options.audio_dev_name);
	set_option("abufs", Options.audio_buffers, Default_options.audio_buffers);
	set_option("rtc", Options.set_system_time, Default_options.set_system_time);
	set_option("nobinds", Options.no_keybinds, Default_options.no_keybinds);
	set_option("ymirq", Options.ym_irq, Default_options.ym_irq);
	set_option("ymstrict", Options.ym_strict, Default_options.ym_strict);
}

void apply_ini(mINI::INIStructure &dst, const mINI::INIStructure &src)
{
	for (const auto& i : src) {
		dst.set(i.first, i.second);
	}
}


void load_options(const char *path, int argc, char **argv)
{
	if (path != nullptr) {
		strncpy(Options_base_path, path, PATH_MAX);
		fixup_directory(Options_base_path);
		snprintf(Options_ini_path, PATH_MAX, "%s/box16.ini", Options_base_path);
		Options_ini_path[PATH_MAX - 1] = 0;
	} else {
		Options_base_path[0] = 0;
		snprintf(Options_ini_path, PATH_MAX, "box16.ini");
		Options_ini_path[PATH_MAX - 1] = 0;
	}

	mINI::INIFile      file(Options_ini_path);
	bool force_write = !file.read(Inifile_ini);
	set_options(Inifile_ini);

	parse_cmdline(Cmdline_ini, argc, argv);
	set_options(Cmdline_ini);

	if (force_write) {
		save_options(true);
	}
}

void load_options()
{
	mINI::INIFile      file(Options_ini_path);
	mINI::INIStructure ini;

	bool force_write = !file.read(ini);

	if (!force_write) {
		set_options(ini);
	} else {
		save_options(true);
	}
}

void save_options(bool all)
{
	mINI::INIFile      file(Options_ini_path);
	mINI::INIStructure ini;

	set_ini(ini, all);

	file.generate(ini);
}

#if defined(WIN32)
static bool is_absolute_path(const char *path)
{
	return (isalpha(path[0]) && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) || (path[0] == '\\' && path[1] == '\\');
}
#else
static bool is_absolute_path(const char *path)
{
	return path[0] == '/';
}
#endif

static int options_get_derived_path(char *derived_path, const char *path, const char *base_path)
{
	int path_len = 0;

	if (is_absolute_path(path) || strlen(base_path) == 0 || strcmp(base_path, ".") == 0) {
		path_len = snprintf(derived_path, PATH_MAX, "%s", path);
	} else {
		path_len = snprintf(derived_path, PATH_MAX, "%s/%s", base_path, path);
		path_len = path_len < (PATH_MAX - 1) ? path_len : (PATH_MAX - 1);

		derived_path[path_len] = '\0';
	}
	return path_len;
}

int options_get_base_path(char *real_path, const char *path)
{
	return options_get_derived_path(real_path, path, Options_base_path);
}

int options_get_relative_path(char *real_path, const char *path)
{
	return options_get_derived_path(real_path, path, "./");
}

int options_get_hyper_path(char *hyper_path, const char *path)
{
	return options_get_derived_path(hyper_path, path, Options.hyper_path);
}

bool option_cmdline_option_was_set(char const *cmdline_option)
{
	return Cmdline_ini.has(cmdline_option);
}

bool option_inifile_option_was_set(char const *cmdline_option)
{
	return Inifile_ini.has(cmdline_option);
}

option_source option_get_source(char const *cmdline_option)
{
	if (Cmdline_ini.has(cmdline_option)) {
		return option_source::CMDLINE;
	}
	if (Inifile_ini.has(cmdline_option)) {
		return option_source::INIFILE;
	}
	return option_source::DEFAULT;
}

char const *option_get_source_name(option_source source)
{
	switch (source) {
		case option_source::DEFAULT: return "Default";
		case option_source::CMDLINE: return "Command-line parameter";
		case option_source::INIFILE: return "Ini file";
	}
	return "Unknown";
}