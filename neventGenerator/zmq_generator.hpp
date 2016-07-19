#ifndef _ZMQ_GENERATOR_H
#define _ZMQ_GENERATOR_H

#include <string>
#include <sstream>
#include <exception>
#include <utility>
#include <assert.h>

#include <zmq.h>

#include "serialiser.hpp"
#include "uparam.hpp"

extern "C" {
#include "cJSON/cJSON.h"
}

namespace generator {

  /*! \struct ZmqGen
   *  Uses 0MQ as data the streamer.
   *
   *  \author Michele Brambilla <mib.mic@gmail.com>
   *  \date Wed Jun 08 15:19:27 2016
   * @tparam mode_selector transmitter = 0, receiver = 1
   */
  template<int mode_selector> // mode_selector = 0 -> transmitter, = 1 -> receiver 
  struct ZmqGen {
    static const int max_header_size=10000;
    static const int max_recv_size=100000000;

  
    ZmqGen(uparam::Param p ) {
      /*! @param p see uparam::Param for description. Must contain "port" key-value */
      /*! Generates the 0MQ context and bind PULL socket to "port". If binding
        fails throws an error */
      context = zmq_ctx_new ();
      int rc;
      if(!mode_selector) {
        socket = zmq_socket (context, ZMQ_PUSH);
        std::cout << "tcp://*:"+p["port"] << std::endl;
        rc = zmq_bind(socket,("tcp://*:"+p["port"]).c_str());
        //    socket.setsockopt(zmq::SNDHWM, 100);
      }
      else {
        socket = zmq_socket (context, ZMQ_PULL);
        std::cout << "tcp://" << p["host"] << ":" << p["port"] << std::endl;
        rc = zmq_connect(socket,("tcp://"+p["host"]+":"+p["port"]).c_str());
      }
      assert (rc == 0);
    }

    /// \brief 
    template<typename T>
    void send(const T* data, int size, const int flag = 0) {
      /*! @tparam T data type
       *  @param data data to be sent
       *  @param size number of elements
       *  @param flag optional flag (default = 0) */
      std::cout << "->" << data
                << "," << size
                << std::endl;
      zmq_send(socket,data,size,flag);
      std::cout << std::endl;
    }

    template<typename T>
    void send(std::string h, T* data, int nev, serialiser::NoSerialiser<T>) {
      /*! @tparam T data type
       *  @param h data header
       *  @param data pointer to data array to be sent
       *  @param nev number of elements in data array */
      /*! Sends two different messages for header and data. If there are no
          events, data is not sent */

      if(nev > 0) {
        zmq_send(socket,&h[0],h.size(),ZMQ_SNDMORE);
        zmq_send(socket,data,nev*sizeof(T),0);
      }
      else {
        zmq_send(socket,&h[0],h.size(),0);
      }
      std::cout << "->" << h
                << "," << h.size()
                << "," << nev
                << std::endl;    
    }
  
    template<typename T>
    void send(std::string h, T* data, int nev, serialiser::FlatBufSerialiser<T>) {
      /*! @tparam T data type
       *  @param h data header
       *  @param data pointer to data array to be sent
       *  @param nev number of elements in data array */
      /*! Serialises the message using FlatBuffers and sends it */
      serialiser::FlatBufSerialiser<T> s;
      zmq_send(socket,(char*)s(h.c_str(),data,nev),s.size(),0);
      std::cout << "->" << h
                << "," << h.size()
                << "," << nev
                << std::endl;    
    
      std::ofstream of("first.out",std::ofstream::binary);
      of.write((char*)s(h.c_str(),data,nev),s.size()); 
      of.close();
    }
  

    template<typename T>
    int recv(std::string& h, std::vector<T>& data, serialiser::NoSerialiser<T>) {
      /*! @tparam T data type
       *  @param h string containing received data header
       *  @param data vector containing received event data
       *  @result pid packet ID */
      /*! Receives the message split into header and events. Parse header to
          determine the expected number of events. If equal 0 does not wait for
          events. If greater than 0 and ZMQ_RCVMORE == true receives event
          data. Otherwise throws an error.
      */
      int rcvmore;
      int s = zmq_recv (socket,&h[0],max_header_size,0);
      zmq_getsockopt(socket, ZMQ_RCVMORE, &rcvmore , &optlen);

      auto info = parse_header(h);
      std::cout << " pid =  " << info.first << std::endl;

      
      if(rcvmore && (info.second > 0) ) {
        if ( info.second > data.size() ) {
          data.resize(info.second);
        }
        int s = zmq_recv (socket,&d[0],info.second*sizeof(T),0);
      }
      else
        if( rcvmore != info.second )
          std::cout << "Error receiving data: "
                    << "recvmore = " << rcvmore
                    << "info.second = " << info.second
                    <<  std::endl;
      return info.first;
    }



    template<typename T>
    int recv(std::string& h, std::vector<T>& data, serialiser::FlatBufSerialiser<T>) {
      /*! @tparam T data type
       *  @param h string containing received data header
       *  @param data vector containing received event data
       *  @result pid packet ID */
      /*! Receives the serialised message and unserialise it. */
      if (!raw) {
        raw = new char [max_recv_size];
      }
      int n = zmq_recv (socket,raw,max_header_size,0);
      
      serialiser::FlatBufSerialiser<T> s;
      s.extract(raw,h,data);

      auto info = parse_header(h);
      std::cout << " pid =  " << info.first << std::endl;
      std::cout << "n events = " << info.second << std::endl; 
        
      return info.first;
    }

  private:
    void* context; /// pointer to 0MQ context 
    void* socket;  /// pointer to 0MQ socket 
    char* d = NULL;
    char* raw = NULL;
    int max_event_size = 0;
    size_t optlen=sizeof(int);

    std::pair<int,int> parse_header(std::string& s) {
      std::pair<int,int> result;
      cJSON* root = NULL;
      root = cJSON_Parse(s.c_str());
      if( root == 0 ) {
        //      throw std::runtime_error("can't parse header");
        std::cout << "Error: can't parse header" << std::endl;
        exit(-1);
      }
      
      result.first = cJSON_GetObjectItem(root,"pid")->valuedouble;
      cJSON* item = cJSON_GetObjectItem(root,"ds");
      result.second = cJSON_GetArrayItem(item,1) -> valuedouble;

      std::cout << "parser: " << result.first << "\t" << result.second << std::endl;
      return result;
    }
    
  };
  

  
} // namespace generator
  
#endif //ZMQ_GENERATOR_H
