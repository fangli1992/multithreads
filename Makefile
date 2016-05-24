all:
		g++ main.cpp  -lboost_system -lboost_thread -o main.bin
clean:
		rm -fr *.out *.bin
