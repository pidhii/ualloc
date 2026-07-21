#pragma once

#include <cassert>
#include <list>
#include <memory>


namespace pidhii {

static constexpr size_t default_block_size = 2 << 15;

template <typename T, size_t BlockSize = default_block_size,
          template <typename> typename Alloc = std::allocator>
class uniform_allocator {
  union storage_type { storage_type *next; T value; };
  static constexpr size_t storage_size = sizeof(storage_type);
  static constexpr size_t block_length = BlockSize / storage_size;
  struct block_type { storage_type data[block_length]; };
  using block_allocator_type = Alloc<block_type>;
  using block_allocator_traits = std::allocator_traits<block_allocator_type>;

  public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  uniform_allocator(block_allocator_type alloc = block_allocator_type { })
  : m_free_head {nullptr},
    m_block_alloc {alloc}
  {
    m_new_block =
        block_allocator_traits::allocate(m_block_alloc, 1);
    m_blocks.push_back(m_new_block);
    m_new_block_offs = 0;
  }

  ~uniform_allocator()
  {
    for (block_type *bp : m_blocks)
      block_allocator_traits::deallocate(m_block_alloc, bp, 1);
  }

  uniform_allocator(const uniform_allocator &) = delete;
  uniform_allocator &operator = (const uniform_allocator &) = delete;

  [[nodiscard]] T *
  allocate(size_t n) noexcept
  {
    assert(n == 1);

    if (m_free_head)
    {
      T *p = reinterpret_cast<T*>(m_free_head);
      m_free_head = m_free_head->next;
      return p;
    }

    if (m_new_block_offs < block_length)
      return reinterpret_cast<T*>(m_new_block->data + m_new_block_offs++);

    assert(m_new_block_offs == block_length);
    m_new_block =
        block_allocator_traits::allocate(m_block_alloc, sizeof(block_type));
    m_blocks.push_back(m_new_block);
    m_new_block_offs = 0;
    return allocate(n);
  }

  void
  deallocate(T *p, size_t n) noexcept
  {
    assert(n == 1);

    p->~T();

    storage_type *cell = reinterpret_cast<storage_type*>(p);
    cell->next = m_free_head;
    m_free_head = cell;
  }

  private:
  storage_type *m_free_head;
  block_type *m_new_block;
  size_t m_new_block_offs;
  std::list<block_type*> m_blocks;
  block_allocator_type m_block_alloc;
}; // class pidhii::uniform_allocator


template <typename T, size_t BlockSize = default_block_size,
          template <typename> typename Alloc = std::allocator>
class static_uniform_allocator {
  public:
  using internal_allocator_type = uniform_allocator<T, BlockSize, Alloc>;
  using value_type = internal_allocator_type::value_type;
  using size_type = internal_allocator_type::size_type;
  using difference_type = internal_allocator_type::difference_type;

  template <typename U, size_t S = BlockSize,
            template <typename> typename A = Alloc>
  struct rebind {
    using other = static_uniform_allocator<U, S, A>;
  };

  static_uniform_allocator() = default;

  template <typename U, size_t S, template <typename> typename A>
  static_uniform_allocator(const static_uniform_allocator<U, S, A> &) { }

  internal_allocator_type&
  internal() noexcept
  {
    static internal_allocator_type alloc;
    return alloc;
  }

  T *
  allocate(size_t n) noexcept
  { return internal().allocate(n); }

  void
  deallocate(T *p, size_t n) noexcept
  { return internal().deallocate(p, n); }
}; // class pidhii::static_uniform_allocator

} // namespace pidhii