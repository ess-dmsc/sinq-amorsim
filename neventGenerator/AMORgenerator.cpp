#include <iostream>
#include <unistd.h> // getopt

#include "uparam.hpp"
#include "nexus_reader.hpp"
#include "mcstas_reader.hpp"
#include "generator.hpp"

using StreamFormat = nexus::ESSformat;

typedef nexus::Amor Instrument;

typedef nexus::NeXusSource<Instrument,StreamFormat> Source;
typedef control::CommandlineControl Control;

typedef serialiser::FlatBufSerialiser<StreamFormat::value_type> Serialiser;
//typedef serialiser::NoSerialiser<uint64_t> Serialiser;

///////////////////////
// In the end we want to use kafka, I will use 0MQ for development purposes
//typedef generator::ZmqGen<generator::transmitter> generator_t;
typedef  generator::KafkaGen<generator::transmitter> Communication;
//typedef FileWriterGen generator_t;

typedef uparam::Param Param;

Param parse(int, char **);

///////////////////////////////////////////////
///////////////////////////////////////////////
///
/// Main program for using the flexible event generator
///
///  \author Michele Brambilla <mib.mic@gmail.com>
///  \date Wed Jun 08 15:14:10 2016
int main(int argc, char **argv) {

  Param input = parse(argc,argv);
  input.print();
  
   Source stream(input,uparam::to_num<int>(input["multiplier"]));
   
   Generator<Communication,Control,Serialiser> g(input);

   g.run(&(stream.begin()[0]),stream.count());

  return 0;
}



void helper(Param input) {
  std::cout << "AMORgenerator" << "\n is a neutron event generator based on a flexible library. It "
            << "supports multiple transport (kafka, 0MQ, file I/O), sources (NeXus files,"
            << "MCstas simulation output) and serialisation (no serialisation,"
            << "FlatBuffers). When executed the generator sites on the status defined in the"
            << "control file and can be driven from the command line (available commands are"
            << "'run', 'pause', 'stop')\n"
            << std::endl;

  std::cout << "Usage example:\n"
            << "[0MQ]\t./AMORgenerator -p 1234 -f sample/amor2015n001774.hdf\n"
            << "[kafka]\t./AMORgenerator -b ess01 -t test_0\n"
            << std::endl;
  
  std::cout << "-a" << "\t" << "area detector source file (mcstas)" << "\n"
            << "-b" << "\t" << "broker address (when use kafka)" << "\n"
            << "-c" << "\t" << "control file (when use file)" << "\n"
            << "-f" << "\t" << "NeXus file source [default = " << input["filename"] << "\n"
            << "-e" << "\t" << "header template [default = " << input["header"] << "\n"
            << "-i" << "\t" << "configuration file" << "\n"
            << "-m" << "\t" << "data multiplier [default = " << input["multiplier"] << "\n"
            << "-p" << "\t" << "port used for streaming (kafka,zmq) [default = " << input["port"] << "]\n"
            << "-r" << "\t" << "set generator status to 'run'" << "\n"
            << "-s" << "\t" << "1D detector source file (mcstas)" << "\n"
            << "-t" << "\t" << "topic name (kafka) [default = " << input["topic"] << "\n"
            << "-h" << "\t" << "this help" << "\n"
            << std::endl;
  exit(0);
}


/*!
 * Default arguments and command line parser
 *  \author Michele Brambilla <mib.mic@gmail.com>
 *  \date Fri Jun 17 12:20:25 2016
 */
Param parse(int argc, char **argv) {
  Param input;
  std::string configuration_file("config.in");

  for(int i=0;i<argc;++i) {
    if(std::string(*(argv+i)) == "-i" ) {
      if( (*(argv+i+1))[0] == '-' ) {
        throw std::runtime_error("Bad parameter for configuration file.");
      }
      else {
        configuration_file=std::string( *(argv+i+1) );
        std::cout << configuration_file << std::endl;
      }
      break;
    }
  }

  input.read(configuration_file,uparam::RapidJSON());
  input["status"] = "pause";

  opterr = 0;
  int opt;
  while ((opt = getopt (argc, argv, "a:b:c:f:p:s:t:e:m:rh")) != -1) {
    switch (opt) {
    case 'a': //area
      input["2D"] = std::string(optarg);
      break;
    case 'b':
      input["brokers"] = std::string(optarg);
      break;
    case 'c':
      input["control"] = std::string(optarg);
      break;
    case 'f':
      input["filename"] = std::string(optarg);
      break;
    case 'p':
      input["port"] = std::string(optarg);
      break;
    case 's': // single dimension detector
      input["1D"] = std::string(optarg);
      break;
    case 't':
      input["topic"] = std::string(optarg);
      break;
    case 'e':
      input["header"] = std::string(optarg);
      break;
    case 'm':
      input["multiplier"] = std::string(optarg);
      break;
    case 'r':
      input["status"] = "run";
      break;
    case 'h':
      helper(input);
    }
  }

  return std::move(input);
}
