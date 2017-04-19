#ifndef _NVDS_NVM_H_
#define _NVDS_NVM_H_

#include <cstdlib>
#include <cstddef>

namespace nvds {

/*
 * A pointer that points to objects allocated on NVM. As are 
 * persistent on NVM, They should be Allocated and freed by
 * the Allocator. This class is for an abstraction of accessing
 * NVM.
 */
template<typename T>
class NVMPtr {
 public:
  explicit NVMPtr(T* ptr) : ptr_(ptr) {}
  ~NVMPtr() {}
  T* ptr() { return ptr_; }
  const T* ptr() const { return ptr_; }
  T* operator->() { return ptr_; }
  const T* operator->() const { return ptr_; }
  T& operator*() { return *ptr_; }
  const T& operator*() const { return *ptr_; }
  bool operator==(const NVMPtr& other) const {
    return ptr_ == other.ptr_;
  }
  bool operator!=(const NVMPtr& other) const {
    return !(*this == other);
  }
  bool operator==(const std::nullptr_t& null) const {
    return ptr_ == null;
  }
  bool operator!=(const std::nullptr_t& null) const {
    return !(*this == nullptr);
  }
  NVMPtr<T> operator+(std::ptrdiff_t d) const {
    return NVMPtr<T>(ptr_ + d);
  }
  NVMPtr<T> operator-(std::ptrdiff_t d) const {
    return NVMPtr<T>(ptr_ - d);
  }
  std::ptrdiff_t operator-(NVMPtr<T> other) const {
    return ptr_ - other.ptr_;
  }
  template<typename RT>
  const NVMPtr<T>& operator=(NVMPtr<RT> rhs) {
    ptr_ = rhs.ptr_;
    return *this;
  }

 private:
  T* ptr_;
};

/*
 * Get nvm with specified size
 */
template <typename T>
NVMPtr<T> AcquireNVM(size_t size) {
  return NVMPtr<T>(static_cast<T*>(malloc(size)));
}

} // namespace nvds

#endif // _NVDS_NVM_H_
