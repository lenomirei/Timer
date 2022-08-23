#include <iostream>
#include <thread>
#include "Timer.h"


void OnTimeOuut()
{

}


int main()
{
    Timer timer;
    timer.SetSingleShot(false);
    timer.SetInterval(4000);
    timer.SetTimeoutCallback(std::bind(&OnTimeOuut));
    //timer.SetTimeoutCallback(nullptr);
    timer.Start();

    //std::this_thread::sleep_for(std::chrono::milliseconds(6000));

    int hahaha = timer.RemainingTime();
    //std::this_thread::sleep_for(std::chrono::milliseconds(6000));

    return 0;
}