#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace jadefx {

// A list that tells its owner when an item arrives or leaves.
// JadeFX uses that to parent scene-graph nodes as they are added.
// An indexed callback replaces the plain one for that event, so a callback runs once.
// Any number of listeners can watch as well, in the shape of JavaFX's
// ListChangeListener: each hears every change after the owner's callback.
template <typename T>
class ObservableList {
public:
    using Callback = std::function<void(const T&)>;
    using IndexedCallback = std::function<void(T item, std::size_t index)>;

    // One item arrived at index, left from index, or was replaced there.
    // item is the item that arrived, left, or replaced the old one.
    struct Change {
        enum class Kind { Added, Removed, Replaced };
        Kind kind = Kind::Added;
        std::size_t index = 0;
        const T& item;
    };
    using Listener = std::function<void(const Change&)>;
    using ListenerId = std::size_t;

    // The id removes the listener again.
    ListenerId addListener(Listener listener) {
        listeners_.push_back({++lastListener_, std::move(listener)});
        return lastListener_;
    }
    void removeListener(ListenerId id) {
        for (std::size_t i = 0; i < listeners_.size(); ++i) {
            if (listeners_[i].id == id) {
                listeners_.erase(listeners_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

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
        notify(Change::Kind::Removed, index, removed);
    }

    // Replaces the item at index. The owner's callbacks see the old item leave
    // and the new one arrive; listeners see one Replaced change.
    void set(std::size_t index, T item) {
        if (index >= items_.size()) {
            return;
        }
        T removed = std::move(items_[index]);
        items_[index] = std::move(item);
        if (onRemoveAt_) {
            onRemoveAt_(removed, index);
        } else if (onRemove_) {
            onRemove_(removed);
        }
        T added = items_[index];
        if (onAddAt_) {
            onAddAt_(added, index);
        } else if (onAdd_) {
            onAdd_(added);
        }
        notify(Change::Kind::Replaced, index, added);
    }

    // Clears the list, then adds each item in order.
    void setAll(std::vector<T> items) {
        clear();
        for (T& item : items) {
            add(std::move(item));
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
    typename std::vector<T>::const_iterator begin() const { return items_.begin(); }
    typename std::vector<T>::const_iterator end() const { return items_.end(); }

private:
    void added(std::size_t index) {
        // Copy first. The listener may insert or erase, which can move the vector storage.
        T item = items_[index];
        if (onAddAt_) {
            onAddAt_(item, index);
        } else if (onAdd_) {
            onAdd_(item);
        }
        notify(Change::Kind::Added, index, item);
    }

    void notify(typename Change::Kind kind, std::size_t index, const T& item) {
        if (listeners_.empty()) {
            return;
        }
        // A listener may add or remove listeners, so the round runs on a copy.
        const std::vector<Entry> round = listeners_;
        const Change change{kind, index, item};
        for (const Entry& entry : round) {
            if (entry.listener) {
                entry.listener(change);
            }
        }
    }

    struct Entry {
        ListenerId id = 0;
        Listener listener;
    };

    std::vector<T> items_;
    Callback onAdd_;
    Callback onRemove_;
    IndexedCallback onAddAt_;
    IndexedCallback onRemoveAt_;
    std::vector<Entry> listeners_;
    ListenerId lastListener_ = 0;
};

}  // namespace jadefx
