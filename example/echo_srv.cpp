
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "donkey_core.h"
#include "log.h"

#define RESP_DATA "'HTTP/1.1 200 OK\r\nServer: nginx/0.8.44\r\nContent-length: 0\r\nConnection: keep-alive\r\n\r\n"

using namespace std;

class MyServer;

static MyServer *server;

class SrvConnection: public DonkeyBaseConnection {

  virtual void ConnectedCallback() {
    //dlog1("new SrvConnection fd:%d %s:%d\n", fd_, host_.c_str(), port_);
  }
  
  virtual void CloseCallback() {
    dlog1("SrvConnection close fd:%d %s:%d\n",
        fd_, host_.c_str(), port_); 
  }

  virtual void ErrorCallback() {
    dlog1("SrvConnection Error %s\n", this->get_error_string());
  }

  virtual void WriteCallback() {
    //dlog1("SrvConnection %s\n", __func__);

  }

  virtual enum READ_STATUS ReadCallback() {
    struct evbuffer *inbuf = get_input_buffer();
    size_t inbuf_size = evbuffer_get_length(inbuf);
    
    //dlog1("ReadCallback %d\n", inbuf_size);
    if (inbuf_size <= 0)
      return READ_BAD_CLIENT;
    
    char buf[1024];
    int nread = evbuffer_remove(inbuf, buf, sizeof(buf));
     
    if (nread > 0) {
      
      if (strncmp(buf, "quit", 4) == 0) {
        /* close connection after response */
        set_keep_alive(false);
      } else {
        set_keep_alive(true);
      }
      //Send(buf, nread);
      Send(RESP_DATA, strlen(RESP_DATA));
    } else
      return READ_BAD_CLIENT;
    
    return READ_ALL_DATA;
  }
};

class MyServer: public DonkeyServer {
  virtual void ClockCallback() {
    dlog1("conns: %d\n", this->get_conns_map_size()); 
  }

  virtual DonkeyBaseConnection *NewConnection() {
    return new SrvConnection(); 
  }

  virtual void ConnectionMade(DonkeyBaseConnection *conn) {
  }
};

#define PORT 60006

int main(int argc, char **argv) {
  server = new MyServer();

  int rv = LOGGER->Create("./echo_srv_",
                      "./echo_srv_debug_", 
                      false,
                      true,
                      3,
                      1024*1024*1024,
                      1024*1024*1024);
  
  if (rv < 0) {
    perror("create logger error");
    exit(1);
  }

  if (!server || !server->Init()) {
    dlog1("new DonkeyServer Init error\n");
    return 1;
  }
  
  dlog1("server start ......\n");

  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  int port = PORT;
  if (argc > 1) {
    port = atoi(argv[1]);
  }

  if (!server->StartListenTCP(NULL, port, 1024)) {
    dlog1("StartListenTCP fail (:%d)\n", port);
    return 1;
  }

  server->set_timeout(10);
  server->EventLoop();

  delete server;
}
