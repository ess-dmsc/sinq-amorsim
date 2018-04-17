#include <iostream>

#include "generator.hpp"
#include "mcstas_reader.hpp"
#include "nexus_reader.hpp"

using StreamFormat = SINQAmorSim::ESSformat;

using Instrument = SINQAmorSim::Amor;
using Source = SINQAmorSim::NeXusSource<Instrument, StreamFormat>;
using Control = SINQAmorSim::CommandlineControl;

using Serialiser = SINQAmorSim::FlatBufferSerialiser;
using Communication = SINQAmorSim::KafkaTransmitter<Serialiser>;

int main(int argc, char **argv) {

  SINQAmorSim::ConfigurationParser parser;
  auto err = parser.parse_configuration(argc, argv);
  if (!err) {
    parser.print();
  } else {
    std::cout << SINQAmorSim::Err2Str(err) << "\n";
    return -1;
  }
  auto &config = parser.config;

  if (config.bytes > 0 && config.multiplier > 1) {
    throw std::runtime_error(
        "Conflict between parameters `bytes` and `multiplier`");
  }
#if 0
  Source stream(config.source, config.multiplier);
  auto data = stream.get();
#else
  std::vector<StreamFormat::value_type> data;
  if (config.bytes > 0) {
    data.resize(config.bytes / sizeof(StreamFormat::value_type));
  }
#endif

  Generator<Communication, Control, Serialiser> g(config);
  g.run<StreamFormat::value_type>(data);

  return 0;
}
