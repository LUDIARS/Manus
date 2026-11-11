#include "inject/keyboard.hpp"

namespace manus::inject {

TextCommand text_command(std::string text) { return TextCommand{std::move(text)}; }

KeyCommandResult key_command(std::string_view key, const std::vector<Modifier>& modifiers, KeyAction action) {
    std::vector<std::uint16_t> vks;
    vks.reserve(modifiers.size() + 1);
    for (const Modifier m : modifiers) vks.push_back(modifier_vk(m));
    const auto vk = find_vk(key);
    if (!vk) return {std::nullopt, "unknown key name: " + std::string(key)};
    vks.push_back(*vk);
    return {KeyCommand{std::move(vks), action}, {}};
}

}  // namespace manus::inject
