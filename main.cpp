#include "multithreads.hpp"
#include <iostream>

boost::mutex MyMutex;
class MyThreads : public zwfang::MultipleThreads{
	public:
		void entry(int num){

			while(!stop_flags_[num]->flag()){
				MyMutex.lock();
				std::cout << num << std::endl;
				MyMutex.unlock();
			}
		}
};

int main()
{
	MyThreads mt2;
	mt2.StartThreads(5);
	mt2.StopThreads();
}
