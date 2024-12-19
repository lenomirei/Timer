/*
 * @Author: lenomirei lenomirei@163.com
 * @Date: 2024-12-17 11:52:30
 * @FilePath: \Timer\Timer.cpp
 * @Description:
 *
 */
#include "Timer.h"

// Timer
Timer::Timer() {
}

Timer::~Timer() {
  Stop();
}

void Timer::Start(int delay_milisec, bool repeat, std::function<void()>&& callback) {
  user_callback_ = callback;
  delay_milisec_ = delay_milisec;
  repeat_ = repeat;
  StartInternal();
}

void Timer::OnTimeout() {
  if (repeat_) {
    auto user_callback = user_callback_;
    Reset();
    user_callback();
  } else {
    auto user_callback = std::move(user_callback_);
    impl_->Stop();
    std::move(user_callback)();
  }
}

void Timer::Stop() {
  impl_->Stop();
}

void Timer::Reset() {
  if (impl_) {
    impl_->Stop();
    impl_ = nullptr;
  }
  impl_ = std::make_shared<Timer::TimerImpl>(std::bind(&Timer::OnTimeout, this));
  impl_->Start(delay_milisec_);
  AddImplToManager();
}

bool Timer::IsActive() const {
  if (impl_) {
    return impl_->IsActive();
  } else {
    return false;
  }
}

int64_t Timer::RemainingTime() const {
  if (impl_) {
    return impl_->RemainingTime();
  } else {
    return -1;
  }
}

void Timer::StartInternal() {
  Reset();
}

void Timer::AddImplToManager() {
  // insert to manager
  TimerManager::GetInstance()->AddTimer(impl_);
}

//////////////////////////////////////////// TimerImpl

int Timer::TimerImpl::TimerId() const {
  return id_;
}

Timer::TimerImpl::TimerImpl(std::function<void()>&& callback)
    : callback_(std::move(callback)) {
  id_ = TimerManager::GetInstance()->GenerateTimerID();
}

Timer::TimerImpl::~TimerImpl() {
  Stop();
}

void Timer::TimerImpl::Start(int delay_milisec) {
  std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);
  active_ = true;
  next_notify_timepoint_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_milisec);
  writer_lck.unlock();
}

void Timer::TimerImpl::Stop() {
  std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);
  active_ = false;
}

bool Timer::TimerImpl::IsActive() const {
  std::shared_lock<std::shared_mutex> reader_lck(shared_mutex_);
  return active_;
}

int64_t Timer::TimerImpl::RemainingTime() const {
  if (IsActive()) {
    std::shared_lock<std::shared_mutex> reader_lck(shared_mutex_);
    std::chrono::steady_clock::duration duration = next_notify_timepoint_ - std::chrono::steady_clock::now();
    reader_lck.unlock();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  } else {
    return -1;
  }
}

std::chrono::milliseconds Timer::TimerImpl::RemainingTimeAsDuration() const {
  if (IsActive()) {
    std::shared_lock<std::shared_mutex> reader_lck(shared_mutex_);
    std::chrono::steady_clock::duration duration = next_notify_timepoint_ - std::chrono::steady_clock::now();
    reader_lck.unlock();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  } else {
    return std::chrono::milliseconds(-1);
  }
}

bool Timer::TimerImpl::operator<(const Timer::TimerImpl& timer) const {
  return this->RemainingTime() < timer.RemainingTime();
}

void Timer::TimerImpl::RunCallback() {
  {
    std::lock_guard<std::shared_mutex> reader_lck(shared_mutex_);
    if (!active_)
      return;
  }
  if (callback_) {
    callback_();
  }
}

// TimerManager

TimerManager::TimerManager()
    : g_timer_id_(0) {
  running_ = true;
  timer_thread_ = std::make_unique<std::thread>(&TimerManager::ThreadLoop, this);
}

TimerManager::~TimerManager() {
  Stop();
}

void TimerManager::ThreadLoop() {
  while (running_) {
    {
      // lock to make sure that wait and add will not execute at the same time
      std::unique_lock<std::mutex> lock(mutex_);
      if (!timer_queue_.empty()) {
        cond_.wait_for(lock, timer_queue_.top()->RemainingTimeAsDuration());
      } else {
        cond_.wait(lock);
      }
    }

    if (!running_)
      break;

    while (!timer_queue_.empty()) {
      std::unique_lock<std::mutex> lock(mutex_);
      const std::shared_ptr<Timer::TimerImpl> const timer = timer_queue_.top();
      if (timer->IsActive() && timer->RemainingTime() > 0) {
        break;
      }

      // must pop, priority can't change object's weight
      timer_queue_.pop();
      lock.unlock();
      if (!timer->IsActive()) {
        // drop this timer
        continue;
      }
      // todo worker thread or thread pool
      timer->RunCallback();
    }
  }
}

void TimerManager::Stop() {
  std::unique_lock<std::mutex> lock(mutex_);
  running_ = false;
  lock.unlock();

  cond_.notify_one();
  if (timer_thread_->joinable()) {
    timer_thread_->join();
  }
}

TimerManager* TimerManager::GetInstance() {
  static TimerManager g_timer_manager;
  return &g_timer_manager;
}

void TimerManager::AddTimer(const std::shared_ptr<Timer::TimerImpl>& timer_ptr) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    timer_queue_.push(timer_ptr);
  }

  cond_.notify_one();
}

int TimerManager::GenerateTimerID() {
  std::lock_guard<std::mutex> lock(mutex_);
  return g_timer_id_++;
}
