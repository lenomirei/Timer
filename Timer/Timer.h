#pragma once
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <queue>
#include <shared_mutex>
#include <thread>

class Timer {
 public:
  Timer();
  ~Timer();
  void Start(int delay_milisec, bool repeat, std::function<void()>&& callback);

  // do not use synchronous stop in timeout function
  void Stop();
  bool IsActive() const;
  int64_t RemainingTime() const;
  void OnTimeout();

 private:
  class TimerImpl : public std::enable_shared_from_this<TimerImpl> {
   public:
    TimerImpl(std::function<void()>&& user_callback);
    // TimerImpl(const TimerImpl& timer);
    ~TimerImpl();
    void Start(int delay_milisec);
    void Stop();
    bool Running() const { return running_; }
    bool IsActive() const;
    int TimerId() const;
    int64_t RemainingTime() const;
    std::chrono::milliseconds RemainingTimeAsDuration() const;

    bool operator<(const TimerImpl& timer) const;
    void RunCallback();

    class Cmp {
     public:
      bool operator()(const std::shared_ptr<TimerImpl>& timer_a, const std::shared_ptr<TimerImpl>& timer_b) {
        return timer_a->RemainingTime() > timer_b->RemainingTime();
      }
    };

   private:
    int id_;
    bool active_ = false;
    bool running_ = false;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> next_notify_timepoint_;
    mutable std::shared_mutex shared_mutex_;
    mutable std::mutex idle_mutex_;
    // wrapper callback
    std::function<void()> callback_ = nullptr;
    friend class Timer;
    friend class TimerManager;
  };

 private:
  void Reset();
  void StartInternal();
  void AddImplToManager();

 private:
  std::shared_ptr<TimerImpl> impl_ = nullptr;
  bool repeat_ = false;
  int delay_milisec_ = 0;
  std::function<void()> user_callback_ = nullptr;
  friend class TimerManager;
};

class TimerManager {
 public:
  ~TimerManager();
  static TimerManager* GetInstance();
  int GenerateTimerID();
  void AddTimer(const std::shared_ptr<Timer::TimerImpl>& timer_ptr);
  void Stop();

 private:
  TimerManager();
  // called by queue
  void ThreadLoop();

 private:
  int g_timer_id_;
  std::mutex mutex_;
  std::unique_ptr<std::thread> timer_thread_;
  std::priority_queue<std::shared_ptr<Timer::TimerImpl>, std::vector<std::shared_ptr<Timer::TimerImpl>>, Timer::TimerImpl::Cmp> timer_queue_;
  std::condition_variable cond_;
  bool running_ = false;
};