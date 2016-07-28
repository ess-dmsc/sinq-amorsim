#pragma once

#include <string>
#include <sstream>
#include <utility>
#include <cctype>
#include <iterator>

#include <assert.h>

#include <librdkafka/rdkafkacpp.h>

#include "serialiser.hpp"
#include "uparam.hpp"
#include "header.hpp"

const int max_message_size = 100000000;

namespace generator {

  enum {transmitter,receiver};
  /*! \struct KafkaGen
   *  Uses Kafka as the streamer
   *
   *  \author Michele Brambilla <mib.mic@gmail.com>
   *  \date Wed Jun 08 15:19:16 2016
   */
  template<int mode_selector>
  struct KafkaGen {
  
    KafkaGen(uparam::Param p) : brokers(p["brokers"]), topic_str(p["topic"]) {
      /*! @param p see uparam::Param for description. Must contain "brokers" and "topic" key-value */
      /*! Connects to the "broker" clients of the Kafka messaging system. Streams data to the "topic" topic.
       */
      conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
      tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

      conf->set("metadata.broker.list", brokers, errstr);
      if (!debug.empty()) {
        if (conf->set("debug", debug, errstr) != RdKafka::Conf::CONF_OK) {
          std::cerr << errstr << std::endl;
          exit(1);
        }
      }

      conf->set("message.max.bytes", "100000000", errstr);
      std::cerr << errstr << std::endl;

      if(topic_str.empty()) {
        std::cerr << "Topic not set" << std::endl;
        exit(1);
      }
      
      producer = RdKafka::Producer::create(conf, errstr);
      if (!producer) {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        exit(1);
      }
      std::cout << "% Created producer " << producer->name() << std::endl;
      
      topic = RdKafka::Topic::create(producer, topic_str,
                                     tconf, errstr);
      if (!topic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
      }
    }

    template<typename T>
    void send(T* data,const int size, const int flag = 0) {
      /*! @param data data to be sent
       *  @param size number of elements
       *  @param flag optional flag (unused) */
      RdKafka::ErrorCode resp =
        producer->produce(topic, partition,
                          RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                          data, size*sizeof(T),
                          NULL, NULL);
      if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
      std::cerr << "% Produced message (" << sizeof(data) 
                << " bytes)" 
                << std::endl;
    }

    template<typename T>
    void send(std::string h, T* data, int nev, serialiser::NoSerialiser<T>) {
      RdKafka::ErrorCode resp =
        producer->produce(topic, partition,
                          RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                          (void*)h.c_str(), h.size(),
                          NULL, NULL);
      if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed header: " <<
          RdKafka::err2str(resp) << std::endl;
      if( nev > 0 ) {
        resp =
          producer->produce(topic, partition,
                            RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                            (void*)data, nev*sizeof(T),
                            NULL, NULL);
        
        if (resp != RdKafka::ERR_NO_ERROR)
          std::cerr << "% Produce failed data: " <<
            RdKafka::err2str(resp) << std::endl;
      }
      
      std::cerr << "% Produced message (" << h.size()+sizeof(T)*nev 
                << " bytes)" 
                << std::endl;
    }
    
    
    template<typename T>
    void send(std::string h, T* data, int nev, serialiser::FlatBufSerialiser<T>) {
      
      serialiser::FlatBufSerialiser<T> s;
      s(h,data,nev);
      
      RdKafka::ErrorCode resp =
        producer->produce(topic, partition,
                          RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                          (void*)s(), s.size(),
                          NULL, NULL);
      if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " <<
          RdKafka::err2str(resp) << std::endl;
      
      std::cerr << "% Produced message (" << h.size()+sizeof(T)*nev 
                << " bytes)" 
                << std::endl;
    }
    
    
    
  private:
    std::string brokers;
    std::string topic_str;
    std::string errstr;
    std::string debug;
    
int32_t partition = 0;//RdKafka::Topic::PARTITION_UA;
    int64_t start_offset = RdKafka::Topic::OFFSET_BEGINNING;
    
    RdKafka::Conf *conf;
    RdKafka::Conf *tconf;
    RdKafka::Producer *producer;
    RdKafka::Topic *topic;
    
  };


  
  std::pair<int,int> consume_header(RdKafka::Message* message, void* opaque) {
    std::cout << "Read msg at offset " << message->offset() << std::endl;
    opaque = message->payload();
    std::copy(static_cast<char*>(message->payload()),
              static_cast<char*>(message->payload())+message->len(),
              static_cast<char*>(opaque));
    return parse_header(std::string(static_cast<char*>(message->payload()) ));
  }
  
  template<typename T>
  void consume_data(RdKafka::Message* message, void* opaque) {
    
    std::cout << "Len: " << message->len() << "\n";
    std::cout << "Do something with data..." << std::endl;
    
    //    opaque = message->payload();    
    return;
  }




  template<int mode_selector>
  struct KafkaListener {
    static const int max_header_size=10000;
    
    KafkaListener(uparam::Param p) : brokers(p["brokers"]), topic_str(p["topic"]) {
      /*! @param p see uparam::Param for description. Must contain "brokers" and "topic" key-value */
      /*! Connects to the "broker" clients of the Kafka messaging system. Streams data to the "topic" topic.
       */
      conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
      tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
      
      conf->set("metadata.broker.list", brokers, errstr);
      if (!debug.empty()) {
        if (conf->set("debug", debug, errstr) != RdKafka::Conf::CONF_OK) {
          std::cerr << errstr << std::endl;
          exit(1);
        }
      }
      conf->set("fetch.message.max.bytes", "100000000", errstr);
      std::cerr << errstr << std::endl;
      
      if(topic_str.empty()) {
        std::cerr << "Topic required." << std::endl;
        exit(1);
      }
      
      consumer = RdKafka::Consumer::create(conf, errstr);
      if (!consumer) {
        std::cerr << "Failed to create consumer: " << errstr << std::endl;
        exit(1);
      }
      std::cout << "% Created consumer " << consumer->name() << std::endl;
      
      topic = RdKafka::Topic::create(consumer, topic_str,
                                     tconf, errstr);
      if (!topic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
      }
      
      /*
       * Start consumer for topic+partition at start offset
       */
      RdKafka::ErrorCode resp = consumer->start(topic, partition, start_offset);
      if (resp != RdKafka::ERR_NO_ERROR) {
        std::cerr << "Failed to start consumer: " <<
          RdKafka::err2str(resp) << std::endl;
        exit(1);
      }
      
    }


    template<typename T>
    int recv(std::string& h, std::vector<T>& data, serialiser::NoSerialiser<T>) {
      void* value; 
      int use_ccb = 1;

      /*
       * Consume messages
       */
      std::pair<int,int> result;
      RdKafka::Message *msg = nullptr;
      do {
        msg = consumer->consume(topic, partition, 1000);
      } while (  msg->err() != RdKafka::ERR_NO_ERROR );
      result = consume_header(msg, static_cast<void*>(&h[0]));
      h = std::string(static_cast<char*>(msg->payload()));
      std::cout << h << std::endl;
      
      if(result.second > 0) {
        msg = consumer->consume(topic, partition, 1000);
        if(msg->err() != RdKafka::ERR_NO_ERROR )
          std::cerr << "expected event data" << std::endl;
        consume_data<T>(msg,value);
        // if(data.size() < result.second)
        //   data.resize(result.second);
        // std::cout << std::distance(static_cast<T*>(msg->payload()),
        //                              static_cast<T*>(msg->payload())+result.second)
        //           << std::endl;
        // std::copy(static_cast<T*>(msg->payload()),
        //           static_cast<T*>(msg->payload())+result.second,
        //           data.begin());
      }

      delete msg;


      return result.first;
    }
    


    template<typename T>
    int  recv(std::string& h, std::vector<T>& data, serialiser::FlatBufSerialiser<T>) {

      //SerialisedConsumeCb ex_consume_cb;
      int use_ccb = 1;
      /*
       * Consume messages
       */
std::cout << "Poll: "<<      consumer->poll(0) << std::endl;

//while (true) {
      //      consumer->poll(0);
      // if (use_ccb) {
      //   consumer->consume_callback(topic,
      //                              partition,
      //                              -1,
      //                              &ex_consume_cb,
      //                              &use_ccb);
      // } else {
      //   RdKafka::Message *msg = consumer->consume(topic, partition, 1000);
      //   //msg_consume1(msg, NULL);
      //   delete msg;
      // }
      //  }
  
      // std::cout << "pid = " << ex_consume_cb.info.first << "\t" << "nev = " << ex_consume_cb.info.second << std::endl;
      // return ex_consume_cb.info.first;

    }



  private:

    std::string brokers;
    std::string topic_str;
    std::string errstr;
    std::string debug;
  
    int32_t partition = 0;//RdKafka::Topic::PARTITION_UA;
    int64_t start_offset = RdKafka::Topic::OFFSET_BEGINNING;
    int pid_ = -1;
    
    RdKafka::Conf *conf;
    RdKafka::Conf *tconf;
    RdKafka::Consumer *consumer;
    RdKafka::Topic *topic;
  
  };


} // namespace generator




