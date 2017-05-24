#ifndef _NVDS_MEASUREMENT_H_
#define _NVDS_MEASUREMENT_H_

#include <chrono>
#include <iostream>
#include <ratio>

namespace nvds {

class Measurement {
 public:
  void begin() {
    using namespace std::chrono;
    begin_clock_ = high_resolution_clock::now();
  }
  void end() {
    using namespace std::chrono;
    end_clock_ = high_resolution_clock::now();
    ++num_repetitions_;
    total_time_ += duration_cast<duration<double, std::micro>>(end_clock_ - begin_clock_).count();
  }

  double total_time() const { return total_time_; }
  size_t num_repetitions() const { return num_repetitions_; }
  double average_time() const { return total_time_ / num_repetitions_; }
  double cur_period() const {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    return duration_cast<duration<double, std::micro>>(now - begin_clock_).count();
  }
  // DEBUG
  void Print() const {
    std::cout << "total time: " << total_time() << std::endl;
    std::cout << "repetitions: " << num_repetitions() << std::endl;
    std::cout << "average time: " << average_time() << std::endl;
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> begin_clock_;
  std::chrono::time_point<std::chrono::high_resolution_clock> end_clock_;
  double total_time_ {0};
  size_t num_repetitions_ {0};
};

} // namespace nvds

#endif // _NVDS_MESSAGE_
