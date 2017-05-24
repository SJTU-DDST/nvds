#include <bits/stdc++.h>
using namespace std;

bool is_prime(int x) {
  for (int i = 2; i * i < x; ++i) {
    if (x % i == 0)
      return false;
  }
  return true;
}

int main() {
  for (int i = 999900; i < 1000100; ++i) {
    if (is_prime(i)) {
      cout << i << endl;
    }
  }
  return 0;
}
