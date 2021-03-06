#include "../Configuration.hpp"

#include <stdlib.h>

#include <gtest/gtest.h>

auto source_dir = std::string(CMAKE_CURRENT_SOURCE_DIR);
int configuration_OK =
    SINQAmorSim::ConfigurationError::error_no_configuration_error;
TEST(ConfigurationParser, use_default_if_wrong_syntax) {
  SINQAmorSim::ConfigurationParser parser;
  auto configuration = parser.parse_string_uri("hello", true);
  EXPECT_EQ(configuration.broker, "localhost:9092");
  EXPECT_EQ(configuration.topic, "empty-topic");
}

TEST(ConfigurationParser, leave_empty_if_wrong_syntax) {
  SINQAmorSim::ConfigurationParser parser;

  EXPECT_ANY_THROW(parser.parse_string_uri("hello", false));
  EXPECT_ANY_THROW(parser.parse_string_uri("//localhost:9092/", false));
  EXPECT_ANY_THROW(parser.parse_string_uri("//localhost:/my-topic", false));
  EXPECT_ANY_THROW(parser.parse_string_uri("//:9092/my-topic", false));
  EXPECT_ANY_THROW(parser.parse_string_uri("//:/my-topic", false));
  EXPECT_ANY_THROW(
      parser.parse_string_uri("//localhost:9092,:9093/my-topic", false));
  EXPECT_ANY_THROW(
      parser.parse_string_uri("//localhost:9092,localhost:/my-topic", false));

  EXPECT_NO_THROW(parser.parse_string_uri("//localhost:9092/my-topic", false));
  EXPECT_NO_THROW(parser.parse_string_uri(
      "//localhost:9092,localhost:9093/my-topic", false));
}

TEST(ConfigurationParser, parse_broker_port_topic) {
  SINQAmorSim::ConfigurationParser parser;
  auto configuration = parser.parse_string_uri("//localhost:8080/my-topic");
  EXPECT_EQ(configuration.broker, "localhost:8080");
  EXPECT_EQ(configuration.topic, "my-topic");
}

TEST(ConfigurationParser, missing_json_configuration) {
  SINQAmorSim::ConfigurationParser parser;
  auto filename = source_dir + "/missing_configuration.json";
  auto result = parser.parse_configuration_file(filename);
  EXPECT_EQ(result, SINQAmorSim::ConfigurationError::error_input_file);
}

TEST(ConfigurationParser, invalid_json_configuration) {
  SINQAmorSim::ConfigurationParser parser;
  auto filename = source_dir + "/invalid_configuration.json";
  auto result = parser.parse_configuration_file(filename);
  EXPECT_NE(result, configuration_OK);
}

TEST(ConfigurationParser, read_json_configuration) {
  SINQAmorSim::ConfigurationParser parser;
  auto filename = source_dir + "/valid_configuration.json";
  auto result = parser.parse_configuration_file(filename);
  EXPECT_NE(result, SINQAmorSim::ConfigurationError::error_input_file);
}

TEST(ConfigurationParser, parse_json_invalid_string) {
  SINQAmorSim::ConfigurationParser parser;
  rapidjson::Document document;
  document.Parse("{ \"producer_uri\" : 1 }");

  auto result = parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(result, SINQAmorSim::ConfigurationError::error_parsing_json);
  document.Parse("{ \"source\" : 1 }");
  result = parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(result, SINQAmorSim::ConfigurationError::error_parsing_json);
}

TEST(ConfigurationParser, parse_json_invalid_int) {
  SINQAmorSim::ConfigurationParser parser;
  rapidjson::Document document;
  document.Parse("{ \"multiplier\" : \"1\" }");
  auto result = parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(result, SINQAmorSim::ConfigurationError::error_parsing_json);
  document.Parse("{ \"rate\" : \"1\" }");
  result = parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(result, SINQAmorSim::ConfigurationError::error_parsing_json);
}

TEST(ConfigurationParser, parse_valid_json) {
  SINQAmorSim::ConfigurationParser parser;
  rapidjson::Document document;
  document.Parse("{ \"multiplier\" : 1,"
                 "\"rate\" : 2,"
                 "\"source\" : \"file.h5\","
                 "\"producer_uri\" : \"//localhost:9092/my-topic\"}");
  auto result = parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(result, configuration_OK);
  EXPECT_EQ(parser.config.producer.broker, std::string("localhost:9092"));
  EXPECT_EQ(parser.config.producer.topic, std::string("my-topic"));
  EXPECT_EQ(parser.config.source, std::string("file.h5"));
  EXPECT_EQ(parser.config.multiplier, 1);
  EXPECT_EQ(parser.config.rate, 2);
}

TEST(ConfigurationParser, verify_json) {
  SINQAmorSim::ConfigurationParser parser;
  rapidjson::Document document;
  document.Parse("{ \"multiplier\" : 1,"
                 "\"rate\" : 2,"
                 "\"source\" : \"file.h5\","
                 "\"producer_uri\" : \"//localhost:9092/my-topic\"}");
  auto result = parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(parser.config.producer.broker, std::string("localhost:9092"));
  EXPECT_EQ(parser.config.producer.topic, std::string("my-topic"));
  EXPECT_EQ(parser.config.source, std::string("file.h5"));
  EXPECT_EQ(parser.config.multiplier, 1);
  EXPECT_EQ(parser.config.rate, 2);
}

TEST(ConfigurationParser, parse_multiple_brokers) {
  SINQAmorSim::ConfigurationParser parser;
  // pc12206:,pc12207:9092/topic-name
  // pc12206:,pc12207:/topic-name
  // pc12206:9092,pc12206:9093/topic-name
  EXPECT_ANY_THROW(
      parser.parse_string_uri("//localhost:,localhost:9093/my-topic"));
  EXPECT_EQ(parser.parse_string_uri("//localhost:9092,localhost:9093/my-topic")
                .broker,
            "localhost:9092,localhost:9093");
  EXPECT_EQ(
      parser.parse_string_uri("//localhost:9092,localhost:9093/my-topic").topic,
      "my-topic");
}

void init_command_line_args(const int c, char **v) {
  for (int i = 0; i < c; ++i) {
    v[i] = (char *)calloc(100, sizeof(char));
  }
}
void add_command_line_arg(int *count, char **v, const char *key,
                          const char *value) {
  std::strncpy(v[(*count)++], key, 100);
  std::strncpy(v[(*count)++], value, 100);
}
void destroy_command_line_args(const int c, char **v) {
  for (int i = 0; i < c; ++i) {
    free(v[i]);
  }
  free(v);
}

// NOTE: after each test reset optind so that getopt() is reinitialised

TEST(ConfigurationParser, no_error_flag_on_empty_command_line) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 10, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);
  add_command_line_arg(&count, argv, "", "");

  auto configuration = parser.parse_command_line(argc, argv);
  EXPECT_TRUE(configuration.valid);

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, error_flag_on_invalid_command_line_opts) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 10, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);
  add_command_line_arg(&count, argv, "--key0", "val0");
  add_command_line_arg(&count, argv, "--key1", "val1");
  add_command_line_arg(&count, argv, "--key2", "val2");

  auto configuration = parser.parse_command_line(argc, argv);
  EXPECT_FALSE(configuration.valid);

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, valid_flag_on_valid_command_line_opts) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 10, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);
  add_command_line_arg(&count, argv, "--config-file", "config");
  add_command_line_arg(&count, argv, "--producer-uri", "//:/as");
  add_command_line_arg(&count, argv, "--source", "sourcefile.h5");
  add_command_line_arg(&count, argv, "--multiplier", "5");

  auto configuration = parser.parse_command_line(argc, argv);
  EXPECT_TRUE(configuration.valid);

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, error_flag_on_valid_and_invalid_command_line_opts) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 10, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);
  add_command_line_arg(&count, argv, "--config-file", "config");
  add_command_line_arg(&count, argv, "--producer-uri", "//:/as");
  add_command_line_arg(&count, argv, "--source", "sourcefile.h5");
  add_command_line_arg(&count, argv, "--key", "val");

  auto configuration = parser.parse_command_line(argc, argv);
  EXPECT_FALSE(configuration.valid);

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, parse_command_line_opts) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 10, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);
  add_command_line_arg(&count, argv, "--config-file", "config");
  add_command_line_arg(&count, argv, "--producer-uri", "//localhost:8082/as");
  add_command_line_arg(&count, argv, "--source", "sourcefile.h5");
  add_command_line_arg(&count, argv, "--multiplier", "5");

  auto configuration = parser.parse_command_line(argc, argv);

  EXPECT_EQ(configuration.configuration_file, std::string("config"));
  EXPECT_EQ(configuration.producer.broker, std::string("localhost:8082"));
  EXPECT_EQ(configuration.producer.topic, std::string("as"));
  EXPECT_EQ(configuration.source, std::string("sourcefile.h5"));

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, parse_command_line_opts_despite_wrong_args) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 11, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);
  add_command_line_arg(&count, argv, "--config-file", "config");
  add_command_line_arg(&count, argv, "--producer-uri", "//localhost:8082/as");
  add_command_line_arg(&count, argv, "--source", "sourcefile.h5");
  add_command_line_arg(&count, argv, "--multiplier", "5");
  add_command_line_arg(&count, argv, "--key", "val");

  auto configuration = parser.parse_command_line(argc, argv);

  EXPECT_EQ(configuration.configuration_file, std::string("config"));
  EXPECT_EQ(configuration.producer.broker, std::string("localhost:8082"));
  EXPECT_EQ(configuration.producer.topic, std::string("as"));
  EXPECT_EQ(configuration.source, std::string("sourcefile.h5"));

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, parse_both_command_line_and_file) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 11, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);

  auto filename = source_dir + "/valid_configuration.json";
  add_command_line_arg(&count, argv, "--config-file", &filename[0]);
  add_command_line_arg(&count, argv, "--producer-uri", "//nohost:8082/as");
  add_command_line_arg(&count, argv, "--source", "sourcefile.h5");

  auto err = parser.parse_configuration(count, argv);
  EXPECT_EQ(err, configuration_OK);

  auto &cfg = parser.config;
  EXPECT_EQ(cfg.configuration_file, filename);
  EXPECT_EQ(cfg.producer.broker, std::string("nohost:8082"));
  EXPECT_EQ(cfg.producer.topic, std::string("as"));
  EXPECT_EQ(cfg.source, std::string("sourcefile.h5"));
  EXPECT_EQ(cfg.multiplier, 1);
  EXPECT_EQ(cfg.rate, 10);

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, error_incomplete_configuration) {
  SINQAmorSim::ConfigurationParser parser;
  int argc = 11, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);

  auto filename = source_dir + "/incomplete_configuration.json";
  add_command_line_arg(&count, argv, "--config-file", &filename[0]);
  add_command_line_arg(&count, argv, "--producer-uri", "//nohost:8082/as");
  add_command_line_arg(&count, argv, "--source", "sourcefile.h5");

  auto err = parser.parse_configuration(count, argv);
  EXPECT_EQ(err, SINQAmorSim::ConfigurationError::error_configuration_invalid);
  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, timestamp_generator_from_file) {

  SINQAmorSim::ConfigurationParser parser;
  rapidjson::Document document;
  document.Parse("{ \"timestamp_generator\" : \"none\" }");
  parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(parser.config.timestamp_generator, std::string("none"));

  document.Parse("{ \"timestamp_generator\" : \"const_timestamp\" }");
  parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(parser.config.timestamp_generator, std::string("const_timestamp"));

  document.Parse("{ \"timestamp_generator\" : \"random_timestamp\" }");
  parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(parser.config.timestamp_generator, std::string("random_timestamp"));

  document.Parse("{ \"timestamp_generator\" : \"any_timestamp\" }");
  parser.parse_configuration_file_impl(std::move(document));
  EXPECT_EQ(parser.config.timestamp_generator, std::string("any_timestamp"));

  auto filename = source_dir + "/valid_configuration.json";
  parser.parse_configuration_file(filename);
  EXPECT_EQ(parser.config.timestamp_generator,
            std::string("any_other_timestamp"));
}

TEST(ConfigurationParser, empty_timestamp_generator_in_command_line) {

  SINQAmorSim::ConfigurationParser parser;

  int argc = 11, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);

  auto result = parser.parse_command_line(count, argv);
  EXPECT_TRUE(result.timestamp_generator.empty());

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, timestamp_generator_from_command_line) {

  SINQAmorSim::ConfigurationParser parser;

  int argc = 11, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);

  add_command_line_arg(&count, argv, "--timestamp-generator", "any_timestamp");
  auto result = parser.parse_command_line(count, argv);
  EXPECT_EQ(result.timestamp_generator, std::string("any_timestamp"));

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, command_line_timestamp_generator_override_file) {
  SINQAmorSim::ConfigurationParser parser;

  int argc = 11, count = 1;
  char **argv = (char **)calloc(argc, sizeof(char *));
  init_command_line_args(argc, argv);

  auto filename = source_dir + "/valid_configuration.json";
  add_command_line_arg(&count, argv, "--config-file", &filename[0]);
  add_command_line_arg(&count, argv, "--timestamp-generator",
                       "yet_another_timestamp");

  parser.parse_configuration(count, argv);
  EXPECT_EQ(parser.config.timestamp_generator,
            std::string("yet_another_timestamp"));

  destroy_command_line_args(argc, argv);
  optind = 0;
}

TEST(ConfigurationParser, kafka_options) {

  {
    std::string opt = {"{\"some_other_parameter\": \"none\",\"kafka_options\": "
                       "{\"message.max.bytes\":\"1024\" }}"};
    rapidjson::StringStream stream(opt.c_str());
    rapidjson::Document d;
    d.ParseStream(stream);

    SINQAmorSim::ConfigurationParser parser;
    parser.parse_configuration_file_impl(std::move(d));

    ASSERT_EQ(parser.config.options.size(), 1);
    EXPECT_EQ(parser.config.options[0].first, "message.max.bytes");
    EXPECT_EQ(parser.config.options[0].second, "1024");
  }
  {
   std::string opt = {"{\"x\": \"none\",\"kafka_options\": "
                       "{\"a\":\"1\",\"b\":\"2\",\"c\":\"3\"  }}"};
    rapidjson::StringStream stream(opt.c_str());
    rapidjson::Document d;
    d.ParseStream(stream);

    SINQAmorSim::ConfigurationParser parser;
    parser.parse_configuration_file_impl(std::move(d));

    ASSERT_EQ(parser.config.options.size(), 3);
    EXPECT_EQ(parser.config.options[0].first, "a");
    EXPECT_EQ(parser.config.options[0].second, "1");
    EXPECT_EQ(parser.config.options[1].first, "b");
    EXPECT_EQ(parser.config.options[1].second, "2");
    EXPECT_EQ(parser.config.options[2].first, "c");
    EXPECT_EQ(parser.config.options[2].second, "3");
   }
}
