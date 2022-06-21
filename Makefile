all: myweb

myweb: mywebserver.cpp http_conn/http_conn.cpp
	g++ mywebserver.cpp http_conn/http_conn.cpp -o myweb -lpthread

clean:
	rm myweb