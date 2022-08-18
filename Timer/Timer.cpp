#include "Timer.h"

TimerManager* TimerManager::g_timer_manager = nullptr;
int TimerManager::g_timer_id_ = 0;
std::mutex TimerManager::mutex_;

int Timer::TimerId() const
{
    return id_;
}

Timer::Timer()
{
    id_ = TimerManager::GetInstance()->GenerateTimerID();
}

Timer::Timer(const Timer& timer)
{
    this->id_ = timer.id_;
    this->is_single_shot_ = timer.is_single_shot_;
    this->active_ = timer.active_;
    this->interval_ = timer.interval_;
    this->next_notify_timepoint_ = timer.next_notify_timepoint_;
    this->timeout_callback_ = timer.timeout_callback_;
}

Timer& Timer::operator =(const Timer& timer)
{
    this->id_ = timer.id_;
    this->is_single_shot_ = timer.is_single_shot_;
    this->active_ = timer.active_;
    this->interval_ = timer.interval_;
    this->next_notify_timepoint_ = timer.next_notify_timepoint_;
    this->timeout_callback_ = timer.timeout_callback_;
    return *this;
}

Timer::~Timer()
{
    active_ = false;
}

void Timer::Start()
{
    active_ = true;
    next_notify_timepoint_ = std::chrono::steady_clock::now() + interval_;
    // insert to manager
    TimerManager::GetInstance()->AddTimer((*this));
}

void Timer::Stop()
{
    active_ = false;
}

bool Timer::IsActive() const
{
    // todo active_ need lock£¿
    return active_;
}

void Timer::SetInterval(int msec)
{
    interval_ = std::chrono::milliseconds(msec);
}

int Timer::RemainingTime() const
{
    if (IsActive())
    {
        std::chrono::steady_clock::duration duration = next_notify_timepoint_ - std::chrono::steady_clock::now();
        return duration.count();
    }
    else
    {
        return -1;
    }
}

std::chrono::milliseconds Timer::RemainingTimeAsDuration() const
{
    if (IsActive())
    {
        std::chrono::steady_clock::duration duration = next_notify_timepoint_ - std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    }
    else
    {
        return std::chrono::milliseconds(-1);
    }
}

void Timer::SetTimeoutCallback(std::function<void()>&& callback)
{
    timeout_callback_ = std::move(callback);
}

bool Timer::operator<(const Timer& timer) const
{
    return this->RemainingTime() < timer.RemainingTime();
}

const std::function<void()>& Timer::operator()() const
{
    return timeout_callback_;
}

bool Timer::IsSingleShot() const
{
    return is_single_shot_;
}

void Timer::SetSingleShot(bool single_shot)
{
    is_single_shot_ = single_shot;
}

void Timer::Repeat()
{
    next_notify_timepoint_ = std::chrono::steady_clock::now() + interval_;
}

// TimerManager

TimerManager::TimerManager()
{
    running_ = true;
    timer_thread_ = std::make_unique<std::thread>(&TimerManager::Start, this);
}

TimerManager::~TimerManager()
{
    running_ = false;
    cond_.notify_one();
    if (timer_thread_->joinable())
    {
        timer_thread_->join();
    }
    //if (worker_thread_->joinable())
    //{
    //    worker_thread_->join();
    //}
}

void TimerManager::Start()
{
    while (running_)
    {
        while (!timer_queue_.empty())
        {
            std::unique_lock<std::mutex> lck(mutex_);
            const std::shared_ptr<Timer>& timer = timer_queue_.top();
            if (timer->IsActive() && timer->RemainingTime() > 0)
            {
                break;
            }
            
            const std::shared_ptr<Timer>& ready_timer = timer_queue_.top();
            // must pop, priority can't change object's weight
            timer_queue_.pop();
            lck.unlock();
            if (!ready_timer->IsActive())
            {
                RemoveTimer(ready_timer->TimerId());
                continue;
            }
            // todo worker thread or thread pool
            (*ready_timer)()();
            
            if (!ready_timer->IsSingleShot())
            {
                ready_timer->Repeat();
                // repeat must push again
                timer_queue_.push(ready_timer);
            }
            else
            {
                RemoveTimer(ready_timer->TimerId());
            }
        }

        {
            // lock to make sure that wait and add will not execute at the same time
            std::unique_lock<std::mutex> lck(mutex_);
            if (!timer_queue_.empty())
            {
                cond_.wait_for(lck, timer_queue_.top()->RemainingTimeAsDuration());
            }
            else
            {
                cond_.wait(lck);
            }
        }
    }
}

TimerManager* TimerManager::GetInstance()
{
    std::unique_lock<std::mutex> lck(mutex_);
    if (!g_timer_manager)
    {
        g_timer_manager = new TimerManager();
        static Destroyer d;
    }

    return g_timer_manager;
}

void TimerManager::AddTimer(const Timer& timer)
{
    {
        std::unique_lock<std::mutex> lck(mutex_);
        std::shared_ptr<Timer> new_timer = std::make_shared<Timer>(timer);
        timer_map_.emplace(new_timer->TimerId(), new_timer);
        timer_queue_.push(new_timer);
    }

    cond_.notify_one();
}

void TimerManager::RemoveTimer(int timer_id)
{
    std::unique_lock<std::mutex> lck(mutex_);
    if (timer_map_.find(timer_id) != timer_map_.end())
    {
        // Something wrong maybe happened;
        return;
    }

    timer_map_.erase(timer_id);
}

int TimerManager::GenerateTimerID()
{
    std::unique_lock<std::mutex> lck(mutex_);
    return g_timer_id_++;
}