all: epoll_event.o server.o
	g++ -o server server.o epoll_event.o

epoll_event.o: epoll_event.cpp epoll_event.h
	g++ -g -c epoll_event.cpp

server.o: server.cpp server.h
	g++ -g -c server.cpp

clean:
	rm *.o server
