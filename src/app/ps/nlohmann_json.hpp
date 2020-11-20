/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief Includes nlohmann json to suppress warning from some compilers
 */

#ifndef _NLOHMANN_JSON_HPP_
#define _NLOHMANN_JSON_HPP_

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

#include <nlohmann/json.hpp>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // _NLOHMANN_JSON_HPP_
