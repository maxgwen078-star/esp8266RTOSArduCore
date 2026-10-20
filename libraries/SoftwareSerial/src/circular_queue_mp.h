/*
circular_queue_mp.h - Implementation of a lock-free circular queue for EspSoftwareSerial.
Copyright (c) 2019 Dirk O. Kaar. All rights reserved.
Adapté pour ESP8266 RTOS SDK uniquement.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
*/

#ifndef __circular_queue_mp_h
#define __circular_queue_mp_h

#include "circular_queue.h"

#include <mutex>

/*!
    @brief  Instance class for a multi-producer, single-consumer circular queue / ring buffer (FIFO).
*/
template< typename T, typename ForEachArg = void >
class circular_queue_mp : protected circular_queue<T, ForEachArg>
{
public:
    circular_queue_mp() = default;
    circular_queue_mp(const size_t capacity) : circular_queue<T, ForEachArg>(capacity) {}
    circular_queue_mp(circular_queue<T, ForEachArg>&& cq) : circular_queue<T, ForEachArg>(std::move(cq)) {}

    using circular_queue<T, ForEachArg>::operator=;
    using circular_queue<T, ForEachArg>::capacity;
    using circular_queue<T, ForEachArg>::flush;
    using circular_queue<T, ForEachArg>::available;
    using circular_queue<T, ForEachArg>::available_for_push;
    using circular_queue<T, ForEachArg>::peek;
    using circular_queue<T, ForEachArg>::pop;
    using circular_queue<T, ForEachArg>::pop_n;
    using circular_queue<T, ForEachArg>::for_each;
    using circular_queue<T, ForEachArg>::for_each_rev_requeue;

    bool capacity(const size_t cap)
    {
        std::lock_guard<std::mutex> lock(m_pushMtx);
        return circular_queue<T, ForEachArg>::capacity(cap);
    }

    bool IRAM_ATTR push() = delete;

    bool IRAM_ATTR push(T&& val)
    {
        std::lock_guard<std::mutex> lock(m_pushMtx);
        return circular_queue<T, ForEachArg>::push(std::move(val));
    }

    bool IRAM_ATTR push(const T& val)
    {
        std::lock_guard<std::mutex> lock(m_pushMtx);
        return circular_queue<T, ForEachArg>::push(val);
    }

    size_t push_n(const T* buffer, size_t size)
    {
        std::lock_guard<std::mutex> lock(m_pushMtx);
        return circular_queue<T, ForEachArg>::push_n(buffer, size);
    }

    T& pop_requeue();

    bool for_each_requeue(const Delegate<bool(T&), ForEachArg>& fun);

protected:
    std::mutex m_pushMtx;
};

template< typename T, typename ForEachArg >
T& circular_queue_mp<T, ForEachArg>::pop_requeue()
{
    std::lock_guard<std::mutex> lock(m_pushMtx);
    const auto outPos = circular_queue<T, ForEachArg>::m_outPos.load(std::memory_order_acquire);
    const auto inPos = circular_queue<T, ForEachArg>::m_inPos.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (inPos == outPos) return circular_queue<T, ForEachArg>::defaultValue;
    T& val = circular_queue<T, ForEachArg>::m_buffer[inPos] = std::move(circular_queue<T, ForEachArg>::m_buffer[outPos]);
    const auto bufSize = circular_queue<T, ForEachArg>::m_bufSize;
    std::atomic_thread_fence(std::memory_order_release);
    circular_queue<T, ForEachArg>::m_outPos.store((outPos + 1) % bufSize, std::memory_order_relaxed);
    circular_queue<T, ForEachArg>::m_inPos.store((inPos + 1) % bufSize, std::memory_order_release);
    return val;
}

template< typename T, typename ForEachArg >
bool circular_queue_mp<T, ForEachArg>::for_each_requeue(const Delegate<bool(T&), ForEachArg>& fun)
{
    auto inPos0 = circular_queue<T, ForEachArg>::m_inPos.load(std::memory_order_acquire);
    auto outPos = circular_queue<T, ForEachArg>::m_outPos.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (outPos == inPos0) return false;
    do {
        T&& val = std::move(circular_queue<T, ForEachArg>::m_buffer[outPos]);
        if (fun(val))
        {
            std::lock_guard<std::mutex> lock(m_pushMtx);
            std::atomic_thread_fence(std::memory_order_release);
            auto inPos = circular_queue<T, ForEachArg>::m_inPos.load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            circular_queue<T, ForEachArg>::m_buffer[inPos] = std::move(val);
            std::atomic_thread_fence(std::memory_order_release);
            circular_queue<T, ForEachArg>::m_inPos.store((inPos + 1) % circular_queue<T, ForEachArg>::m_bufSize, std::memory_order_release);
        }
        else
        {
            std::atomic_thread_fence(std::memory_order_release);
        }
        outPos = (outPos + 1) % circular_queue<T, ForEachArg>::m_bufSize;
        circular_queue<T, ForEachArg>::m_outPos.store(outPos, std::memory_order_release);
    } while (outPos != inPos0);
    return true;
}

#endif // __circular_queue_mp_h