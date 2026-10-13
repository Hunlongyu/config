#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <utility>

namespace config
{
struct Change
{
    std::uint64_t revision;
    nlohmann::json before;
    nlohmann::json after;
};
namespace detail
{
struct Slot
{
    std::atomic<bool> active{true};
    std::function<void(const Change &)> callback;
};
} // namespace detail
class Subscription
{
    std::shared_ptr<detail::Slot> slot_;

  public:
    Subscription() = default;
    explicit Subscription(std::shared_ptr<detail::Slot> slot) : slot_(std::move(slot))
    {
    }
    Subscription(const Subscription &)            = delete;
    Subscription &operator=(const Subscription &) = delete;
    Subscription(Subscription &&) noexcept        = default;
    Subscription &operator=(Subscription &&other) noexcept
    {
        if (this != &other)
        {
            disconnect();
            slot_ = std::move(other.slot_);
        }
        return *this;
    }
    ~Subscription()
    {
        disconnect();
    }
    void disconnect() noexcept
    {
        if (slot_)
            slot_->active = false;
    }
    bool connected() const noexcept
    {
        return slot_ && slot_->active.load();
    }
};
} // namespace config
