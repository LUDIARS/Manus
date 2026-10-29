#include "util/bounded_json.hpp"

#include <utility>
#include <vector>

#include <lorica/input_budget.hpp>

namespace manus::util {
namespace {

using json = nlohmann::json;

// Builds a DOM like nlohmann's own SAX DOM parser, but charges the budget
// before each value is stored and refuses nesting beyond the depth limit.
class BudgetedDomBuilder {
public:
    explicit BudgetedDomBuilder(lorica::input_budget& budget) : budget_(budget) {}

    bool null() { return add(json(nullptr), 0); }
    bool boolean(bool v) { return add(json(v), 0); }
    bool number_integer(json::number_integer_t v) { return add(json(v), 0); }
    bool number_unsigned(json::number_unsigned_t v) { return add(json(v), 0); }
    bool number_float(json::number_float_t v, const json::string_t&) { return add(json(v), 0); }
    bool string(json::string_t& v) {
        const std::size_t size = v.size();
        return add(json(std::move(v)), size);
    }
    bool binary(json::binary_t&) { return fail("binary values are not JSON"); }

    bool start_object(std::size_t) { return open(json::value_t::object); }
    bool end_object() { return close(); }
    bool start_array(std::size_t) { return open(json::value_t::array); }
    bool end_array() { return close(); }

    bool key(json::string_t& k) {
        if (!budget_.consume(k.size(), 1, 1)) return fail("JSON exceeds the input budget");
        pending_key_ = std::move(k);
        return true;
    }

    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception& ex) {
        return fail(ex.what());
    }

    json take() { return std::move(root_); }
    const std::string& error() const { return error_; }

private:
    bool fail(std::string message) {
        if (error_.empty()) error_ = std::move(message);
        return false;
    }

    json* place(json value) {
        if (stack_.empty()) {
            root_ = std::move(value);
            return &root_;
        }
        json& parent = *stack_.back();
        if (parent.is_array()) {
            parent.push_back(std::move(value));
            return &parent.back();
        }
        json& slot = parent[pending_key_];
        slot = std::move(value);
        return &slot;
    }

    bool add(json value, std::size_t bytes) {
        if (!budget_.consume(bytes, 1, bytes == 0 ? 0 : 1)) return fail("JSON exceeds the input budget");
        place(std::move(value));
        return true;
    }

    bool open(json::value_t kind) {
        const std::size_t depth = stack_.size() + 1;
        if (!budget_.permits_depth(depth)) return fail("JSON nesting is too deep");
        if (!budget_.consume(0, 1, 1)) return fail("JSON exceeds the input budget");
        stack_.push_back(place(json(kind)));
        return true;
    }

    bool close() {
        if (stack_.empty()) return fail("unbalanced JSON");
        stack_.pop_back();
        return true;
    }

    lorica::input_budget& budget_;
    json root_;
    std::vector<json*> stack_;
    std::string pending_key_;
    std::string error_;
};

}  // namespace

JsonParseResult parse_bounded_json(std::string_view text, const JsonLimits& limits) {
    lorica::input_budget budget({limits.max_bytes, limits.max_elements, limits.max_elements, limits.max_depth});
    // The raw text is charged before the parser allocates anything for it.
    if (!budget.consume(text.size(), 0, 1)) return {std::nullopt, "JSON text is too large"};
    // The decoded strings are charged again as they are produced, against the
    // same byte allowance, so the expanded size is bounded too.
    lorica::input_budget decoded({limits.max_bytes, limits.max_elements, limits.max_elements, limits.max_depth});
    BudgetedDomBuilder builder(decoded);
    bool ok = false;
    try {
        ok = json::sax_parse(text.data(), text.data() + text.size(), &builder, json::input_format_t::json, true, false);
    } catch (const std::exception& ex) {
        return {std::nullopt, ex.what()};
    }
    if (!ok) {
        return {std::nullopt, builder.error().empty() ? std::string("invalid JSON") : builder.error()};
    }
    return {builder.take(), {}};
}

}  // namespace manus::util
