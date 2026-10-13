#pragma once

#include <config/subscription.hpp>
#include <deque>
#include <mutex>
#include <vector>

namespace config::detail
{
class Observers
{
    struct Pending
    {
        Change change;
        std::vector<std::shared_ptr<Slot>> slots;
    };
    std::mutex mutex_;
    std::vector<std::weak_ptr<Slot>> slots_;
    std::deque<Pending> queue_;
    bool dispatching_ = false;
    bool closed_      = false;
    std::function<void(std::exception_ptr)> on_error_;

  public:
    explicit Observers(std::function<void(std::exception_ptr)> on_error) : on_error_(std::move(on_error))
    {
    }
    ~Observers()
    {
        close();
    }
    Subscription subscribe(std::function<void(const Change &)> callback)
    {
        auto slot      = std::make_shared<Slot>();
        slot->callback = std::move(callback);
        std::lock_guard lock(mutex_);
        if (closed_)
            slot->active = false;
        else
        {
            std::erase_if(slots_, [](const auto &p) { return p.expired(); });
            slots_.push_back(slot);
        }
        return Subscription(std::move(slot));
    }
    void enqueue(Change change)
    {
        Pending pending{std::move(change), {}};
        std::lock_guard lock(mutex_);
        if (closed_)
            return;
        for (const auto &weak : slots_)
            if (auto slot = weak.lock())
                pending.slots.push_back(std::move(slot));
        queue_.push_back(std::move(pending));
    }
    void drain()
    {
        std::unique_lock lock(mutex_);
        if (dispatching_)
            return;
        dispatching_ = true;
        while (!queue_.empty())
        {
            auto pending = std::move(queue_.front());
            queue_.pop_front();
            lock.unlock();
            for (const auto &slot : pending.slots)
                if (slot->active)
                {
                    try
                    {
                        slot->callback(pending.change);
                    }
                    catch (...)
                    {
                        if (on_error_)
                            try
                            {
                                on_error_(std::current_exception());
                            }
                            catch (...)
                            {
                            }
                    }
                }
            // 用户回调及其捕获对象的析构均不得在内部锁下运行。
            pending.slots.clear();
            lock.lock();
        }
        dispatching_ = false;
    }
    void close()
    {
        std::deque<Pending> pending;
        std::vector<std::shared_ptr<Slot>> keep_alive;
        std::lock_guard lock(mutex_);
        closed_ = true;
        for (const auto &weak : slots_)
            if (auto slot = weak.lock())
            {
                slot->active = false;
                keep_alive.push_back(std::move(slot));
            }
        pending.swap(queue_);
    }
};
} // namespace config::detail
