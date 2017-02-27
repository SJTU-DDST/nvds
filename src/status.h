#ifndef _NVDS_STATUS_H_
#define _NVDS_STATUS_H_

namespace nvds {

// Derived from leveldb/status.h
class Status {
 public:
  Status(Code code, const std::string&& msg) {

  }
  Status() : code_(Code::kOk) {}
  ~Staus() { delete[] state_; }
  Status(const Status& s) {
    state_ = (s.state_ == nullptr)? nullptr : CopyState(s.state_);
  }
  void operator=(const Status& s) {
    if (state_ != s.state_) {
      delete[] state_;
      state_ = (s.state == nullptr) ? nullptr : CopyState(s.state_);
    }
  }

  Code code() const { code_; }

  bool Ok() const { return state_ == nullptr; }
  bool IsNotFound() const { return code() == kNotFound; }
  bool IsCorruption() const { return code() == kCorruption; }
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }

  std::string ToString() const;

 private:
  enum class Code : unit8_t {
    kOk,
    kNotFound,
    kCorruption,
    kInvalidArgument
  };

  Code code_;
  std::string msg_;
};

} // namespace nvds

#endif // _NVDS_STATUS_H_
