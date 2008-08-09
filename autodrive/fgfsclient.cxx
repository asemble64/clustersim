// $Id: fgfsclient.cxx,v 1.4 2006/11/23 09:26:43 mfranz Exp $
// g++ -O2 -g -pedantic -Wall fgfsclient.cxx -o fgfsclient -lstdc++
// USAGE: ./fgfsclient [hostname [port]]
// Public Domain

#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>


const char *HOST = "localhost";
const unsigned PORT = 5501;
const int BUFLEN = 256;


class FGFSSocket {
public:
	FGFSSocket(const char *name, unsigned port);
	~FGFSSocket();

	int		write(const char *msg, ...);
	const char	*read(void);
	inline void	flush(void);
	void		settimeout(unsigned t) { _timeout = t; }

private:
	int		close(void);

	int		_sock;
	bool		_connected;
	unsigned	_timeout;
	char		_buffer[BUFLEN];
};


FGFSSocket::FGFSSocket(const char *hostname = HOST, unsigned port = PORT) :
	_sock(-1),
	_connected(false),
	_timeout(1)
{
	_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sock < 0)
		throw("FGFSSocket/socket");

	struct hostent *hostinfo;
	hostinfo = gethostbyname(hostname);
	if (!hostinfo) {
		close();
		throw("FGFSSocket/gethostbyname: unknown host");
	}

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr = *(struct in_addr *)hostinfo->h_addr;

	if (connect(_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close();
		throw("FGFSSocket/connect");
	}
	_connected = true;
	try {
		write("data");
	} catch (...) {
		close();
		throw;
	}
}


FGFSSocket::~FGFSSocket()
{
	close();
}


int FGFSSocket::close(void)
{
	if (_connected)
		write("quit");
	if (_sock < 0)
		return 0;
	int ret = ::close(_sock);
	_sock = -1;
	return ret;
}


int FGFSSocket::write(const char *msg, ...)
{
	va_list va;
	ssize_t len;
	char buf[BUFLEN];
	fd_set fd;
	struct timeval tv;

	FD_ZERO(&fd);
	FD_SET(_sock, &fd);
	tv.tv_sec = _timeout;
	tv.tv_usec = 0;
	if (!select(FD_SETSIZE, 0, &fd, 0, &tv))
		throw("FGFSSocket::write/select: timeout exceeded");

	va_start(va, msg);
	vsnprintf(buf, BUFLEN - 2, msg, va);
	va_end(va);
	std::cout << "SEND: " << buf << std::endl;
	strcat(buf, "\015\012");

	len = ::write(_sock, buf, strlen(buf));
	if (len < 0)
		throw("FGFSSocket::write");
	return len;
}


const char *FGFSSocket::read(void)
{
	char *p;
	fd_set fd;
	struct timeval tv;
	ssize_t len;

	FD_ZERO(&fd);
	FD_SET(_sock, &fd);
	tv.tv_sec = _timeout;
	tv.tv_usec = 0;
	if (!select(FD_SETSIZE, &fd, 0, 0, &tv)) {
		if (_timeout == 0)
			return 0;
		else
			throw("FGFSSocket::read/select: timeout exceeded");
	}

	len = ::read(_sock, _buffer, BUFLEN - 1);
	if (len < 0)
		throw("FGFSSocket::read/read");
	if (len == 0)
		return 0;

	for (p = &_buffer[len - 1]; p >= _buffer; p--)
		if (*p != '\015' && *p != '\012')
			break;
	*++p = '\0';
	return strlen(_buffer) ? _buffer : 0;
}


inline void FGFSSocket::flush(void)
{
	int i = _timeout;
	_timeout = 0;
	while (read())
		;
	_timeout = i;
}


int main(const int argc, const char *argv[])
  try {
     const char *hostname = argc > 1 ? argv[1] : "localhost"; //assumes localhost if nothing specified
     int port = argc > 2 ? atoi(argv[2]) : 5501; // always assumes port 5501
     
     FGFSSocket f(hostname, port);  //Connect to flightgear
     f.flush();
     f.write("get /sim/aircraft");  //read aircraft type
     const char *p = f.read(); // process returned text
     if (p) std::cout << "RECV: " << p << std::endl;
   
     //SPECIAL STUFF FOR UFO
     /*if (p=="ufo"){
	f.write("set /controls/engines/engine/throttle %lg", 0.13);
     }
     else{
	f.write("set /controls/engines/engine/throttle %lg", 1.0);
     }
     */

     //SET SOME WAYPOINTS: set /autopilot/route-manager/input @insert-1: lon,lat@alt
     
     f.write("set /autopilot/route-manager/input @clear");//clear route
     f.write("set /autopilot/route-manager/input @insert-1:151.179,-33.94@2000");//append waypoint
     f.write("set /autopilot/route-manager/input @insert-1:YSBK@1000");//append bankstown airport
     f.write("set /autopilot/route-manager/input @insert-0:YSSY@200");//push to top

     //READ CURRENT POSITION
     f.write("get /position/longitude-deg"); const char *longitude_pos = f.read();
     std::cout << "RECV Longitude: " << longitude_pos << std::endl;
     f.write("get /position/latitude-deg"); const char *latitude_pos = f.read();
     std::cout << "RECV Latitude: " << latitude_pos << std::endl;
     f.write("get /position/altitude-ft"); const char *altitude_ft = f.read();
     std::cout << "RECV Altitude: " <<  altitude_ft << std::endl;
     
     return EXIT_SUCCESS;

} catch (const char s[]) {
	std::cerr << "Error: " << s << ": " << strerror(errno) << std::endl;
	return EXIT_FAILURE;

} catch (...) {
	std::cerr << "Error: unknown exception" << std::endl;
	return EXIT_FAILURE;
}


// vim:cindent
