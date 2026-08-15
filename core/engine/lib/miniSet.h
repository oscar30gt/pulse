#ifndef MINI_SET_H
#define MINI_SET_H

#include <cstddef>
#include <vector>
#include <type_traits>
#include <algorithm>

/// Non-template base class containing core memory management logic.
/// 
/// Uses a hybrid storage strategy:
/// - First element stored inline (m_first) for zero heap allocations when N <= 1
/// - Additional elements stored in a vector (m_rest) only when N > 1
/// Use this class to store data sets that typically contain 0 or 1 elements, 
/// with occasional growth to larger sizes.
class MiniSetCore
{
protected:
    void* m_first = nullptr;              /// Inline static element (0 heap allocations for N <= 1).
    std::vector<void*>* m_rest = nullptr; /// Heap allocation used only when N > 1.

public:
    /// Default constructor.
    MiniSetCore() = default;

    /// Destructor. Cleans up the dynamically allocated vector if present.
    ~MiniSetCore()
    {
        delete m_rest;
    }

    /// Copy constructor. Creates a deep copy of the other set.
    /// @param other The MiniSetCore to copy from.
    MiniSetCore(const MiniSetCore& other)
        : m_first(other.m_first)
    {
        if (other.m_rest)
            m_rest = new std::vector<void*>(*other.m_rest);
    }

    /// Move constructor. Transfers ownership of resources from other to this.
    /// @param other The MiniSetCore to move from (left in a valid empty state).
    MiniSetCore(MiniSetCore&& other) noexcept
        : m_first(other.m_first), m_rest(other.m_rest)
    {
        other.m_first = nullptr;
        other.m_rest = nullptr;
    }

    /// Copy assignment operator. Replaces this set with a deep copy of other.
    /// @param other The MiniSetCore to copy from.
    /// @return Reference to this object.
    MiniSetCore& operator=(const MiniSetCore& other)
    {
        if (this != &other)
        {
            delete m_rest;
            m_first = other.m_first;
            m_rest = other.m_rest ? new std::vector<void*>(*other.m_rest) : nullptr;
        }
        return *this;
    }

    /// Move assignment operator. Transfers ownership of resources from other to this.
    /// @param other The MiniSetCore to move from (left in a valid empty state).
    /// @return Reference to this object.
    MiniSetCore& operator=(MiniSetCore&& other) noexcept
    {
        if (this != &other)
        {
            delete m_rest;
            m_first = other.m_first;
            m_rest = other.m_rest;
            other.m_first = nullptr;
            other.m_rest = nullptr;
        }
        return *this;
    }

    /// Checks if the container is empty.
    /// @return True if the set contains no elements, false otherwise.
    [[nodiscard]] bool empty() const
    {
        return m_first == nullptr;
    }

    /// Gets the total number of elements in the set.
    /// @return The number of elements currently stored.
    [[nodiscard]] std::size_t size() const
    {
        if (m_first == nullptr) return 0;
        return 1 + (m_rest ? m_rest->size() : 0);
    }

    /// Checks if a pointer exists in the set.
    /// @param ptr The pointer to search for.
    /// @return True if the pointer is found, false otherwise.
    [[nodiscard]] bool contains(void* ptr) const
    {
        if (ptr == nullptr || m_first == nullptr) return false;
        if (m_first == ptr) return true;

        if (m_rest)
        {
            return std::find(m_rest->begin(), m_rest->end(), ptr) != m_rest->end();
        }
        return false;
    }

    /// Inserts a unique pointer into the set.
    /// Duplicates and nullptr are rejected. The first element is stored inline,
    /// subsequent elements use a dynamically allocated vector.
    /// @param ptr The pointer to insert.
    /// @return True if inserted successfully, false if nullptr or duplicate.
    bool insert(void* ptr)
    {
        if (ptr == nullptr) return false;

        // Handle empty set: store first element inline
        if (m_first == nullptr)
        {
            m_first = ptr;
            return true;
        }

        if (m_first == ptr) return false;

        // Handle N > 1: allocate vector if needed and add to it
        if (!m_rest)
        {
            m_rest = new std::vector<void*>();
        }
        else
        {
            if (std::find(m_rest->begin(), m_rest->end(), ptr) != m_rest->end())
                return false;
        }

        m_rest->push_back(ptr);
        return true;
    }

    /// Removes a pointer from the set and compacts memory if necessary.
    /// Uses swap-with-back technique to avoid costly element shifts.
    /// @param ptr The pointer to remove.
    /// @return True if the element was found and removed, false otherwise.
    bool erase(void* ptr)
    {
        if (ptr == nullptr || m_first == nullptr) return false;

        // Remove first element
        if (m_first == ptr)
        {
            if (m_rest && !m_rest->empty())
            {
                m_first = m_rest->back();
                m_rest->pop_back();

                if (m_rest->empty())
                {
                    delete m_rest;
                    m_rest = nullptr;
                }
            }
            else
            {
                m_first = nullptr;
            }
            return true;
        }

        // Remove from rest vector
        if (m_rest)
        {
            auto it = std::find(m_rest->begin(), m_rest->end(), ptr);
            if (it != m_rest->end())
            {
                *it = m_rest->back();
                m_rest->pop_back();

                if (m_rest->empty())
                {
                    delete m_rest;
                    m_rest = nullptr;
                }
                return true;
            }
        }

        return false;
    }

    /// Internal direct accessor for iterator indexing.
    /// Index 0 returns the first element, indices 1+ return elements from the vector.
    /// @param index The position to access.
    /// @return The pointer at the given index, or nullptr if out of bounds.
    [[nodiscard]] void* get(std::size_t index) const
    {
        if (index == 0) return m_first;
        if (m_rest && (index - 1) < m_rest->size())
        {
            return (*m_rest)[index - 1];
        }
        return nullptr;
    }
};


/// Templated pointer-based set container with inline storage for a single element.
/// 
/// This class is optimized for scenarios where the number of pointers is typically small (N <= 1).
/// The first element is stored directly on the stack, avoiding heap allocations for single-element sets.
/// Additional elements trigger allocation of a dynamic vector.
/// 
/// Only pointer types (T*) are supported via static_assert.
template <typename T>
class MiniSet : private MiniSetCore
{
    static_assert(std::is_pointer_v<T>, "MiniSet requires a pointer type (T*).");

public:
    /// Default constructor. Creates an empty set.
    MiniSet() = default;

    using MiniSetCore::empty;
    using MiniSetCore::size;

    /// Checks if an element exists in the set.
    /// @param element The pointer to search for.
    /// @return True if the element is found, false otherwise.
    [[nodiscard]] bool contains(T element) const
    {
        return MiniSetCore::contains(reinterpret_cast<void*>(element));
    }

    /// Inserts a unique pointer into the set.
    /// Duplicates and nullptr values are rejected.
    /// @param element The pointer to insert.
    /// @return True if inserted successfully, false if nullptr or duplicate.
    bool insert(T element)
    {
        return MiniSetCore::insert(reinterpret_cast<void*>(element));
    }

    /// Removes a pointer from the set and compacts memory if necessary.
    /// @param element The pointer to remove.
    /// @return True if the element was found and removed, false otherwise.
    bool erase(T element)
    {
        return MiniSetCore::erase(reinterpret_cast<void*>(element));
    }

    /// Iterator supporting range-based for loops.
    /// Example: for (T elem : set) { /* use elem */ }
    struct Iterator
    {
        const MiniSet* set = nullptr;   /// Pointer to the parent set.
        std::size_t index = 0;          /// Current position in the set.

        /// Dereference operator. Returns the element at the current position.
        T operator*() const
        {
            return reinterpret_cast<T>(set->get(index));
        }

        /// Pre-increment operator. Advances to the next element.
        Iterator& operator++()
        {
            ++index;
            return *this;
        }

        /// Inequality comparison operator. Used by range-based for loops.
        bool operator!=(const Iterator& other) const
        {
            return index != other.index || set != other.set;
        }
    };

    /// Returns an iterator to the beginning of the set.
    /// @return An iterator pointing to the first element, or end() if empty.
    Iterator begin() const
    {
        if (empty()) return end();
        return Iterator{ this, 0 };
    }

    /// Returns an iterator to the end of the set (past the last element).
    /// @return An iterator representing the end position.
    Iterator end() const
    {
        return Iterator{ this, size() };
    }
};

#endif // MINI_SET_H