#include "Timer.h"

int TimerManager::g_timer_id_ = 0;
std::mutex TimerManager::mutex_;


// Timer
Timer::Timer()
    : impl_(std::make_shared<TimerImpl>())
{
}

Timer::~Timer()
{
    Stop();
}

void Timer::Start()
{
    if (!impl_->IsActive())
    {
        // insert to manager
        impl_->Start();
        TimerManager::GetInstance()->AddTimer(impl_);
    }
}

void Timer::Stop(bool sync)
{
    impl_->Stop(sync);
}

void Timer::SetInterval(int milsec)
{
    if (!impl_->IsActive())
    {
        impl_->SetInterval(milsec);
    }
    else
    {
        impl_->Stop();
        std::shared_ptr<TimerImpl> temp = std::make_shared<TimerImpl>();
        temp->SetInterval(milsec);
        temp->SetSingleShot(impl_->IsSingleShot());
        temp->SetTimeoutCallback(std::move(impl_->timeout_callback_));
        impl_ = temp;
        impl_->Start();
        TimerManager::GetInstance()->AddTimer(impl_);
    }
}

// TimerImpl

int Timer::TimerImpl::TimerId() const
{
    return id_;
}

Timer::TimerImpl::TimerImpl()
{
    id_ = TimerManager::GetInstance()->GenerateTimerID();
}

Timer::TimerImpl::~TimerImpl()
{
    Stop();
}

void Timer::TimerImpl::Start()
{
    std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);
    active_ = true;
    next_notify_timepoint_ = std::chrono::steady_clock::now() + interval_;
}

void Timer::TimerImpl::Stop(bool sync)
{
    std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);

    active_ = false;
    if (running_ && idle_promise_ && sync)
    {
        std::future<bool> idle = idle_promise_->get_future();
        // before wait must unlock or else AfterRun dead lock
        writer_lck.unlock();
        idle.wait();
    }
}

bool Timer::TimerImpl::IsActive() const
{
    std::shared_lock<std::shared_mutex> reader_lck(shared_mutex_);
    return active_;
}

void Timer::TimerImpl::SetInterval(int milsec)
{
    interval_ = std::chrono::milliseconds(milsec);
}

int Timer::TimerImpl::RemainingTime() const
{
    if (IsActive())
    {
        std::shared_lock<std::shared_mutex> reader_lck(shared_mutex_);
        std::chrono::steady_clock::duration duration = next_notify_timepoint_ - std::chrono::steady_clock::now();
        reader_lck.unlock();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
    else
    {
        return -1;
    }
}

std::chrono::milliseconds Timer::TimerImpl::RemainingTimeAsDuration() const
{
    if (IsActive())
    {
        std::shared_lock<std::shared_mutex> reader_lck(shared_mutex_);
        std::chrono::steady_clock::duration duration = next_notify_timepoint_ - std::chrono::steady_clock::now();
        reader_lck.unlock();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    }
    else
    {
        return std::chrono::milliseconds(-1);
    }
}

void Timer::TimerImpl::SetTimeoutCallback(std::function<void()>&& callback)
{
    timeout_callback_ = std::move(callback);
}

bool Timer::TimerImpl::operator<(const Timer::TimerImpl& timer) const
{
    return this->RemainingTime() < timer.RemainingTime();
}

void Timer::TimerImpl::operator()()
{
    if (!IsSingleShot())
    {
        std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);
        next_notify_timepoint_ = std::chrono::steady_clock::now() + interval_;
        writer_lck.unlock();
        TimerManager::GetInstance()->AddTimer(shared_from_this());
    }
    else
    {
        std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);
        active_ = false;
    }

    if (timeout_callback_)
    {
        BeforeRun();
        timeout_callback_();
        AfterRun();
    }
}

bool Timer::TimerImpl::IsSingleShot() const
{
    return is_single_shot_;
}

void Timer::TimerImpl::SetSingleShot(bool single_shot)
{
    is_single_shot_ = single_shot;
}

void Timer::TimerImpl::BeforeRun()
{
    std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);
    running_ = true;
    idle_promise_ = std::make_shared<std::promise<bool>>();
}

void Timer::TimerImpl::AfterRun()
{
    std::unique_lock<std::shared_mutex> writer_lck(shared_mutex_);
    running_ = false;
    idle_promise_->set_value(true);
    idle_promise_ = nullptr;
}

// TimerManager

TimerManager::TimerManager()
{
    running_ = true;
    timer_thread_ = std::make_unique<std::thread>(&TimerManager::Start, this);
}

TimerManager::~TimerManager()
{
    Stop();
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

        while (!timer_queue_.empty())
        {
            std::unique_lock<std::mutex> lck(mutex_);
            const std::shared_ptr<Timer::TimerImpl>& timer = timer_queue_.top();
            if (timer->IsActive() && timer->RemainingTime() > 0)
            {
                break;
            }
            
            std::shared_ptr<Timer::TimerImpl> ready_timer = timer_queue_.top();
            // must pop, priority can't change object's weight
            timer_queue_.pop();
            lck.unlock();
            RemoveTimer(ready_timer->TimerId());
            if (!ready_timer->IsActive())
            {
                continue;
            }
            // todo worker thread or thread pool
            (*ready_timer)();
            
        }
    }
}

void TimerManager::Stop()
{
    std::shared_ptr<Timer::TimerImpl> stop_timer = std::make_shared<Timer::TimerImpl>();
    stop_timer->SetSingleShot(true);
    stop_timer->SetInterval(0);
    stop_timer->SetTimeoutCallback(std::bind(&TimerManager::OnStop, this));
    stop_timer->Start();
    AddTimer(stop_timer);
}

void TimerManager::OnStop()
{
    running_ = false;
}

TimerManager* TimerManager::GetInstance()
{
    static TimerManager g_timer_manager;
    return &g_timer_manager;
}

void TimerManager::AddTimer(const std::shared_ptr<Timer::TimerImpl>& timer_ptr)
{
    {
        std::unique_lock<std::mutex> lck(mutex_);
        if (timer_map_.find(timer_ptr->TimerId()) != timer_map_.end())
        {
            // shouldn't insert timer
            return;
        }
        timer_map_.emplace(timer_ptr->TimerId(), timer_ptr);
        timer_queue_.push(timer_ptr);
    }

    cond_.notify_one();
}

void TimerManager::RemoveTimer(int timer_id)
{
    std::unique_lock<std::mutex> lck(mutex_);
    if (timer_map_.find(timer_id) == timer_map_.end())
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

