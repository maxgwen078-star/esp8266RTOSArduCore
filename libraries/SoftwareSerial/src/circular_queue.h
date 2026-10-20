/*
circular_queue.h - Implementation of a lock-free circular queue for EspSoftwareSerial.
Copyright (c) 2019 Dirk O. Kaar. All rights reserved.
Adapté pour ESP8266 RTOS SDK uniquement.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
*/

#ifndef __circular_queue_h
#define __circular_queue_h

#include <Arduino.h>

#include <atomic>
#include <memory>
#include <algorithm>
#include "Delegate.h"
using std::min;

/*!
    @brief  Instance class for a single-producer, single-consumer circular queue / ring buffer (FIFO).
*/
template< typename T, typename ForEachArg = void >
class circular_queue
{
public:
    circular_queue() : m_bufSize(1)
    {
        m_inPos.store(0);
        m_outPos.store(0);
    }
    circular_queue(const size_t capacity) : m_bufSize(capacity + 1), m_buffer(new T[m_bufSize])
    {
        m_inPos.store(0);
        m_outPos.store(0);
    }
    circular_queue(circular_queue&& cq) :
        m_bufSize(cq.m_bufSize), m_buffer(cq.m_buffer), m_inPos(cq.m_inPos.load()), m_outPos(cq.m_outPos.load())
    {}
    ~circular_queue()
    {
        m_buffer.reset();
    }
    circular_queue(const circular_queue&) = delete;
    circular_queue& operator=(circular_queue&& cq)
    {
        m_bufSize = cq.m_bufSize;
        m_buffer = cq.m_buffer;
        m_inPos.store(cq.m_inPos.load());
        m_outPos.store(cq.m_outPos.load());
    }
    circular_queue& operator=(const circular_queue&) = delete;

    size_t capacity() const { return m_bufSize - 1; }

    bool capacity(const size_t cap);

    void flush() { m_outPos.store(m_inPos.load()); }

    size_t IRAM_ATTR available() const
    {
        int avail = static_cast<int>(m_inPos.load() - m_outPos.load());
        if (avail < 0) avail += m_bufSize;
        return avail;
    }

    size_t IRAM_ATTR available_for_push() const
    {
        int avail = static_cast<int>(m_outPos.load() - m_inPos.load()) - 1;
        if (avail < 0) avail += m_bufSize;
        return avail;
    }

    T peek() const
    {
        const auto outPos = m_outPos.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        return m_buffer[outPos];
    }

    T& IRAM_ATTR pushpeek()
    {
        const auto inPos = m_inPos.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        return m_buffer[inPos];
    }

    bool IRAM_ATTR push()
    {
        const auto inPos = m_inPos.load(std::memory_order_acquire);
        const size_t next = (inPos + 1) % m_bufSize;
        if (next == m_outPos.load(std::memory_order_relaxed)) return false;
        std::atomic_thread_fence(std::memory_order_acquire);
        m_inPos.store(next, std::memory_order_release);
        return true;
    }

    bool IRAM_ATTR push(T&& val)
    {
        const auto inPos = m_inPos.load(std::memory_order_acquire);
        const size_t next = (inPos + 1) % m_bufSize;
        if (next == m_outPos.load(std::memory_order_relaxed)) return false;
        std::atomic_thread_fence(std::memory_order_acquire);
        m_buffer[inPos] = std::move(val);
        std::atomic_thread_fence(std::memory_order_release);
        m_inPos.store(next, std::memory_order_release);
        return true;
    }

    bool IRAM_ATTR push(const T& val)
    {
        T v(val);
        return push(std::move(v));
    }

    size_t push_n(const T* buffer, size_t size);

    T pop();

    size_t pop_n(T* buffer, size_t size);

    void for_each(const Delegate<void(T&&), ForEachArg>& fun);

    bool for_each_rev_requeue(const Delegate<bool(T&), ForEachArg>& fun);

protected:
    const T defaultValue = {};
    size_t m_bufSize;
    std::unique_ptr<T[]> m_buffer;
    std::atomic<size_t> m_inPos;
    std::atomic<size_t> m_outPos;
};

template< typename T, typename ForEachArg >
bool circular_queue<T, ForEachArg>::capacity(const size_t cap)
{
    if (cap + 1 == m_bufSize) return true;
    else if (available() > cap) return false;
    std::unique_ptr<T[]> buffer(new T[cap + 1]);
    const auto available = pop_n(buffer, cap);
    m_buffer.reset(buffer);
    m_bufSize = cap + 1;
    std::atomic_thread_fence(std::memory_order_release);
    m_inPos.store(available, std::memory_order_relaxed);
    m_outPos.store(0, std::memory_order_release);
    return true;
}

template< typename T, typename ForEachArg >
size_t circular_queue<T, ForEachArg>::push_n(const T* buffer, size_t size)
{
    const auto inPos = m_inPos.load(std::memory_order_acquire);
    const auto outPos = m_outPos.load(std::memory_order_relaxed);
    size_t blockSize = (outPos > inPos) ? outPos - 1 - inPos : (outPos == 0) ? m_bufSize - 1 - inPos : m_bufSize - inPos;
    blockSize = min(size, blockSize);
    if (!blockSize) return 0;
    int next = (inPos + blockSize) % m_bufSize;
    std::atomic_thread_fence(std::memory_order_acquire);
    auto dest = m_buffer.get() + inPos;
    std::copy_n(std::make_move_iterator(buffer), blockSize, dest);
    size = min(size - blockSize, outPos > 1 ? static_cast<size_t>(outPos - next - 1) : 0);
    next += size;
    dest = m_buffer.get();
    std::copy_n(std::make_move_iterator(buffer + blockSize), size, dest);
    std::atomic_thread_fence(std::memory_order_release);
    m_inPos.store(next, std::memory_order_release);
    return blockSize + size;
}

template< typename T, typename ForEachArg >
T circular_queue<T, ForEachArg>::pop()
{
    const auto outPos = m_outPos.load(std::memory_order_acquire);
    if (m_inPos.load(std::memory_order_relaxed) == outPos) return defaultValue;
    std::atomic_thread_fence(std::memory_order_acquire);
    auto val = std::move(m_buffer[outPos]);
    std::atomic_thread_fence(std::memory_order_release);
    m_outPos.store((outPos + 1) % m_bufSize, std::memory_order_release);
    return val;
}

template< typename T, typename ForEachArg >
size_t circular_queue<T, ForEachArg>::pop_n(T* buffer, size_t size) {
    size_t avail = size = min(size, available());
    if (!avail) return 0;
    const auto outPos = m_outPos.load(std::memory_order_acquire);
    size_t n = min(avail, static_cast<size_t>(m_bufSize - outPos));
    std::atomic_thread_fence(std::memory_order_acquire);
    if (buffer) {
        buffer = std::copy_n(std::make_move_iterator(m_buffer.get() + outPos), n, buffer);
        avail -= n;
        std::copy_n(std::make_move_iterator(m_buffer.get()), avail, buffer);
    }
    std::atomic_thread_fence(std::memory_order_release);
    m_outPos.store((outPos + size) % m_bufSize, std::memory_order_release);
    return size;
}

template< typename T, typename ForEachArg >
void circular_queue<T, ForEachArg>::for_each(const Delegate<void(T&&), ForEachArg>& fun)
{
    auto outPos = m_outPos.load(std::memory_order_acquire);
    const auto inPos = m_inPos.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    while (outPos != inPos)
    {
        fun(std::move(m_buffer[outPos]));
        std::atomic_thread_fence(std::memory_order_release);
        outPos = (outPos + 1) % m_bufSize;
        m_outPos.store(outPos, std::memory_order_release);
    }
}

template< typename T, typename ForEachArg >
bool circular_queue<T, ForEachArg>::for_each_rev_requeue(const Delegate<bool(T&), ForEachArg>& fun)
{
    auto inPos0 = m_inPos.load(std::memory_order_acquire);
    auto outPos = m_outPos.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (outPos == inPos0) return false;
    auto pos = inPos0;
    auto outPos1 = inPos0;
    const auto posDecr = m_bufSize - 1;
    do {
        pos = (pos + posDecr) % m_bufSize;
        T&& val = std::move(m_buffer[pos]);
        if (fun(val))
        {
            outPos1 = (outPos1 + posDecr) % m_bufSize;
            if (outPos1 != pos) m_buffer[outPos1] = std::move(val);
        }
    } while (pos != outPos);
    m_outPos.store(outPos1, std::memory_order_release);
    return true;
}

#endif // __circular_queue_h