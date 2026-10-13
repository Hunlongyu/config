#pragma once

#include <chrono>
#include <condition_variable>
#include <config/store.hpp>
#include <optional>
#include <thread>

namespace config
{
class Watcher
{
    struct Control
    {
        std::mutex mutex;
        std::condition_variable wake;
        bool stop = false;
        std::optional<Error> error;
        std::function<void(const Error &)> on_error;
    };
    std::shared_ptr<Control> control_ = std::make_shared<Control>();
    std::mutex lifecycle_;
    std::thread thread_;

  public:
    explicit Watcher(Store &store, std::chrono::milliseconds interval = std::chrono::seconds(1),
                     std::function<void(const Error &)> on_error = {})
    {
        if (interval <= std::chrono::milliseconds::zero())
            throw Error(ErrorCode::type, "Watch interval must be positive");
        control_->on_error = std::move(on_error);
        thread_ = std::thread([weak = std::weak_ptr<detail::State>(store.state_), control = control_, interval] {
            std::unique_lock lock(control->mutex);
            while (!control->wake.wait_for(lock, interval, [&] { return control->stop; }))
            {
                lock.unlock();
                auto state = weak.lock();
                if (!state)
                    return;
                std::optional<Error> failure;
                try
                {
                    state->reload(true);
                }
                catch (const Error &e)
                {
                    failure = e;
                }
                catch (const std::exception &e)
                {
                    failure = Error(ErrorCode::read, e.what(), state->file);
                }
                catch (...)
                {
                    failure = Error(ErrorCode::read, "Unknown watcher failure", state->file);
                }
                state.reset();
                lock.lock();
                control->error     = failure;
                const bool stopped = control->stop;
                lock.unlock();
                if (!stopped && failure && control->on_error)
                    try
                    {
                        control->on_error(*failure);
                    }
                    catch (...)
                    {
                    }
                lock.lock();
            }
        });
    }
    Watcher(const Watcher &)            = delete;
    Watcher &operator=(const Watcher &) = delete;
    ~Watcher()
    {
        stop();
    }
    void stop()
    {
        {
            std::lock_guard lock(control_->mutex);
            control_->stop = true;
        }
        control_->wake.notify_all();
        std::thread worker;
        {
            std::lock_guard lock(lifecycle_);
            if (thread_.joinable())
                worker = std::move(thread_);
        }
        if (worker.joinable())
        {
            if (worker.get_id() == std::this_thread::get_id())
                worker.detach();
            else
                worker.join();
        }
    }
    [[nodiscard]] std::optional<Error> last_error() const
    {
        std::lock_guard lock(control_->mutex);
        return control_->error;
    }
};
} // namespace config
