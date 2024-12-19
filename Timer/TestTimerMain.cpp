/*
 * @Author: lenomirei lenomirei@163.com
 * @Date: 2024-12-17 11:52:30
 * @FilePath: \Timer\TestTimerMain.cpp
 * @Description:
 *
 */
#include <iostream>
#include <thread>
#include "Timer.h"

void OnTimeOut() {
  std::cout << "Test repeat timer" << std::endl;
}

int main() {
  Timer timer3;
  timer3.Start(4000, false, []() {
    std::cout << "this is timer3 time out" << std::endl;
  });

  Timer timer;
  timer.Start(2000, true, std::bind(&OnTimeOut));

  Timer timer2;
  timer2.Start(5000, false, []() {
    std::cout << "this is timer2 time out" << std::endl;
  });

  int64_t hahaha = timer.RemainingTime();
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));

  return 0;
}