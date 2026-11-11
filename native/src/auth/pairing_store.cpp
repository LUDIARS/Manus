#include "auth/pairing_store.hpp"

#include <nlohmann/json.hpp>

#include "crypto/crypto.hpp"
#include "platform/file_read.hpp"
#include "util/bounded_json.hpp"

namespace manus::auth {
namespace {

constexpr util::JsonLimits kPairingFileLimits{64 * 1024, 1024, 16};
constexpr std::size_t kSecretBytes = 32;       // what pairing.ts generated
constexpr std::size_t kMaxSecretBytes = 4096;  // anything larger is not ours

}  // namespace

PairingFiles pairing_files(const std::filesystem::path& root) {
    return {root / "data" / "pairing.json", root / "data" / "session-secret"};
}

LoadedPairing load_pairing(const std::filesystem::path& root) {
    const PairingFiles files = pairing_files(root);
    LoadedPairing result;

    const auto pairing = platform::read_file_bounded(files.pairing, kPairingFileLimits.max_bytes);
    if (pairing.status == platform::FileReadStatus::missing) {
        result.error = "[fatal] ペアリング PIN が未設定です。先に `manus set-pin <pin>` を実行してください (" +
                       files.pairing.string() + ")";
        return result;
    }
    if (pairing.status != platform::FileReadStatus::ok) {
        result.error = "[fatal] " + files.pairing.string() + " を読めません (64 KiB 以下の通常ファイルが必要です)";
        return result;
    }
    const auto parsed = util::parse_bounded_json(pairing.bytes, kPairingFileLimits);
    if (!parsed.value || !parsed.value->is_object() || !parsed.value->contains("salt") ||
        !(*parsed.value)["salt"].is_string() || !parsed.value->contains("pinHash") ||
        !(*parsed.value)["pinHash"].is_string()) {
        result.error = "[fatal] " + files.pairing.string() + " の形式が不正です。set-pin で設定し直してください";
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(files.secret, ec)) {
        std::string write_error;
        if (!platform::write_file_atomic(files.secret, crypto::random_bytes(kSecretBytes), write_error)) {
            result.error = "[fatal] session-secret を作成できません: " + write_error;
            return result;
        }
    }
    const auto secret = platform::read_file_bounded(files.secret, kMaxSecretBytes);
    if (secret.status != platform::FileReadStatus::ok || secret.bytes.size() < kSecretBytes) {
        result.error = "[fatal] " + files.secret.string() +
                       " が不正です (32〜4096 バイトが必要です)。削除して再起動すると再生成されます (発行済みトークンは無効になります)";
        return result;
    }

    result.pin = StoredPin{(*parsed.value)["salt"].get<std::string>(), (*parsed.value)["pinHash"].get<std::string>()};
    result.secret = secret.bytes;
    return result;
}

bool store_pin(const std::filesystem::path& root, std::string_view pin, std::string& error) {
    if (!is_valid_pin_format(pin)) {
        error = "PIN は 4〜10 桁の数字で指定してください";
        return false;
    }
    const std::string salt = crypto::to_hex(crypto::random_bytes(16));
    nlohmann::ordered_json doc;
    doc["salt"] = salt;
    doc["pinHash"] = hash_pin(pin, salt);
    return platform::write_file_atomic(pairing_files(root).pairing, doc.dump(), error);
}

}  // namespace manus::auth
