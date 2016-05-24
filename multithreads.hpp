#ifndef ZWFANG_MULTIPLE_THREADS_HPP_
#define ZWFANG_MULTIPLE_THREADS_HPP_

#include <queue>
#include <string>
#include <boost/shared_ptr.hpp> 
#include <boost/thread.hpp>
#include <vector>
#include <iostream>

// Disable the copy and assignment operator for a class.
#define DISABLE_COPY_AND_ASSIGN(classname) \
private:\
classname(const classname&);\
classname& operator=(const classname&)

namespace zwfang {/*{{{*/

template<typename T>
class BlockingQueue {/*{{{*/
 public:
  explicit BlockingQueue();

  void push(const T& t);

  bool try_pop(T* t);

  // This logs a message if the threads needs to be blocked
  // useful for detecting e.g. when data feeding is too slow
  T pop(const std::string& log_on_wait = "");

  bool try_peek(T* t);

  // Return element without removing it
  T peek();

  size_t size() const;

 protected:
  /**
   Move synchronization fields out instead of including boost/thread.hpp
   to avoid a boost/NVCC issues (#1009, #1010) on OSX. Also fails on
   Linux CUDA 7.0.18.
   */
  class sync;

  std::queue<T> queue_;
	boost::shared_ptr<sync> sync_;

DISABLE_COPY_AND_ASSIGN(BlockingQueue);
};/*}}}*/

template<typename T>
class BlockingQueue<T>::sync {/*{{{*/
 public:
  mutable boost::mutex mutex_;
  boost::condition_variable condition_;
};/*}}}*/

template<typename T>
BlockingQueue<T>::BlockingQueue()
    : sync_(new sync()) {
}

template<typename T>
void BlockingQueue<T>::push(const T& t) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);
  queue_.push(t);
  lock.unlock();
  sync_->condition_.notify_one();
}/*}}}*/

template<typename T>
bool BlockingQueue<T>::try_pop(T* t) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  if (queue_.empty()) {
    return false;
  }

  *t = queue_.front();
  queue_.pop();
  return true;
}/*}}}*/

template<typename T>
T BlockingQueue<T>::pop(const std::string& log_on_wait) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  while (queue_.empty()) {
    if (!log_on_wait.empty()) {
      //LOG_EVERY_N(INFO, 1000)<< log_on_wait;
    }
    sync_->condition_.wait(lock);
  }

  T t = queue_.front();
  queue_.pop();
  return t;
}/*}}}*/

template<typename T>
bool BlockingQueue<T>::try_peek(T* t) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  if (queue_.empty()) {
    return false;
  }

  *t = queue_.front();
  return true;
}/*}}}*/

template<typename T>
T BlockingQueue<T>::peek() {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  while (queue_.empty()) {
    sync_->condition_.wait(lock);
  }

  return queue_.front();
}/*}}}*/

template<typename T>
size_t BlockingQueue<T>::size() const {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);
  return queue_.size();
}/*}}}*/


template<typename T>
class SerialBlockingQueue {/*{{{*/
 public:
  explicit SerialBlockingQueue();

  void push(const size_t& order_in, const T& t);

  bool try_pop(size_t& order_out, T* t);

  // This logs a message if the threads needs to be blocked
  // useful for detecting e.g. when data feeding is too slow
  T pop(size_t& order_out, const std::string& log_on_wait = "");

  bool try_peek(size_t& order_out, T* t);

  // Return element without removing it
  T peek(size_t& order_out);

  size_t size() const;

 protected:
  /**
   Move synchronization fields out instead of including boost/thread.hpp
   to avoid a boost/NVCC issues (#1009, #1010) on OSX. Also fails on
   Linux CUDA 7.0.18.
   */
  class sync;

	size_t next_order_in;
	size_t next_order_out;
	const int thread_delay_msec;


  std::queue<T> queue_;
	boost::shared_ptr<sync> sync_;

DISABLE_COPY_AND_ASSIGN(SerialBlockingQueue);
};/*}}}*/

template<typename T>
class SerialBlockingQueue<T>::sync {/*{{{*/
 public:
  mutable boost::mutex mutex_;
  boost::condition_variable condition_;
};/*}}}*/

template<typename T>
SerialBlockingQueue<T>::SerialBlockingQueue()
    : sync_(new sync()) , next_order_in(0), next_order_out(0), thread_delay_msec(5){
}

template<typename T>
void SerialBlockingQueue<T>::push(const size_t& order_in, const T& t) {/*{{{*/
  sync_->mutex_.lock();
	//std::cout << order_in << "," << next_order_in << std::endl;
	while(next_order_in != order_in){
		if(order_in < next_order_in){
			// ignore invalid order
			sync_->mutex_.unlock();
			return;
		}
		int delay = (order_in - next_order_in + 1) * thread_delay_msec;
		sync_->mutex_.unlock();
		boost::this_thread::sleep(boost::posix_time::milliseconds(delay));
		sync_->mutex_.lock();
	}
	queue_.push(t);
	next_order_in ++;
	sync_->mutex_.unlock();
  sync_->condition_.notify_one();
}/*}}}*/

template<typename T>
bool SerialBlockingQueue<T>::try_pop(size_t& order_out, T* t) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  if (queue_.empty()) {
    return false;
  }

  *t = queue_.front();
  queue_.pop();
	order_out = next_order_out;
	next_order_out ++;
  return true;
}/*}}}*/

template<typename T>
T SerialBlockingQueue<T>::pop(size_t& order_out, const std::string& log_on_wait) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  while (queue_.empty()) {
    if (!log_on_wait.empty()) {
      //LOG_EVERY_N(INFO, 1000)<< log_on_wait;
    }
    sync_->condition_.wait(lock);
  }

  T t = queue_.front();
  queue_.pop();
	order_out = next_order_out;
	next_order_out ++;
  return t;
}/*}}}*/

template<typename T>
bool SerialBlockingQueue<T>::try_peek(size_t& order_out, T* t) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  if (queue_.empty()) {
    return false;
  }

  *t = queue_.front();
	order_out = next_order_out;
  return true;
}/*}}}*/

template<typename T>
T SerialBlockingQueue<T>::peek(size_t& order_out) {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);

  while (queue_.empty()) {
    sync_->condition_.wait(lock);
  }
	order_out = next_order_out;
  return queue_.front();
}/*}}}*/

template<typename T>
size_t SerialBlockingQueue<T>::size() const {/*{{{*/
  boost::mutex::scoped_lock lock(sync_->mutex_);
  return queue_.size();
}/*}}}*/


class BlockingFlag {/*{{{*/
public:
	BlockingFlag(bool flag = false): flag_(flag), mutex_(){}
	virtual ~BlockingFlag(){}
	bool flag(){
		boost::mutex::scoped_lock lock(mutex_);
		return flag_;
	}

	void set_true()
	{
		boost::mutex::scoped_lock lock(mutex_);
		flag_ = true;	
	}

	void set_false()
	{
		boost::mutex::scoped_lock lock(mutex_);
		flag_ = false;	
	}
private:
	bool flag_;
	boost::mutex mutex_;
	DISABLE_COPY_AND_ASSIGN(BlockingFlag);
};/*}}}*/



class MultipleThreads{/*{{{*/
public:
MultipleThreads() : threads_() {}
virtual ~MultipleThreads(){ StopThreads(); }

void StartThreads(int num = 1);
void StopThreads(){/*{{{*/
	for (int i = 0; i < threads_.size(); i++){
		if(!stop_flags_[i]->flag()){
			stop_flags_[i]->set_true();
			threads_[i]->join();
		}
	}
	stop_flags_.clear();
	threads_.clear();
}/*}}}*/

void Join(){
	for(int i = 0; i < threads_.size(); i++){
		if(!stop_flags_[i]->flag()){
			threads_[i]->join();
		}
	}
}

protected:
	virtual void entry(int threadIdx){}

protected:
	std::vector<boost::shared_ptr<boost::thread> > threads_;
	std::vector<boost::shared_ptr<BlockingFlag> > stop_flags_;
};/*}}}*/

void MultipleThreads::StartThreads(int num)
{/*{{{*/
	if(num < 1)
	{
		return;
	}
	stop_flags_.resize(num);
	threads_.resize(num);
	for (int i = 0; i < num; i++){
		stop_flags_[i].reset(new BlockingFlag(false));
		threads_[i].reset(new boost::thread(&MultipleThreads::entry, this, i));
	}
}/*}}}*/
	

}/*}}}*/

#endif
