#pragma once

#include "Errors.hpp"
#include "json.h"
#include "utils.hpp"

namespace SINQAmorSim {

class KafkaConfiguration {
public:
  std::string broker{""};
  std::string topic{""};
};

class Configuration {

public:
  KafkaConfiguration producer;
  std::string configuration_file{""};
  std::string source{""};
  std::string source_name{"AMOR.event.stream"};
  std::string timestamp_generator{"none"};
  int multiplier{0};
  int bytes{0};
  int rate{0};
  int report_time{10};
  int num_threads{0};
  bool valid{true};
  KafkaOptions options;
};

class ConfigurationParser {
  // Looks into command line if any configuration file is specified. If so
  // generates an initial configuration based on it, else looks for a default
  // config file. If the latter is missing expects that all the relevant
  // informations have been provided as command line argument.
public:
  int parse_configuration_file(const std::string &input);
  int parse_configuration(int argc, char **argv);
  void override_configuration_with(const Configuration &other);
  int validate();
  void print();

  int parse_configuration_file_impl();
  void get_kafka_options(nlohmann::json &);

  Configuration parse_command_line(int argc, char **argv);

  KafkaConfiguration parse_string_uri(const std::string &uri,
                                      const bool use_defaults = false);

  Configuration config;

private:
  nlohmann::json configuration;
};
} // namespace SINQAmorSim
