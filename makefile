all: client sever
sever:sever.cpp
	g++ -std=c++0x $^ -o $@ -lboost_filesystem -lboost_system -lpthread
client:client.cpp
	g++ -std=c++0x $^ -o $@ -lboost_filesystem -lboost_system -lpthread -lboost_thread
