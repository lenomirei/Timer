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
    timer.SetInterval(2000);
    //timer.SetTimeoutCallback(std::bind(&OnTimeOuut));
    timer.SetTimeoutCallback([]() {
        std::cout << "hello" << std::endl;
        });
    timer.Start();


    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    return 0;
}