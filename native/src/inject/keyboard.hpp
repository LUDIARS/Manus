#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "inject/command.hpp"
#include "inject/jis_vk_map.hpp"

namespace manus::inject {

// 文字列を Unicode 注入コマンドにする (改行/タブは入力計画で Enter/Tab になる)。
TextCommand text_command(std::string text);

struct KeyCommandResult {
    std::optional<KeyCommand> command;
    std::string error;  // "unknown key name: <key>" (vkOf's message)
};

// キー + 修飾キーを VK 列にする。修飾キーを指定順で先頭に、対象キーを末尾に置く
// (押し順・離し順をここで配列の順序として確定させる)。
KeyCommandResult key_command(std::string_view key, const std::vector<Modifier>& modifiers = {},
                             KeyAction action = KeyAction::press);

}  // namespace manus::inject
