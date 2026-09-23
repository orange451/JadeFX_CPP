#include "jadefx/scene/Controls/InlineCssTextArea.hpp"

#include "jadefx/event/Events.hpp"
#include "scene/text/TextCss.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace jadefx {
namespace {

struct Decl {
    std::string name;
    std::string value;
};

std::vector<Decl> Declarations(std::string_view css) {
    std::vector<Decl> decls;
    std::size_t cursor = 0;
    while (cursor < css.size()) {
        const std::size_t end = css.find(';', cursor);
        const std::string_view piece =
            TrimCss(css.substr(cursor, end == std::string_view::npos ? css.size() - cursor : end - cursor));
        cursor = end == std::string_view::npos ? css.size() : end + 1;
        const std::size_t colon = piece.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        Decl decl;
        decl.name = std::string(TrimCss(piece.substr(0, colon)));
        decl.value = std::string(TrimCss(piece.substr(colon + 1)));
        if (!decl.name.empty()) {
            decls.push_back(std::move(decl));
        }
    }
    return decls;
}

std::string Serialize(const std::vector<Decl>& decls) {
    std::string css;
    for (std::size_t i = 0; i < decls.size(); ++i) {
        if (i > 0) {
            css += "; ";
        }
        css += decls[i].name;
        css += ": ";
        css += decls[i].value;
    }
    return css;
}

std::vector<std::string> Words(std::string_view value) {
    std::vector<std::string> words;
    std::size_t index = 0;
    while (index < value.size()) {
        while (index < value.size() && std::isspace(static_cast<unsigned char>(value[index])) != 0) {
            ++index;
        }
        const std::size_t start = index;
        while (index < value.size() && std::isspace(static_cast<unsigned char>(value[index])) == 0) {
            ++index;
        }
        if (start < index) {
            words.emplace_back(value.substr(start, index - start));
        }
    }
    return words;
}

std::string JoinWords(const std::vector<std::string>& words) {
    std::string value;
    for (const std::string& word : words) {
        if (!value.empty()) {
            value.push_back(' ');
        }
        value += word;
    }
    return value;
}

void SetWeight(std::vector<Decl>& decls, bool bold) {
    const auto isWeight = [](const Decl& decl) { return EqualsCss(decl.name, "font-weight"); };
    auto first = std::find_if(decls.begin(), decls.end(), isWeight);
    if (!bold) {
        decls.erase(std::remove_if(decls.begin(), decls.end(), isWeight), decls.end());
        return;
    }
    if (first == decls.end()) {
        decls.push_back(Decl{"font-weight", "bold"});
        return;
    }
    first->value = "bold";
    const auto later = std::next(first);
    decls.erase(std::remove_if(later, decls.end(), isWeight), decls.end());
}

void SetUnderline(std::vector<Decl>& decls, bool underline) {
    const auto isDecoration = [](const Decl& decl) { return EqualsCss(decl.name, "text-decoration"); };
    std::vector<std::string> tokens;
    for (const Decl& decl : decls) {
        if (!isDecoration(decl)) {
            continue;
        }
        for (const std::string& word : Words(decl.value)) {
            if (EqualsCss(word, "none") || EqualsCss(word, "underline")) {
                continue;
            }
            const bool seen = std::any_of(tokens.begin(), tokens.end(), [&](const std::string& token) {
                return EqualsCss(token, word);
            });
            if (!seen) {
                tokens.push_back(word);
            }
        }
    }
    if (underline) {
        tokens.push_back("underline");
    }
    decls.erase(std::remove_if(decls.begin(), decls.end(), isDecoration), decls.end());
    const std::string value = JoinWords(tokens);
    if (!value.empty()) {
        decls.push_back(Decl{"text-decoration", value});
    }
}

std::string RewriteCss(std::string_view css, bool underline, bool enable) {
    std::vector<Decl> decls = Declarations(css);
    if (underline) {
        SetUnderline(decls, enable);
    } else {
        SetWeight(decls, enable);
    }
    return Serialize(decls);
}

}  // namespace

void InlineCssTextArea::setStyle(int start, int end, const std::string& css) {
    StyledTextArea::setStyle(start, end, ParseTextCss(css));
}

void InlineCssTextArea::toggleDecoration(bool underline) {
    if (!isEditable()) {
        return;
    }
    std::vector<IndexRange> ranges = selections();
    bool enable = false;
    bool anyRange = false;
    for (const IndexRange& range : ranges) {
        if (range.empty()) {
            const TextStyle current = styleForInsertion(range.start);
            if (underline ? !current.underline : !current.bold) {
                enable = true;
            }
            continue;
        }
        anyRange = true;
        const StyleSpans spans = getStyleSpans(range.start, range.end);
        for (const StyleSpan& span : spans.spans()) {
            if (underline ? !span.style.underline : !span.style.bold) {
                enable = true;
            }
        }
    }
    if (!anyRange) {
        const TextStyle current = styleForInsertion(caretPosition());
        setTypingStyle(ParseTextCss(RewriteCss(current.inlineCss, underline, enable)));
        return;
    }
    std::sort(ranges.begin(), ranges.end(), [](const IndexRange& a, const IndexRange& b) { return a.start > b.start; });
    transact(false, [&] {
        for (const IndexRange& range : ranges) {
            if (range.empty()) {
                continue;
            }
            const StyleSpans spans = getStyleSpans(range.start, range.end);
            StyleSpansBuilder builder;
            for (const StyleSpan& span : spans.spans()) {
                builder.add(ParseTextCss(RewriteCss(span.style.inlineCss, underline, enable)), span.length);
            }
            setStyleSpans(range.start, builder.create(), true);
        }
    });
}

void InlineCssTextArea::handleKey(KeyEvent& event) {
    if ((event.pressed || event.repeat) && event.shortcut()) {
        if (event.key == Key::B) {
            toggleDecoration(false);
            event.consume();
            return;
        }
        if (event.key == Key::U) {
            toggleDecoration(true);
            event.consume();
            return;
        }
    }
    StyledTextArea::handleKey(event);
}

}  // namespace jadefx
