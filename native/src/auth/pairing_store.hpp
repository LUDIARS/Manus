#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "auth/pairing.hpp"

namespace manus::auth {

struct PairingFiles {
    std::filesystem::path pairing;  // data/pairing.json  {"salt":"<32 hex>","pinHash":"<64 hex>"}
    std::filesystem::path secret;   // data/session-secret (raw bytes, created on first start)
};

PairingFiles pairing_files(const std::filesystem::path& root);

struct LoadedPairing {
    std::optional<StoredPin> pin;
    std::string secret;
    std::string error;  // set when pin is empty
};

// Reads the stored PIN hash (fail-fast when it was never set) and the token
// signing secret, creating a 32-byte secret on first start as pairing.ts did.
LoadedPairing load_pairing(const std::filesystem::path& root);

// set-pin: validates the PIN format, then writes a fresh salt and the salted
// hash. The PIN itself is never stored. Returns false with error on failure.
bool store_pin(const std::filesystem::path& root, std::string_view pin, std::string& error);

}  // namespace manus::auth
