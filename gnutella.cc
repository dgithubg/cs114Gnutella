#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <vector>
#include <cstdlib>
#include <string.h>
#include <string>
#include <fstream>
#include <ctime>
#include "descriptor_header.h"

#define DEFAULT_PORT 11111
#define BUFFER_SIZE 1024
#define MAX_PEERS 7

using namespace std;

// Contains an address and port in big-endian format
typedef struct peer {
  unsigned long address;
  unsigned short port;
} peer_t;

string get_time() {
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];
	time (&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, 80, "%m/%d/%y %H:%M:%S", timeinfo);

	return string(buffer);
}

class Gnutella {
private:
  int m_socket;      // Holds the socket descriptor
  vector<peer_t> m_peers;  // A list of peers that this node knows about
  int m_port;        // The port that this node will listen on, in little-endian format
  fstream m_log;
  
  void error(string msg) {
    m_log << "[ERR " << get_time() << "] " << msg << ": " << strerror(errno) << endl;
    exit(1);
  }
  
  void log(string msg) {
    m_log << "[LOG " << get_time() << "] " << msg << endl;
  }

public:
  Gnutella(int port = DEFAULT_PORT) {
    m_port = port;
    char str[15];
    sprintf(str,"logs/log_%d",m_port);
    m_log.open(str,ios::out);
    if(!m_log.is_open()) {
      cerr << "Could not open log file: " << strerror(errno) << endl;
      exit(1);
    }
  }

  ~Gnutella() {
    m_log.close();
  }

  void acquireSocket() {
    // Acquire the socket
    m_socket = socket(PF_INET, SOCK_STREAM, 0);

    if (m_socket == -1) {
      error("Could not acquire socket");
    }
  }

  void acceptConnections() {
    acquireSocket();

    // Bind the socket to the port
    sockaddr_in nodeInfo;
    memset(&nodeInfo, 0, sizeof(nodeInfo));
    nodeInfo.sin_family = AF_INET;
    nodeInfo.sin_addr.s_addr = INADDR_ANY;
    nodeInfo.sin_port = htons(m_port);

    int status = bind(m_socket, (sockaddr *) &nodeInfo, sizeof(nodeInfo));

    if (status == -1) {
      error("Could not bind to socket");
    }

    // Listen for connections on the socket
    status = listen(m_socket, 10000);

    if (status == -1) {
      error("Could not listen on socker");
    }

    // Continuously accept connections
    while (true) {
      sockaddr_in remoteInfo;
      memset(&remoteInfo, 0, sizeof(remoteInfo));
      socklen_t addrLength;
      int connection = accept(m_socket, (sockaddr *) &remoteInfo, &addrLength);

      // Create a buffer for a received message
		char header[HEADER_SIZE];
	readMessageHeader(header, connection);

      // Handle responses
      if (strcmp(header, "GNUTELLA CONNECT/0.4\n\n") == 0) {
        handleConnectRequest(connection, remoteInfo);
      }
      else {
        // No Response
      }

      close(connection);
    }

    // Close the socket
    close(m_socket);
  }

  void handleConnectRequest(int connection, sockaddr_in remoteInfo) {
    log("Received connect request.");
    char connectResponse[] = "GNUTELLA OK\n\n";

    // Check if the peer list is full
    send(connection, connectResponse, sizeof(connectResponse), 0);
  }

  void bootstrap(const char *address, int port) {
    acquireSocket();

    // Attempt to connect to the address/port
    sockaddr_in nodeInfo;
    memset(&nodeInfo, 0, sizeof(nodeInfo));
    nodeInfo.sin_family = AF_INET;
    nodeInfo.sin_addr.s_addr = inet_addr(address);
    nodeInfo.sin_port = htons(port);

    int status = connect(m_socket, (sockaddr *) &nodeInfo, sizeof(nodeInfo));

    if (status == -1) {
      error("Could not connect to boostrap host");
    }

    char request[] = "GNUTELLA CONNECT/0.4\n\n";

    // Send a connect request
    log("Sending bootstrap connect request.");
    send(m_socket, request, sizeof(request), 0);

    // Create a buffer for a received message
		char header[HEADER_SIZE];
	readMessageHeader(header, m_socket);

    // If the host replies, add it as a new peer
    if (strcmp(header, "GNUTELLA OK\n\n") == 0) {
      peer_t peer;
      peer.address = nodeInfo.sin_addr.s_addr;
      peer.port = nodeInfo.sin_port;
      m_peers.push_back(peer);

      log("Connected to bootstrap host.");
    }
    else {
      log("The bootstrap host rejected the connect request.");
    }

    // Close the connection and reinitialize the socket
    close(m_socket);
  }

	void readMessageHeader(char *buffer, int connection) {
		memset(buffer, 0, HEADER_SIZE);
		int used = 0;
		int remaining = HEADER_SIZE - 1;
		
		while (remaining > 0) {
			int bytesRead = recv(connection, &buffer[used], remaining, 0);
			used += bytesRead;
			remaining -= bytesRead;
			buffer[used] = '\0';
			
			string str(buffer);
			int pos = str.find("\n\n");	
			
			if (pos != -1) {
				memset(buffer, 0, sizeof(buffer));
				strcpy(buffer, str.substr(0, pos + 2).c_str());
				break;
			}
		}
	}
};

int main(int argc, char **argv) {
  // Check if arguments passed
  Gnutella *node;
  if (argc >= 2) {
    node = new Gnutella(atoi(argv[1]));
  }
  else {
    node = new Gnutella();
  }

  if (argc >= 4) {
    node->bootstrap(argv[2], atoi(argv[3]));
  }

  node->acceptConnections();
  delete node;

  return 0;
}
