#pragma once

#include <array>
#include <vector>

namespace jadefx {

// The values last given to each of one shader program's uniforms. GL keeps a
// uniform's value with its program, whatever program is bound since, so a draw
// that would set the same value again can skip the call. A slot holds up to
// four floats. reset forgets everything, for when the program is made again.
class UniformCache {
public:
    explicit UniformCache(int slots) : slots_(slots > 0 ? static_cast<std::size_t>(slots) : 0) {}

    // True, recording the values, when they differ from the slot's last ones or
    // it has none; the caller then sets the uniform. A slot out of range is
    // always true and never recorded.
    bool changed(int slot, float a, float b = 0.f, float c = 0.f, float d = 0.f) {
        if (slot < 0 || static_cast<std::size_t>(slot) >= slots_.size()) {
            return true;
        }
        Slot& held = slots_[static_cast<std::size_t>(slot)];
        const std::array<float, 4> next{a, b, c, d};
        if (held.set && held.values == next) {
            return false;
        }
        held.set = true;
        held.values = next;
        return true;
    }

    void reset() {
        for (Slot& held : slots_) {
            held.set = false;
        }
    }

private:
    struct Slot {
        bool set = false;
        std::array<float, 4> values{};
    };
    std::vector<Slot> slots_;
};

}  // namespace jadefx
