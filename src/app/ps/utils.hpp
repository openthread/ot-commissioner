/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief Common util functions
 */

#ifndef _CLI_UTILS_HPP_
#define _CLI_UTILS_HPP_

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

#include <commissioner/network_data.hpp>

namespace ot {

namespace commissioner {

/**
 * Registry error codes.
 * TODO convert to Error class later.
 */
enum registry_error_t
{
    RE_SUCCESS,               /**< operation succeeded */
    RE_FAILURE,               /**< generic failure */
    RE_ERROR_BAD_PARAMETER,   /**< unacceptable parameter value */
    RE_ERROR_INCOMPLETE_DATA, /**< insufficient data for call */
    RE_ERROR_COAP,            /**< libcoap error */
    RE_ERROR_NETWORK,         /**< Network stack error */
    RE_ERROR_SM_NOT_FOUND,    /**< Security Materials not found */
    RE_ERROR_NO_MEM,          /**< memory allocation error */
    RE_ERROR_ALREADY_EXISTS,  /**< instance already exists */
    RE_ERROR_NOT_FOUND,       /**< instance not found */
    RE_ERROR_ALREADY_ACTIVE,  /**< Already has ACTIVE status */
    RE_ERROR_TIMEOUT,         /**< Timeout expired */
    RE_ERROR_NOT_SUPPORTED,   /**< Feature not supported or implemented */
};

/**
 * Converts int type value to HEX string
 *
 * @param[in] val input value
 * @return HEX string without leading 0x
 */
template <typename T> std::string int_to_hex(T const &val)
{
    size_t            hex_len = sizeof(T) * 2;
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(hex_len) << std::hex << val;
    return stream.str();
}

template <> std::string int_to_hex(std::uint8_t const &val);

/**
 * Converts array of bytes to HEX string
 *
 * @param[in] arr array to convert
 * @param[in] len length of the array
 * @return HEX string without leading 0x
 */
std::string arr_to_hex(std::uint8_t const *arr, size_t len);

/**
 * Converts HEX string to array of bytes
 *
 * @param[in] hxstr hex string
 * @param[out] arr array to be filled
 * @param[in] len array len
 * @return true if array len sufficient
 */
bool hex_to_arr(std::string const &hxstr, std::uint8_t *arr, size_t len);

/**
 * Case insensitive string comparison
 *
 * @param[in] str1 string to compare
 * @param[in] str2 string to compare
 * @return true if strings are equal
 */
bool str_cmp_icase(std::string const &str1, std::string const &str2);

/**
 * General purpose std::string serializer
 *
 * @param[in] val value to be serialized
 * @return std::strings repr of val
 */
struct str_serializer
{
    /**
     * If type cannot be directly converted to std::string
     * std::stringstream is used
     */
    template <typename T>
    typename std::enable_if<!std::is_assignable<T, std::string>::value, std::string>::type serialize(T const &val)
    {
        std::stringstream strm;
        strm << val;
        return strm.str();
    }

    /**
     * If type can be directly converted to std::string
     * simple conversion is used
     */
    template <typename T>
    typename std::enable_if<std::is_assignable<T, std::string>::value, std::string>::type serialize(T const &val)
    {
        return val;
    }
};

/**
 * Compares two tg_od_timestamp_t
 *
 * @params[in] a first ts
 * @params[in] b second ts
 * @return 1 a > b; 0 a == b; -1 a < b
 */
int odts_cmp(Timestamp const &a, Timestamp const &b);

/**
 * Adds 15 random bits to frac
 *
 * @params[in] ts timestamp to inc
 */
void odts_inc_rnd(Timestamp &ts);

/**
 * Converts sec to msec
 *
 * @params[in] sec input in sec
 * @return sec * 1000
 */
std::uint32_t sec_to_msec(std::uint32_t sec);

/**
 * Describes tg_ret_t status
 *
 * @params[in] sec input in sec
 * @return sec * 1000
 */
std::string ret_describe(std::uint32_t ret_code);

} // namespace commissioner

} // namespace ot

/**
 * Extends logger to handle Timestamp
 */
std::ostream &operator<<(std::ostream &os, ot::commissioner::Timestamp const &ts);

#endif // _CLI_UTILS_HPP_
