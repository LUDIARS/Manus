#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace manus::inject {

// JIS 日本語 108 キー Qwerty のキー名 -> Windows 仮想キーコード (src/inject/jis-vk-map.ts).
// key メッセージ / ショートカット定義の語彙の正本。文字入力は Unicode 注入を使う。

enum class Modifier { ctrl, shift, alt, win };

std::uint16_t modifier_vk(Modifier modifier);
std::optional<Modifier> parse_modifier(std::string_view name);

// Every key name with its VK, in the order of the TS table.
const std::vector<std::pair<std::string_view, std::uint16_t>>& jis_vk_table();

// vkOf without the throw: nullopt for an unknown name. Only the table's own
// names match (no inherited JS object properties such as "constructor").
std::optional<std::uint16_t> find_vk(std::string_view key);

}  // namespace manus::inject
