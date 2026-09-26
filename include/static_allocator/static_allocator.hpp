#pragma once

#include <memory>
#include <mutex>
#include <algorithm>
#include <utility>
#include <type_traits>
#include <cstddef>

namespace static_allocator {
    namespace detail {
        static consteval std::size_t block_size(std::size_t size, std::size_t alignment) {
            const auto quot = size / alignment;
            const auto rem = size % alignment;

            return rem == 0 ? size : (quot + 1) * alignment;
        }

        static constexpr std::size_t div_round_up(std::size_t x, std::size_t y) {
            const auto quot = x / y;
            const auto rem = x % y;

            return rem == 0 ? quot : quot + 1;
        }

        template<bool, typename = void>
        struct MutexStorage {};

        template<bool ThreadSafe>
        struct MutexStorage<ThreadSafe, std::enable_if_t<!ThreadSafe>> {};

        template<bool ThreadSafe>
        struct MutexStorage<ThreadSafe, std::enable_if_t<ThreadSafe>> {
            std::mutex m_mutex;
        };
    }

    // Memory storage for the allocator
    // Different allocators can thus share the same storage
    template<std::size_t StorageSize, std::size_t BlockSize, std::size_t BlockAlignment, bool ThreadSafe = true, bool Throw = true>
    struct StaticAllocatorStorage : detail::MutexStorage<ThreadSafe> {
        static constexpr auto STORAGE_SIZE = StorageSize;
        static constexpr auto BLOCK_SIZE = detail::block_size(BlockSize, BlockAlignment);
        static constexpr auto BLOCK_ALIGNMENT = BlockAlignment;
        static constexpr bool THREAD_SAFE = ThreadSafe;
        static constexpr bool THROW = Throw;

        static_assert(
            BLOCK_ALIGNMENT == 1 ||
            BLOCK_ALIGNMENT == 2 ||
            BLOCK_ALIGNMENT == 4 ||
            BLOCK_ALIGNMENT == 8 ||
            BLOCK_ALIGNMENT == 16,
            "Invalid block alignment"
        );

        alignas(BLOCK_ALIGNMENT) unsigned char m_base[STORAGE_SIZE * BLOCK_SIZE] {};
        bool m_blocks[STORAGE_SIZE] {};
        std::size_t m_pointer {};

        static StaticAllocatorStorage& get() {
            static constinit StaticAllocatorStorage instance;
            return instance;
        }
    };

    // Allocator interface similar to the standard one
    template<typename T, typename Storage>
    class StaticAllocator {
    public:
        using value_type = T;
        using size_type = std::size_t;

        static_assert(sizeof(T) <= Storage::BLOCK_SIZE, "Type doesn't fit into the block size; increase the block size");
        static_assert(alignof(T) <= Storage::BLOCK_ALIGNMENT, "Type has stricter alignment requirements than the block; increase the block alignment");

        value_type* allocate(size_type n) {
            auto& storage = Storage::get();

            if constexpr (Storage::THREAD_SAFE) {
                std::lock_guard guard {storage.m_mutex};
                return allocate_unsafe(storage, n);
            } else {
                return allocate_unsafe(storage, n);
            }
        }

        void deallocate(value_type* p, size_type n) {
            auto& storage = Storage::get();

            if constexpr (Storage::THREAD_SAFE) {
                std::lock_guard guard {storage.m_mutex};
                deallocate_unsafe(storage, p, n);
            } else {
                deallocate_unsafe(storage, p, n);
            }
        }
    private:
        static value_type* allocate_unsafe(Storage& storage, size_type n) {
            if (n == 0 || n > Storage::STORAGE_SIZE) {
                if constexpr (Storage::THROW) {
                    throw std::bad_alloc();
                }

                std::unreachable();
            }

            for (size_type i = storage.m_pointer; i < Storage::STORAGE_SIZE - n + 1; i++) {
                if (try_allocate(storage, i, n)) {
                    return reinterpret_cast<value_type*>(storage.m_base + Storage::BLOCK_SIZE * i);
                }
            }

            for (size_type i {}; i < storage.m_pointer - n + 1; i++) {
                if (try_allocate(storage, i, n)) {
                    return reinterpret_cast<value_type*>(storage.m_base + Storage::BLOCK_SIZE * i);
                }
            }

            if constexpr (Storage::THROW) {
                throw std::bad_alloc();
            }

            std::unreachable();
        }

        static void deallocate_unsafe(Storage& storage, value_type* p, size_type n) {
            const auto block_pointer = reinterpret_cast<size_type>(p) - reinterpret_cast<size_type>(storage.m_base);
            const auto index = block_pointer / Storage::BLOCK_SIZE;

            std::for_each(storage.m_blocks + index, storage.m_blocks + index + n, [](bool& block) { block = false; });
        }

        static bool try_allocate(Storage& storage, size_type i, size_type n) {
            if (std::all_of(storage.m_blocks + i, storage.m_blocks + i + n, [](const bool& block) { return !block; })) {
                std::for_each(storage.m_blocks + i, storage.m_blocks + i + n, [](bool& block) { block = true; });
                storage.m_pointer = i + 1;

                return true;
            }

            return false;
        }
    };

    template<typename T, typename U, typename Storage>
    bool operator==(const StaticAllocator<T, Storage>&, const StaticAllocator<U, Storage>&) { return true; }

    template<typename T, typename U, typename Storage>
    bool operator!=(const StaticAllocator<T, Storage>&, const StaticAllocator<U, Storage>&) { return false; }

    // A different interface than the allocator, a helper class used to override operator new and operator delete for a specific type
    template<typename T, typename Storage>
    struct StaticAllocated {
        void* operator new(std::size_t size) {
            check_requirements();

            StaticAllocator<unsigned char, Storage> alloc;
            using Alloc = std::allocator_traits<decltype(alloc)>;

            const auto blocks = detail::div_round_up(size, Storage::BLOCK_SIZE);
            return Alloc::allocate(alloc, blocks);
        }

        void operator delete(void* ptr, std::size_t size) noexcept {
            check_requirements();

            StaticAllocator<unsigned char, Storage> alloc;
            using Alloc = std::allocator_traits<decltype(alloc)>;

            const auto blocks = detail::div_round_up(size, Storage::BLOCK_SIZE);
            Alloc::deallocate(alloc, static_cast<unsigned char*>(ptr), blocks);
        }

        void* operator new[](std::size_t size) {
            check_requirements();

            StaticAllocator<unsigned char, Storage> alloc;
            using Alloc = std::allocator_traits<decltype(alloc)>;

            const auto blocks = detail::div_round_up(size, Storage::BLOCK_SIZE);
            return Alloc::allocate(alloc, blocks);
        }

        void operator delete[](void* ptr, std::size_t size) noexcept {
            check_requirements();

            StaticAllocator<unsigned char, Storage> alloc;
            using Alloc = std::allocator_traits<decltype(alloc)>;

            const auto blocks = detail::div_round_up(size, Storage::BLOCK_SIZE);
            Alloc::deallocate(alloc, static_cast<unsigned char*>(ptr), blocks);
        }
    private:
        static consteval void check_requirements() {
            static_assert(sizeof(T) <= Storage::BLOCK_SIZE, "Type doesn't fit into the block size; increase the block size");
            static_assert(alignof(T) <= Storage::BLOCK_ALIGNMENT, "Type has stricter alignment requirements than the block; increase the block alignment");
            static_assert(Storage::BLOCK_ALIGNMENT >= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "Block alignment doesn't respect operator new alignment requirements");
        }
    };
}
