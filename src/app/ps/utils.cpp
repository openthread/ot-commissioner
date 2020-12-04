#include "utils.hpp"

#include <cstring>

namespace ot {

namespace commissioner {

namespace {

uint16_t tg_random_15bit_get()
{
    std::srand(static_cast<unsigned>(std::time(NULL)));

    return std::rand() % 0x7FFF;
}

} // namespace

template <> std::string int_to_hex(std::uint8_t const &val)
{
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(2) << std::hex << (int)val;
    return stream.str();
}

std::string arr_to_hex(std::uint8_t const *arr, size_t len)
{
    if (arr == nullptr || len == 0)
    {
        return "";
    }

    std::string tmp;
    for (size_t i = 0; i < len; ++i)
    {
        tmp += int_to_hex<std::uint8_t>(arr[i]);
    }

    return tmp;
}

bool hex_to_arr(std::string const &hxstr, std::uint8_t *arr, size_t len)
{
    if (arr == nullptr || len == 0)
    {
        return false;
    }
    std::memset(arr, 0, len);

    size_t hlen = hxstr.length();

    if (hlen == 0)
    {
        return true;
    }

    // only two symbols per byte supported
    if (hlen % 2 == 1)
    {
        return false;
    }

    if (len * 2 < hlen)
    {
        return false;
    }

    for (size_t i = 0; (2 * i < hlen) && (i < len); ++i)
    {
        std::string bytes = hxstr.substr(2 * i, 2);
        arr[i]            = (std::uint8_t)std::stoul(bytes, nullptr, 16);
    }

    return true;
}

void fake_function()
{
    int a = 0;
    a += 1;
}

bool str_cmp_icase(std::string const &str1, std::string const &str2)
{
    return ((str1.size() == str2.size()) &&
            std::equal(std::begin(str1), std::end(str1), std::begin(str2), [](char const &c1, char const &c2) {
                return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
            }));
}

int odts_cmp(Timestamp const &a, Timestamp const &b)
{
    if (a.mSeconds > b.mSeconds)
    {
        return 1;
    }

    if (a.mSeconds < b.mSeconds)
    {
        return -1;
    }

    if (a.mTicks > b.mTicks)
    {
        return 1;
    }

    if (a.mTicks < b.mTicks)
    {
        return -1;
    }

    return 0;
}

void odts_inc_rnd(Timestamp &ts)
{
    std::uint16_t old = ts.mTicks;
    ts.mTicks += tg_random_15bit_get();
    if (ts.mTicks < old)
    {
        ++ts.mSeconds;
    }
}

std::uint32_t sec_to_msec(std::uint32_t sec)
{
    return sec * 1000;
}

std::string ret_describe(registry_error_t ret_code)
{
    std::string ret_dscr;

    switch (ret_code)
    {
    case RE_SUCCESS:
        ret_dscr = "Operation succeeded";
        break;
    case RE_FAILURE:
        ret_dscr = "Generic failure";
        break;
    case RE_ERROR_BAD_PARAMETER:
        ret_dscr = "Unacceptable parameter value";
        break;
    case RE_ERROR_INCOMPLETE_DATA:
        ret_dscr = "Insufficient data for call";
        break;
    case RE_ERROR_COAP:
        ret_dscr = "Libcoap error";
        break;
    case RE_ERROR_NETWORK:
        ret_dscr = "Network stack error";
        break;
    case RE_ERROR_SM_NOT_FOUND:
        ret_dscr = "Security Materials not found";
        break;
    case RE_ERROR_NO_MEM:
        ret_dscr = "Memory allocation error";
        break;
    case RE_ERROR_ALREADY_EXISTS:
        ret_dscr = "Instance already exists";
        break;
    case RE_ERROR_NOT_FOUND:
        ret_dscr = "Instance not found";
        break;
    case RE_ERROR_ALREADY_ACTIVE:
        ret_dscr = "Already has ACTIVE status";
        break;
    case RE_ERROR_NOT_SUPPORTED:
        ret_dscr = "Feature not supported or not implemented";
        break;

    default:
        ret_dscr = "Unknown status code";
    }

    return ret_dscr;
}

} // namespace commissioner

} // namespace ot

std::ostream &operator<<(std::ostream &os, ot::commissioner::Timestamp const &ts)
{
    os << "{uxt_sec: " << ts.mSeconds << " uxt_frac: " << ts.mTicks << " bit_u: " << ts.mU << "}";
    return os;
}
