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
    timer.SetInterval(2000);
    //timer.SetTimeoutCallback(std::bind(&OnTimeOuut));
    timer.SetTimeoutCallback([]() {
        std::cout << "hello" << std::endl;
        });
    timer.Start();


    std::this_thread::sleep_for(std::chrono::milliseconds(8000));
    return 0;
}