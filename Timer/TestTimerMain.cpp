#include <iostream>
#include <thread>
#include "Timer.h"


void OnTimeOuut()
{

}


int main()
{
    Timer timer;
    timer.SetSingleShot(true);
    timer.SetInterval(4000);
    //timer.SetTimeoutCallback(std::bind(&OnTimeOuut));
    timer.SetTimeoutCallback([&timer]() {
        std::cout << "hello" << std::endl;
        });
    timer.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(8000));
    return 0;
}