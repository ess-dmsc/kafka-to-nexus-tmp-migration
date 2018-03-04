#include "CLIOptions.h"
#include "MainOpt.h"
#include "uri.h"
#include <CLI/CLI.hpp>

CLI::Option *uri_option(CLI::App &App, std::string Name, uri::URI &URIArg,
                        CLI::callback_t Fun, std::string Description,
                        bool Defaulted) {

  CLI::Option *Opt = App.add_option(Name, Fun, Description, Defaulted);
  Opt->set_custom_option("URI", 1);
  if (Defaulted) {
    Opt->set_default_str(URIArg.URIString);
  }
  return Opt;
}

/// Use for adding a URI option
CLI::Option *add_option(CLI::App &App, std::string Name, uri::URI &URIArg,
                        std::string Description = "", bool Defaulted = false) {
  CLI::callback_t Fun = [&URIArg](CLI::results_t Results) {
    URIArg.parse(Results[0]);
    return true;
  };

  return uri_option(App, Name, URIArg, Fun, Description, Defaulted);
}

/// Use for adding a URI option, if the URI is given then TrueIfOptionGiven is
/// set to true
CLI::Option *add_option(CLI::App &App, std::string Name, uri::URI &URIArg,
                        bool &TrueIfOptionGiven, std::string Description = "",
                        bool Defaulted = false) {
  CLI::callback_t Fun = [&URIArg, &TrueIfOptionGiven](CLI::results_t Results) {
    TrueIfOptionGiven = true;
    URIArg.parse(Results[0]);
    return true;
  };

  return uri_option(App, Name, URIArg, Fun, Description, Defaulted);
}

void setCLIOptions(CLI::App &App, MainOpt &MainOptions) {
  App.set_config("--config-file", "", "Specify an ini file to set config",
                 false)
      ->check(CLI::ExistingFile);
  add_option(
      App, "--command-uri", MainOptions.command_broker_uri,
      "<//host[:port][/topic]> Kafka broker/topic to listen for commands");
  add_option(App, "--status-uri", MainOptions.kafka_status_uri,
             MainOptions.do_kafka_status,
             "<//host[:port][/topic]> Kafka broker/topic to publish status "
                 "updates on");
  App.add_option("--kafka-gelf", MainOptions.kafka_gelf,
                 "<//host[:port]/topic> Log to Graylog via Kafka GELF adapter");
  App.add_option("--graylog-logger-address", MainOptions.graylog_logger_address,
                 "<host:port> Log to Graylog via graylog_logger library");
  App.add_option("-v", log_level,
                 "Set logging level. 3 == Error, 7 == Debug. Default: 6 (Info)",
                 true)
      ->check(CLI::Range(1, 7));
  App.add_option("--hdf-output-prefix", MainOptions.hdf_output_prefix,
                 "<absolute/or/relative/directory> Directory which gets "
                     "prepended to the HDF output filenames in the file write "
                     "commands");
  App.add_flag("--logpid-sleep", MainOptions.logpid_sleep);
  App.add_flag("--use-signal-handler", MainOptions.use_signal_handler);
  App.add_option("--teamid", MainOptions.teamid);
}
