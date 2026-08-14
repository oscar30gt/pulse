#ifndef MINI_SET_H
#define MINI_SET_H

#include <cstddef>
#include <vector>
#include <type_traits>

/// Non-template base class containing the core memory management logic.
/// Compiles once to prevent template code bloat.
class MiniSetCore
{
protected:
    void* m_first = nullptr;              /// Inline static element (0 Heap allocations for N <= 1).
    std::vector<void*>* m_rest = nullptr; /// Heap allocation used only when N > 1.

public:
    MiniSetCore() = default;
    ~MiniSetCore();

    MiniSetCore(const MiniSetCore& other);
    MiniSetCore(MiniSetCore&& other) noexcept;
    MiniSetCore& operator=(const MiniSetCore& other);
    MiniSetCore& operator=(MiniSetCore&& other) noexcept;

    /// Checks if the container is empty.
    [[nodiscard]] bool empty() const;

    /// Gets the total number of elements.
    [[nodiscard]] std::size_t size() const;

    /// Checks if a pointer exists in the set.
    [[nodiscard]] bool contains(void* ptr) const;

    /// Inserts a unique pointer into the set.
    /// @returns True if inserted, false if nullptr or duplicate.
    bool insert(void* ptr);

    /// Removes a pointer from the set and compacts memory if necessary.
    /// @returns True if the element was found and removed.
    bool erase(void* ptr);

    /// Internal direct accessor for iterator indexing.
    [[nodiscard]] void* get(std::size_t index) const;
};


/// Pointer-based set container with inline storage for a single element.
/// This class is designed for scenarios where the number of elements is typically small (N <= 1).
/// It avoids heap allocations for the first element, providing efficient memory usage.
template <typename T>
class MiniSet : private MiniSetCore
{
    static_assert(std::is_pointer_v<T>, "MiniSet requires a pointer type (T*).");

public:
    MiniSet() = default;

    using MiniSetCore::empty;
    using MiniSetCore::size;

    /// Checks if an element exists in the set.
    [[nodiscard]] bool contains(T element) const
    {
        return MiniSetCore::contains(reinterpret_cast<void*>(element));
    }

    /// Inserts a unique pointer into the set.
    /// @returns True if inserted, false if nullptr or duplicate.
    bool insert(T element)
    {
        return MiniSetCore::insert(reinterpret_cast<void*>(element));
    }

    /// Removes a pointer from the set and compacts memory if necessary.
    /// @returns True if the element was found and removed.
    bool erase(T element)
    {
        return MiniSetCore::erase(reinterpret_cast<void*>(element));
    }

    /// Iterator supporting range-based for loops: for (T elem : set)
    struct Iterator
    {
        const MiniSet* set = nullptr;
        std::size_t index = 0;

        T operator*() const
        {
            return reinterpret_cast<T>(set->get(index));
        }

        Iterator& operator++()
        {
            ++index;
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return index != other.index || set != other.set;
        }
    };

    /// Returns an iterator to the beginning.
    Iterator begin() const
    {
        if (empty()) return end();
        return Iterator{ this, 0 };
    }

    /// Returns an iterator to the end.
    Iterator end() const
    {
        return Iterator{ this, size() };
    }
};

#endif // MINI_SET_H