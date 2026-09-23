#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace jadefx {

// A list that tells its owner when an item arrives or leaves.
// JadeFX uses that to parent scene-graph nodes as they are added.
// An indexed callback replaces the plain one for that event, so a listener runs once.
template <typename T>
class ObservableList {
public:
    using Callback = std::function<void(const T&)>;
    using IndexedCallback = std::function<void(T item, std::size_t index)>;

    void setAddCallback(Callback callback) { onAdd_ = std::move(callback); }
    void setRemoveCallback(Callback callback) { onRemove_ = std::move(callback); }
    void setIndexedAddCallback(IndexedCallback callback) { onAddAt_ = std::move(callback); }
    void setIndexedRemoveCallback(IndexedCallback callback) { onRemoveAt_ = std::move(callback); }

    void add(const T& item) { insert(items_.size(), item); }
    void add(T&& item) { insert(items_.size(), std::move(item)); }

    void insert(std::size_t index, const T& item) {
        if (index > items_.size()) {
            index = items_.size();
        }
        items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(index), item);
        added(index);
    }

    void insert(std::size_t index, T&& item) {
        if (index > items_.size()) {
            index = items_.size();
        }
        items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(index), std::move(item));
        added(index);
    }

    void removeAt(std::size_t index) {
        if (index >= items_.size()) {
            return;
        }
        T removed = std::move(items_[index]);
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
        // The local copy stays alive for the callback, even if the listener drops its own pointer.
        if (onRemoveAt_) {
            onRemoveAt_(removed, index);
        } else if (onRemove_) {
            onRemove_(removed);
        }
    }

    template <typename Predicate>
    void removeIf(Predicate predicate) {
        for (std::size_t i = 0; i < items_.size();) {
            if (predicate(items_[i])) {
                removeAt(i);
            } else {
                ++i;
            }
        }
    }

    void clear() {
        while (!items_.empty()) {
            removeAt(items_.size() - 1);
        }
    }

    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    T& operator[](std::size_t index) { return items_[index]; }
    const T& operator[](std::size_t index) const { return items_[index]; }

    const std::vector<T>& items() const { return items_; }

private:
    void added(std::size_t index) {
        // Copy first. The listener may insert or erase, which can move the vector storage.
        T item = items_[index];
        if (onAddAt_) {
            onAddAt_(std::move(item), index);
        } else if (onAdd_) {
            onAdd_(item);
        }
    }

    std::vector<T> items_;
    Callback onAdd_;
    Callback onRemove_;
    IndexedCallback onAddAt_;
    IndexedCallback onRemoveAt_;
};

}  // namespace jadefx
