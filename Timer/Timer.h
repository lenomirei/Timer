#pragma once
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <map>
#include <functional>

class Timer
{
public:
    Timer();
    ~Timer();
    void Start();
    void Stop();
    void SetInterval(int milsec);
    bool IsActive() const { return impl_->IsActive(); }
    int RemainingTime() const { return impl_->RemainingTime(); }
    bool IsSingleShot() const { return impl_->IsSingleShot(); }
    void SetSingleShot(bool single_shot) { impl_->SetSingleShot(single_shot); }
    void SetTimeoutCallback(std::function<void()>&& callback) { impl_->SetTimeoutCallback(std::move(callback)); }

private:
    class TimerImpl
    {
    public:
        TimerImpl();
        TimerImpl(const TimerImpl& timer);
        ~TimerImpl();
        void Start();
        void Stop();
        void SetInterval(int milsec);
        bool IsActive() const;
        int TimerId() const;
        int RemainingTime() const;
        void SetTimeoutCallback(std::function<void()>&& callback);
        std::chrono::milliseconds RemainingTimeAsDuration() const;
        bool IsSingleShot() const;
        void SetSingleShot(bool single_shot);


        bool operator<(const TimerImpl& timer) const;
        const std::function<void()>& operator()();

        class Cmp
        {
        public:
            bool operator()(const std::shared_ptr<TimerImpl>& timer_a, const std::shared_ptr<TimerImpl>& timer_b)
            {
                return timer_a->RemainingTime() > timer_b->RemainingTime();
            }
        };

    private:
        int id_;
        bool is_single_shot_ = true;
        std::chrono::milliseconds interval_ = std::chrono::milliseconds(0);
        bool active_ = false;
        std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> next_notify_timepoint_;
        std::mutex mutex_;
        // callback
        std::function<void()> timeout_callback_ = nullptr;
        friend class Timer;
        friend class TimerManager;
    };

    std::shared_ptr<TimerImpl> impl_ = nullptr;
    friend class TimerManager;
};

class TimerManager
{
public:
    ~TimerManager();
    static TimerManager* GetInstance();
    static int GenerateTimerID();
    void AddTimer(const Timer::TimerImpl& timer);
    void AddTimer(const std::shared_ptr<Timer::TimerImpl>& timer_ptr);
    void Start();
    void Stop();

private:
    TimerManager();
    // called by queue
    void RemoveTimer(int timer_id);
    void OnStop();

    class Destroyer
    {
    public:
        ~Destroyer()
        {
            delete g_timer_manager;
            g_timer_manager = nullptr;
        }
    };

private:
    static TimerManager* g_timer_manager;
    static std::mutex mutex_;
    static int g_timer_id_;
    std::unique_ptr<std::thread> timer_thread_;
    std::unique_ptr<std::thread> worker_thread_;
    std::map<int/* timer id */, std::shared_ptr<Timer::TimerImpl>> timer_map_;
    std::priority_queue<std::shared_ptr<Timer::TimerImpl>, std::vector<std::shared_ptr<Timer::TimerImpl>>, Timer::TimerImpl::Cmp> timer_queue_;
    std::condition_variable cond_;
    bool running_ = false;
};