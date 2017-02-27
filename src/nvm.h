#ifndef _NVDS_NVM_H_
#define _NVDS_NVM_H_

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
  NVMPtr(T* ptr) : ptr_(ptr) {}
  ~NVMPtr() {}
  T* ptr() { return ptr_; }
  const T* ptr() const { return ptr_; }
  T* operator->() { return ptr_; }
  const T* operator->() const { return ptr_; }
  T& operator*() { return *ptr_; }
  const T& operator*() const { return *ptr_; }

 private:
  T* ptr_;
};

} // namespace nvds

#endif // _NVDS_NVM_H_
