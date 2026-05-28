#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <vector>

class MemoryPool {
 public:
  MemoryPool(std::size_t block_size, std::size_t block_count)
      : block_size_(block_size),
        block_count_(block_count),
        count_free_bl_(block_count) {
    pool_ = new char[block_size_ * block_count_];
    pool_begin_ = pool_;
    pool_end_ = pool_ + block_size_ * block_count_;
    used_blocks_.assign(block_count_, false);
  }

  ~MemoryPool() { delete[] pool_; }

  MemoryPool(const MemoryPool&) = delete;
  MemoryPool& operator=(const MemoryPool&) = delete;

  MemoryPool(MemoryPool&& other) noexcept
      : block_size_(other.block_size_),
        block_count_(other.block_count_),
        count_free_bl_(other.count_free_bl_),
        pool_(other.pool_),
        pool_begin_(other.pool_begin_),
        pool_end_(other.pool_end_),
        used_blocks_(std::move(other.used_blocks_)) {
    other.pool_ = nullptr;
    other.pool_begin_ = nullptr;
    other.pool_end_ = nullptr;
    other.block_count_ = 0;
    other.count_free_bl_ = 0;
  }

  MemoryPool& operator=(MemoryPool&& other) noexcept {
    if (this != &other) {
      delete[] pool_;
      block_size_ = other.block_size_;
      block_count_ = other.block_count_;
      count_free_bl_ = other.count_free_bl_;
      pool_ = other.pool_;
      pool_begin_ = other.pool_begin_;
      pool_end_ = other.pool_end_;
      used_blocks_ = std::move(other.used_blocks_);
      other.pool_ = nullptr;
      other.pool_begin_ = nullptr;
      other.pool_end_ = nullptr;
      other.block_count_ = 0;
      other.count_free_bl_ = 0;
    }
    return *this;
  }

  void* allocate(std::size_t size) {
    std::size_t need = NeedBlocks(size);

    std::size_t count = 0;
    std::size_t first_block = 0;
    bool found = false;
    for (std::size_t i = 0; i < used_blocks_.size(); ++i) {
      if (!used_blocks_[i]) {
        if (count == 0) {
          first_block = i;
        }
        ++count;
        if (count == need) {
          found = true;
          break;
        }
      } else {
        count = 0;
      }
    }

    if (!found) {
      throw std::bad_alloc{};
    }

    for (std::size_t i = first_block; i < first_block + need; ++i) {
      used_blocks_[i] = true;
    }
    count_free_bl_ -= need;
    return pool_begin_ + first_block * block_size_;
  }

  void deallocate(void* block, std::size_t size) {
    char* p = static_cast<char*>(block);
    std::size_t index = static_cast<std::size_t>(p - pool_begin_) / block_size_;
    std::size_t need = NeedBlocks(size);

    for (std::size_t j = index; j < index + need && j < used_blocks_.size();
         ++j) {
      used_blocks_[j] = false;
    }
    count_free_bl_ += need;
  }

  char* begin() const { return pool_begin_; }
  char* end() const { return pool_end_; }

  bool operator==(const MemoryPool& other) const {
    return pool_begin_ == other.pool_begin_ && pool_end_ == other.pool_end_ &&
           block_size_ == other.block_size_ &&
           block_count_ == other.block_count_;
  }
  bool operator!=(const MemoryPool& other) const { return !(*this == other); }

 private:
  std::size_t NeedBlocks(std::size_t size) const {
    std::size_t n = size / block_size_;
    if (size % block_size_ != 0) {
      ++n;
    }
    return n == 0 ? 1 : n;
  }

  std::size_t block_size_;
  std::size_t block_count_;
  std::size_t count_free_bl_;
  char* pool_ = nullptr;
  char* pool_begin_ = nullptr;
  char* pool_end_ = nullptr;
  std::vector<bool> used_blocks_;
};

template <typename T, std::size_t pool_size, std::size_t first_block_size, std::size_t... block_sizes>
class MemoryPoolAllocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  template <typename U>
  struct rebind {
    using other = MemoryPoolAllocator<U, pool_size, first_block_size, block_sizes...>;
  };

  MemoryPoolAllocator() : pools_(std::make_shared<std::vector<MemoryPool>>()) {
    MakePools();
  }

  MemoryPoolAllocator(const MemoryPoolAllocator& other) = default;

  template <typename U>
  MemoryPoolAllocator(
      const MemoryPoolAllocator<U, pool_size, first_block_size, block_sizes...>& other)
      : pools_(other.pools_) {}

  T* allocate(size_type n) {
    for (MemoryPool& pool : *pools_) {
      try {
        return static_cast<T*>(pool.allocate(n * sizeof(T)));
      } catch (const std::bad_alloc&) {
        continue;
      }
    }
    throw std::bad_alloc{};
  }

  void deallocate(T* ptr, size_type n) {
    char* p = reinterpret_cast<char*>(ptr);
    for (MemoryPool& pool : *pools_) {
      if (p >= pool.begin() && p < pool.end()) {
        pool.deallocate(ptr, n * sizeof(T));
        return;
      }
    }
  }

  bool operator==(const MemoryPoolAllocator& other) const {
    return pools_ == other.pools_;
  }
  bool operator!=(const MemoryPoolAllocator& other) const { return !(*this == other); }

 private:
  void MakePools() {
    pools_->reserve(1 + sizeof...(block_sizes));
    pools_->emplace_back(first_block_size, pool_size / first_block_size);
    (pools_->emplace_back(block_sizes, pool_size / block_sizes), ...);
  }

  std::shared_ptr<std::vector<MemoryPool>> pools_;

  template <typename U, std::size_t PS, std::size_t FBS, std::size_t... BS>
  friend class MemoryPoolAllocator;
};