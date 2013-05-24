#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "donkey_core.h"

#define dlog1 printf
#define HOST "121.14.161.187"
#define PORT 8002

static DonkeyServer *server;

class MyConnection: public DonkeyBaseConnection {
  virtual enum READ_STATUS ReadCallback() {
    set_keep_alive(true);
    char *resp = (char *)evbuffer_pullup(get_input_buffer(), -1);
    dlog1(">>>>>>>>>>>>>>\n");
    dlog1("Connection::%s: %s\n", __func__, resp);
    dlog1("<<<<<<<<<<<<<<\n");
    evbuffer_drain(get_input_buffer(), -1);
    //server->Stop();
    return READ_ALL_DATA;
  }

  virtual void ConnectedCallback() {
    dlog1("new Connection fd:%d %s:%d\n", fd_, host_.c_str(), port_);
  }
  
  virtual void CloseCallback() {
    dlog1("Connection close fd:%d %s:%d\n",
        fd_, host_.c_str(), port_); 
    server->Stop();
  }

  virtual void ErrorCallback() {
    dlog1("Connection Error %s\n", this->get_error_string());
    server->Stop();
  }
};

static MyConnection *my_conn;

void send_tcp_request(const void *data, int len) {
  assert(data && len > 0); 

  if (!my_conn) {
    my_conn = new MyConnection();
    if (!my_conn) {
      dlog1("new PSConnection failed\n");
      return;
    }
   
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
      perror("socket()");
      return;
    }

    if (evutil_make_socket_nonblocking(fd) < 0)
	    return;

    struct sockaddr_in addr;
    int flags = 1;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(8001);
    
    //setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));

    if (bind(fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) == -1) 
    {
      perror("bind");
      return;
    }
    
    bool res = my_conn->Init(server->get_base(), fd, HOST, PORT);
    my_conn->Connect();
    if (!res) {
      delete my_conn;
      my_conn = NULL;
      dlog1("ps_conn_->Init failed\n");
      return;
    }
    my_conn->set_timeout(10);
    my_conn->set_keep_alive(true);
  }

  my_conn->set_timeout(10);
  my_conn->set_keep_alive(true);
  my_conn->Send(data, len);
}

int main(int argc, char **argv) {
  server = new DonkeyServer();

  if (!server || !server->Init()) {
    dlog1("new DonkeyServer Init error\n");
    return 1;
  }
  
  dlog1("server start ......\n");

  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  dlog1("begin http_load run ....\n");
  const char *buf = "GET / HTTP/1.1\r\nHost: dev5\r\nConnection: keep-alive\r\n\r\n";
  send_tcp_request(buf, strlen(buf)); 
  server->EventLoop();

  delete server;
}
