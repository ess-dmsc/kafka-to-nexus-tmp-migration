#include <cstdlib>
#include <cstdio>
#include <string>
#include <getopt.h>
#include "logger.h"
#include "Master.h"

#if HAVE_GTEST
#include <gtest/gtest.h>
#endif

extern "C" char const GIT_COMMIT[];



// POD
struct MainOpt {
bool help = false;
bool verbose = false;
bool gtest = false;
BrightnESS::FileWriter::MasterConfig master_config;
};


int main(int argc, char ** argv) {
	MainOpt opt;

	static struct option long_options[] = {
		{"help",                            no_argument,              0, 'h'},
		{"broker-command-address",          required_argument,        0,  0 },
		{"broker-command-topic",            required_argument,        0,  0 },
		{"test",                            no_argument,              0,  0 },
		{0, 0, 0, 0},
	};
	std::string cmd;
	int option_index = 0;
	bool getopt_error = false;
	while (true) {
		int c = getopt_long(argc, argv, "vh", long_options, &option_index);
		//LOG(5, "c getopt {}", c);
		if (c == -1) break;
		if (c == '?') {
			getopt_error = true;
		}
		switch (c) {
		case 'v':
			opt.verbose = true;
			log_level = std::max(0, log_level - 1);
			break;
		case 'h':
			opt.help = true;
			break;
		case 0:
			auto lname = long_options[option_index].name;
			if (std::string("help") == lname) {
				opt.help = true;
			}
			if (std::string("broker-command-address") == lname) {
				opt.master_config.command_listener.address = optarg;
			}
			if (std::string("broker-command-topic") == lname) {
				opt.master_config.command_listener.topic = optarg;
			}
			if (std::string("test") == lname) {
				opt.gtest = true;
			}
			break;
		}
	}

	if (getopt_error) {
		LOG(5, "ERROR parsing command line options");
		opt.help = true;
		return 1;
	}

	printf("ess-file-writer-0.0.1  (ESS, BrightnESS)\n");
	printf("  %.7s\n", GIT_COMMIT);
	printf("  Contact: dominik.werder@psi.ch, michele.brambilla@psi.ch\n\n");

	if (opt.help) {
		printf("Forwards EPICS process variables to Kafka topics.\n"
		       "Controlled via JSON packets sent over the configuration topic.\n"
		       "\n"
		       "\n"
		       "forward-epics-to-kafka\n"
		       "  --help, -h\n"
		       "\n"
		       "  --test\n"
		       "      Run tests\n"
		       "\n"
		       "  --broker-configuration-address    host:port,host:port,...\n"
		       "      Kafka brokers to connect with for configuration updates.\n"
		       "      Default: %s\n"
		       "\n",
			opt.master_config.command_listener.address.c_str());

		printf("  --broker-configuration-topic      <topic-name>\n"
		       "      Topic name to listen to for configuration updates.\n"
		       "      Default: %s\n"
		       "\n",
			opt.master_config.command_listener.topic.c_str());

		printf("  -v\n"
		       "      Increase verbosity\n"
		       "\n");
		printf("  --test\n"
		       "      Run test suite\n"
		       "\n");
		return 1;
	}

	if (opt.gtest) {
		#if HAVE_GTEST
		::testing::InitGoogleTest(&argc, argv);
		return RUN_ALL_TESTS();
		#else
		printf("ERROR To run tests, the executable must be compiled with the Google Test library.\n");
		return 1;
		#endif
	}

	return 0;
}