#include "miniSet.h"
#include <algorithm>

MiniSetCore::~MiniSetCore()
{
    delete m_rest;
}

MiniSetCore::MiniSetCore(const MiniSetCore& other)
    : m_first(other.m_first)
{
    if (other.m_rest)
        m_rest = new std::vector<void*>(*other.m_rest);
}

MiniSetCore::MiniSetCore(MiniSetCore&& other) noexcept
    : m_first(other.m_first), m_rest(other.m_rest)
{
    other.m_first = nullptr;
    other.m_rest = nullptr;
}

MiniSetCore& MiniSetCore::operator=(const MiniSetCore& other)
{
    if (this != &other)
    {
        delete m_rest;
        m_first = other.m_first;
        m_rest = other.m_rest ? new std::vector<void*>(*other.m_rest) : nullptr;
    }
    return *this;
}

MiniSetCore& MiniSetCore::operator=(MiniSetCore&& other) noexcept
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

bool MiniSetCore::empty() const
{
    return m_first == nullptr;
}

std::size_t MiniSetCore::size() const
{
    if (m_first == nullptr) return 0;
    return 1 + (m_rest ? m_rest->size() : 0);
}

bool MiniSetCore::contains(void* ptr) const
{
    if (ptr == nullptr || m_first == nullptr) return false;
    if (m_first == ptr) return true;

    if (m_rest)
    {
        return std::find(m_rest->begin(), m_rest->end(), ptr) != m_rest->end();
    }
    return false;
}

bool MiniSetCore::insert(void* ptr)
{
    if (ptr == nullptr) return false;

    // N = 0
    if (m_first == nullptr)
    {
        m_first = ptr;
        return true;
    }

    if (m_first == ptr) return false;

    // N > 1
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

bool MiniSetCore::erase(void* ptr)
{
    if (ptr == nullptr || m_first == nullptr) return false;

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

void* MiniSetCore::get(std::size_t index) const
{
    if (index == 0) return m_first;
    if (m_rest && (index - 1) < m_rest->size())
    {
        return (*m_rest)[index - 1];
    }
    return nullptr;
}