#pragma once
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
namespace retro {
// One persistent worker: no per-frame thread creation. GPU command recording
// stays on the render thread; the independent arm rig can run alongside it.
class FrameWorker {
 std::mutex mutex;
 std::condition_variable changed;
 std::function<void()> job;
 std::exception_ptr failure;
 bool busy=false,stopping=false;
 std::thread thread;
public:
 FrameWorker():thread([this]{
  std::unique_lock lock(mutex);
  for(;;){changed.wait(lock,[&]{return stopping||bool(job);});if(stopping)return;
   auto work=std::move(job);job={};lock.unlock();std::exception_ptr error;
   try{work();}catch(...){error=std::current_exception();}
   lock.lock();failure=error;busy=false;changed.notify_all();
  }
 }){}
 ~FrameWorker(){ {std::unique_lock lock(mutex);changed.wait(lock,[&]{return !busy;});stopping=true;}changed.notify_all();thread.join();}
 void start(std::function<void()> work){std::unique_lock lock(mutex);changed.wait(lock,[&]{return !busy;});failure=nullptr;job=std::move(work);busy=true;lock.unlock();changed.notify_all();}
 void wait(){std::unique_lock lock(mutex);changed.wait(lock,[&]{return !busy;});auto error=std::exchange(failure,nullptr);lock.unlock();if(error)std::rethrow_exception(error);}
};
}
