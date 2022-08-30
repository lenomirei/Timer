#include <iostream>
#include <thread>
#include "Timer.h"


void OnTimeOuut()
{

}


int main()
{
    Timer timer3;
    timer3.SetSingleShot(true);
    timer3.SetInterval(4000);
    timer3.SetTimeoutCallback([]() {
        std::cout << "this is timer2 time out" << std::endl;
        });
    timer3.Start();

    Timer timer;
    timer.SetSingleShot(false);
    timer.SetInterval(2000);
    timer.SetTimeoutCallback(std::bind(&OnTimeOuut));
    //timer.SetTimeoutCallback(nullptr);
    timer.Start();
    
    timer.SetInterval(3000);

    Timer timer2;
    timer2.SetSingleShot(true);
    timer2.SetInterval(5000);
    timer2.SetTimeoutCallback([]() {
        std::cout << "this is timer2 time out" << std::endl;
    });
    timer2.Start();

    int hahaha = timer.RemainingTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));

    return 0;
}