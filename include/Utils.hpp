#include <Poco/Util/IniFileConfiguration.h>
#include <Poco/Logger.h>

#pragma once

extern Poco::AutoPtr<Poco::Util::IniFileConfiguration> config;

std::ostream&
operator<<( std::ostream& dest, __int128_t value );
namespace std {
std::string to_string(__int128_t value);
}

