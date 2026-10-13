#pragma once

#include <config/detail/state.hpp>

namespace config
{
class Watcher;
class Store
{
    std::shared_ptr<detail::State> state_;
    friend class Watcher;

    template <class T> T convert(const nlohmann::json &value) const
    {
        try
        {
            return value.template get<T>();
        }
        catch (const std::exception &e)
        {
            throw Error(ErrorCode::type, e.what(), path());
        }
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

    const std::filesystem::path &path() const noexcept
    {
        return state_->file;
    }
    std::uint64_t revision() const
    {
        return state_->snapshot()->revision;
    }
    bool dirty() const
    {
        return state_->snapshot()->dirty;
    }
    json snapshot(View view = View::effective) const
    {
        auto state = state_->snapshot();
        return view == View::effective ? state->effective : state->writable.values;
    }
    template <class T> T get(std::string_view key) const
    {
        const auto p     = detail::pointer(key);
        const auto state = state_->snapshot();
        if (!state->effective.contains(p))
            throw Error(ErrorCode::missing, "Configuration key is missing: " + std::string(key), path());
        return convert<T>(state->effective.at(p));
    }
    template <class T> T get_or(std::string_view key, T fallback) const
    {
        const auto p     = detail::pointer(key);
        const auto state = state_->snapshot();
        if (!state->effective.contains(p))
            return fallback;
        return convert<T>(state->effective.at(p));
    }
    bool contains(std::string_view key) const
    {
        return state_->snapshot()->effective.contains(detail::pointer(key));
    }

    void set(std::string_view key, json value, Encoding encoding = Encoding::none)
    {
        const auto p = detail::pointer(key);
        if (encoding < Encoding::none || encoding > Encoding::combined ||
            (encoding != Encoding::none && !value.is_string()))
            throw Error(ErrorCode::type, "Encoding requires a string and a supported encoding", path());
        const auto state = state_;
        state->mutate([&](detail::Document &document) {
            document.values[p] = std::move(value);
            std::erase_if(document.encodings,
                          [&](const auto &rule) { return detail::subtree(rule.first, p.to_string()); });
            if (encoding != Encoding::none)
                document.encodings[p.to_string()] = encoding;
        });
    }
    void erase(std::string_view key)
    {
        const auto p     = detail::pointer(key);
        const auto state = state_;
        state->mutate([&](detail::Document &document) { detail::erase_value(document.values, document.encodings, p); });
    }
    // Edit a writable-layer copy once; concurrent conflicts throw without retrying fn.
    void update(const std::function<void(json &)> &fn)
    {
        const auto state = state_;
        state->mutate([&](detail::Document &document) {
            const auto before = document.values;
            fn(document.values);
            detail::require_object(document.values);
            detail::prune_encodings(document.encodings, before, document.values);
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
