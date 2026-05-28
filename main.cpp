#include <deque>

#include "MemoryPoolAllocator.h"

int main() {
  std::vector<int, MemoryPoolAllocator<int, 33000, 4, 8>> au;
  for (int i = 0; i < 5000; i++) {
    au.push_back(i);
  }

  return 0;
}