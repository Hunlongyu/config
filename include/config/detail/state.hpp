#pragma once

#include <config/detail/environment.hpp>
#include <config/detail/file_backend.hpp>
#include <config/detail/json_codec.hpp>
#include <config/detail/observer.hpp>
#include <shared_mutex>

namespace config::detail
{
struct Snapshot
{
    Document writable;
    json layers      = json::object();
    json environment = json::object();
    json effective   = json::object();
    std::vector<std::optional<std::string>> sources;
    std::uint64_t revision = 0;
    bool dirty             = false;
};

class State
{
    mutable std::shared_mutex mutex_;
    std::mutex writer_;
    std::shared_ptr<const Snapshot> current_;
    std::shared_ptr<const std::filesystem::path> ownership_;

    void resolve(Snapshot &candidate) const
    {
        candidate.effective = options.defaults;
        deep_merge(candidate.effective, candidate.layers);
        deep_merge(candidate.effective, candidate.writable.values);
        deep_merge(candidate.effective, candidate.environment);
        if (options.validate)
        {
            try
            {
                options.validate(candidate.effective);
            }
            catch (const std::exception &e)
            {
                throw Error(ErrorCode::validation, e.what(), file);
            }
            catch (...)
            {
                throw Error(ErrorCode::validation, "Configuration validation failed", file);
            }
        }
    }

    std::vector<std::optional<std::string>> read_sources() const
    {
        std::vector<std::optional<std::string>> sources;
        for (const auto &layer : options.layers)
        {
            auto text = backend->read(layer.file);
            if (!text && !layer.optional)
                throw Error(ErrorCode::read, "Required configuration layer is missing", layer.file);
            sources.push_back(std::move(text));
        }
        sources.push_back(backend->read(file));
        return sources;
    }

    std::shared_ptr<Snapshot> load_candidate(std::vector<std::optional<std::string>> sources) const
    {
        auto candidate     = std::make_shared<Snapshot>();
        candidate->sources = std::move(sources);
        for (std::size_t i = 0; i < options.layers.size(); ++i)
            deep_merge(candidate->layers,
                       decode(candidate->sources[i], options.layers[i].file, options.legacy_encoding).values);
        candidate->writable    = decode(candidate->sources.back(), file, options.legacy_encoding);
        candidate->environment = environment_layer(options);
        resolve(*candidate);
        return candidate;
    }

    void commit(const std::shared_ptr<const Snapshot> &before, std::shared_ptr<Snapshot> candidate, bool persist)
    {
        candidate->revision = before->revision + 1;
        std::optional<std::string> text;
        if (persist)
            text = encode(candidate->writable, options.format, file);
        const bool changed = before->effective != candidate->effective;
        Change event{candidate->revision, before->effective, candidate->effective};
        {
            std::lock_guard writer_lock(writer_);
            if (snapshot() != before)
                throw Error(ErrorCode::conflict, "Store changed while preparing the transaction; retry the operation",
                            file);
            if (persist)
            {
                backend->write(file, *text);
                candidate->sources.back() = std::move(text);
                candidate->dirty          = false;
            }
            {
                std::unique_lock lock(mutex_);
                current_ = std::move(candidate);
            }
            if (changed)
                observers->enqueue(std::move(event));
        }
        observers->drain();
    }

  public:
    const std::filesystem::path file;
    const StoreOptions options;
    const std::shared_ptr<Backend> backend;
    const std::shared_ptr<Observers> observers;

    explicit State(std::filesystem::path path, StoreOptions opts)
        : file(normalize_file(path)), options(prepare_options(std::move(opts))),
          backend(options.backend ? options.backend : std::make_shared<FileBackend>()),
          observers(std::make_shared<Observers>(options.on_callback_error))
    {
        ownership_ = claim_file(file);
        current_   = load_candidate(read_sources());
    }

    static StoreOptions prepare_options(StoreOptions opts)
    {
        if ((opts.save != SaveMode::automatic && opts.save != SaveMode::manual) ||
            (opts.format != Format::pretty && opts.format != Format::compact))
            throw Error(ErrorCode::type, "Unsupported store option");
        require_object(opts.defaults);
        for (auto &layer : opts.layers)
            layer.file = normalize_file(layer.file);
        for (const auto &[key, name] : opts.env_bindings)
        {
            pointer(key);
            if (name.empty())
                throw Error(ErrorCode::path, "Environment binding name is empty");
        }
        return opts;
    }

    std::shared_ptr<const Snapshot> snapshot() const
    {
        std::shared_lock lock(mutex_);
        return current_;
    }

    template <class Edit> void mutate(Edit &&edit)
    {
        const auto before = snapshot();
        auto candidate    = std::make_shared<Snapshot>(*before);
        try
        {
            edit(candidate->writable);
        }
        catch (const json::exception &e)
        {
            throw Error(ErrorCode::path, e.what(), file);
        }
        require_object(candidate->writable.values);
        candidate->dirty = true;
        resolve(*candidate);
        commit(before, std::move(candidate), options.save == SaveMode::automatic);
    }

    void reload(bool only_changed = false)
    {
        const auto before = snapshot();
        auto sources      = read_sources();
        if (only_changed && sources == before->sources)
            return;
        if (only_changed && before->dirty)
            throw Error(ErrorCode::conflict, "Watcher will not discard unsaved changes", file);
        auto candidate = load_candidate(std::move(sources));
        commit(before, std::move(candidate), false);
    }

    void save()
    {
        // 写锁覆盖快照读取与完整写入，旧 save 不能覆盖新状态。
        std::lock_guard writer_lock(writer_);
        const auto before = snapshot();
        auto candidate    = std::make_shared<Snapshot>(*before);
        auto text         = encode(candidate->writable, options.format, file);
        backend->write(file, text);
        candidate->sources.back() = std::move(text);
        candidate->dirty          = false;
        candidate->revision++;
        std::unique_lock lock(mutex_);
        current_ = std::move(candidate);
    }
};
} // namespace config::detail
