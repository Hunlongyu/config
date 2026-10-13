#pragma once

#include <concepts>
#include <config/detail/state.hpp>
#include <config/detail/value.hpp>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace config
{
class Watcher;
class Store
{
    std::shared_ptr<detail::State> state_;
    friend class Watcher;

    template <detail::ReadableValue T> T convert(const nlohmann::json &value, std::string_view key) const
    {
        try
        {
            return detail::value_as<T>(value);
        }
        catch (const std::exception &e)
        {
            throw Error(ErrorCode::type, e.what(), path(), std::string(key));
        }
    }

    template <class T, class Value> T fallback_value(Value &&value, std::string_view key) const
    {
        if constexpr (std::is_arithmetic_v<T> && !std::same_as<T, bool>)
        {
            try
            {
                return detail::checked_number<T>(value);
            }
            catch (const std::range_error &e)
            {
                throw Error(ErrorCode::type, e.what(), path(), std::string(key));
            }
        }
        else
            return std::forward<Value>(value);
    }

    template <class T, class Missing> T read(std::string_view key, Missing &&missing) const
    {
        const auto p     = detail::pointer(key, path());
        const auto state = state_->snapshot();
        if (!state->effective.contains(p))
            return std::invoke(std::forward<Missing>(missing));
        return convert<T>(state->effective.at(p), key);
    }

  public:
    using json = nlohmann::json;
    explicit Store(std::filesystem::path file, StoreOptions options = {})
        : state_(std::make_shared<detail::State>(std::move(file), std::move(options)))
    {
    }
    Store(const Store &)            = delete;
    Store &operator=(const Store &) = delete;
    Store(Store &&)                 = delete;
    Store &operator=(Store &&)      = delete;
    ~Store()
    {
        state_->observers->close();
    }

    [[nodiscard]] const std::filesystem::path &path() const noexcept
    {
        return state_->file;
    }
    [[nodiscard]] std::uint64_t revision() const
    {
        return state_->snapshot()->revision;
    }
    [[nodiscard]] bool dirty() const
    {
        return state_->snapshot()->dirty;
    }
    [[nodiscard]] json snapshot(View view = View::effective) const
    {
        const auto state = state_->snapshot();
        switch (view)
        {
        case View::effective:
            return state->effective;
        case View::writable:
            return state->writable.values;
        }
        throw Error(ErrorCode::type, "Unsupported configuration view", path());
    }
    template <detail::ReadableValue T> [[nodiscard]] T get(std::string_view key) const
    {
        return read<T>(key, [&]() -> T {
            throw Error(ErrorCode::missing, "Configuration key is missing: " + std::string(key), path(),
                        std::string(key));
        });
    }
    template <detail::ReadableValue T, class Value>
        requires(!std::invocable<Value> && detail::FallbackValue<T, Value>)
    [[nodiscard]] T get(std::string_view key, Value &&fallback) const
    {
        return read<T>(key, [&]() -> T { return fallback_value<T>(std::forward<Value>(fallback), key); });
    }
    template <detail::ReadableValue T, class Factory>
        requires(std::invocable<Factory> && detail::FallbackValue<T, std::invoke_result_t<Factory>>)
    [[nodiscard]] T get(std::string_view key, Factory &&factory) const
    {
        return read<T>(key, [&]() -> T { return fallback_value<T>(std::invoke(std::forward<Factory>(factory)), key); });
    }
    template <detail::ReadableValue T> [[nodiscard]] std::optional<T> find(std::string_view key) const
    {
        const auto p     = detail::pointer(key, path());
        const auto state = state_->snapshot();
        if (!state->effective.contains(p))
            return std::nullopt;
        return convert<T>(state->effective.at(p), key);
    }
    [[nodiscard]] bool contains(std::string_view key) const
    {
        const auto p = detail::pointer(key, path());
        return state_->snapshot()->effective.contains(p);
    }

    void set(std::string_view key, json value, Encoding encoding = Encoding::none)
    {
        const auto p = detail::pointer(key, path());
        if (encoding < Encoding::none || encoding > Encoding::combined ||
            (encoding != Encoding::none && !value.is_string()))
            throw Error(ErrorCode::type, "Encoding requires a string and a supported encoding", path(),
                        std::string(key));
        const auto state = state_;
        state->mutate(
            [&](detail::Document &document) {
                document.values[p] = std::move(value);
                std::erase_if(document.encodings,
                              [&](const auto &rule) { return detail::subtree(rule.first, p.to_string()); });
                if (encoding != Encoding::none)
                    document.encodings[p.to_string()] = encoding;
                return true;
            },
            key);
    }
    bool erase(std::string_view key)
    {
        const auto p     = detail::pointer(key, path());
        const auto state = state_;
        return state->mutate(
            [&](detail::Document &document) { return detail::erase_value(document.values, document.encodings, p); },
            key);
    }
    // Edit a writable-layer copy once; concurrent conflicts throw without retrying fn.
    template <class Edit>
        requires std::invocable<Edit, json &>
    void update(Edit &&fn)
    {
        const auto state = state_;
        state->mutate([&](detail::Document &document) {
            const auto before = document.values;
            std::invoke(std::forward<Edit>(fn), document.values);
            detail::require_object(document.values);
            detail::prune_encodings(document.encodings, before, document.values);
            return true;
        });
    }
    void merge(const json &overlay)
    {
        detail::require_object(overlay);
        update([&](json &data) { detail::deep_merge(data, overlay); });
    }
    void merge_file(const std::filesystem::path &file)
    {
        const auto normalized = detail::normalize_file(file);
        auto text             = state_->backend->read(normalized);
        if (!text)
            throw Error(ErrorCode::read, "Merge file is missing", normalized);
        const auto overlay = detail::decode(text, normalized, state_->options.legacy_encoding);
        const auto state   = state_;
        state->mutate([&](detail::Document &document) {
            const auto before = document.values;
            detail::deep_merge(document.values, overlay.values);
            detail::prune_encodings(document.encodings, before, document.values);
            for (const auto &[key, encoding] : overlay.encodings)
                document.encodings[key] = encoding;
            return true;
        });
    }
    void save()
    {
        const auto state = state_;
        state->save();
    }
    void reload()
    {
        const auto state = state_;
        state->reload();
    }
    [[nodiscard]] Subscription subscribe(std::function<void(const Change &)> callback)
    {
        if (!callback)
            throw Error(ErrorCode::type, "Subscription callback is empty");
        return state_->observers->subscribe(std::move(callback));
    }
};
} // namespace config
