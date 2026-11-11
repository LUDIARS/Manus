#include "inject/jis_vk_map.hpp"

#include <string>

namespace manus::inject {
namespace {

std::vector<std::pair<std::string_view, std::uint16_t>> build_table() {
    std::vector<std::pair<std::string_view, std::uint16_t>> t = {
        // 制御
        {"enter", 0x0d}, {"esc", 0x1b}, {"tab", 0x09}, {"space", 0x20}, {"bs", 0x08}, {"del", 0x2e}, {"ins", 0x2d},
        // ナビゲーション
        {"up", 0x26}, {"down", 0x28}, {"left", 0x25}, {"right", 0x27}, {"home", 0x24}, {"end", 0x23},
        {"pgup", 0x21}, {"pgdn", 0x22},
        // ファンクション
        {"f1", 0x70}, {"f2", 0x71}, {"f3", 0x72}, {"f4", 0x73}, {"f5", 0x74}, {"f6", 0x75},
        {"f7", 0x76}, {"f8", 0x77}, {"f9", 0x78}, {"f10", 0x79}, {"f11", 0x7a}, {"f12", 0x7b},
        // JIS 固有 (日本語 108 キー)
        {"hankaku", 0xf3},   // 半角/全角 (VK_OEM_AUTO)
        {"henkan", 0x1c},    // 変換 (VK_CONVERT)
        {"muhenkan", 0x1d},  // 無変換 (VK_NONCONVERT)
        {"kana", 0x15},      // カタカナ/ひらがな (VK_KANA)
        {"eisu", 0x14},      // 英数 (VK_CAPITAL = CapsLock/英数)
    };
    // 英数字 (VK は ASCII 一致)
    static constexpr std::string_view letters = "abcdefghijklmnopqrstuvwxyz";
    for (std::size_t i = 0; i < letters.size(); ++i) {
        t.emplace_back(letters.substr(i, 1), static_cast<std::uint16_t>('A' + i));
    }
    static constexpr std::string_view digits = "0123456789";
    for (std::size_t i = 0; i < digits.size(); ++i) {
        t.emplace_back(digits.substr(i, 1), static_cast<std::uint16_t>('0' + i));
    }
    const std::pair<std::string_view, std::uint16_t> rest[] = {
        // JIS 記号キー (物理キー等価。Shift 面は modifiers で表現する)
        {"minus", 0xbd}, {"caret", 0xde}, {"yen", 0xdc}, {"at", 0xc0}, {"lbracket", 0xdb}, {"rbracket", 0xdd},
        {"semicolon", 0xbb}, {"colon", 0xba}, {"comma", 0xbc}, {"period", 0xbe}, {"slash", 0xbf},
        {"backslash", 0xe2},  // \_ (JIS ろ、VK_OEM_102)
        // テンキー
        {"num0", 0x60}, {"num1", 0x61}, {"num2", 0x62}, {"num3", 0x63}, {"num4", 0x64},
        {"num5", 0x65}, {"num6", 0x66}, {"num7", 0x67}, {"num8", 0x68}, {"num9", 0x69},
        {"nummul", 0x6a}, {"numadd", 0x6b}, {"numsub", 0x6d}, {"numdec", 0x6e}, {"numdiv", 0x6f},
        {"numlock", 0x90},
        // その他
        {"capslock", 0x14}, {"printscreen", 0x2c}, {"scrolllock", 0x91}, {"pause", 0x13},
        {"apps", 0x5d},  // メニューキー
    };
    for (const auto& entry : rest) t.push_back(entry);
    return t;
}

}  // namespace

std::uint16_t modifier_vk(Modifier modifier) {
    switch (modifier) {
        case Modifier::ctrl: return 0x11;
        case Modifier::shift: return 0x10;
        case Modifier::alt: return 0x12;
        case Modifier::win: return 0x5b;
    }
    return 0;
}

std::optional<Modifier> parse_modifier(std::string_view name) {
    if (name == "ctrl") return Modifier::ctrl;
    if (name == "shift") return Modifier::shift;
    if (name == "alt") return Modifier::alt;
    if (name == "win") return Modifier::win;
    return std::nullopt;
}

const std::vector<std::pair<std::string_view, std::uint16_t>>& jis_vk_table() {
    static const auto table = build_table();
    return table;
}

std::optional<std::uint16_t> find_vk(std::string_view key) {
    for (const auto& [name, vk] : jis_vk_table()) {
        if (name == key) return vk;
    }
    return std::nullopt;
}

}  // namespace manus::inject
