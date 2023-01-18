#include <sstream>
#include "OrderBook.hpp"

namespace {
  static std::string null_name("NULL");
}

SymbolFactory SymbolFactory::_factory;

std::ostream&
operator<<( std::ostream& dest, __int128_t value )
{
    std::ostream::sentry s( dest );
    if ( s ) {
        __uint128_t tmp = value < 0 ? -value : value;
        char buffer[ 128 ];
        char* d = std::end( buffer );
        do
        {
            -- d;
            *d = "0123456789"[ tmp % 10 ];
            tmp /= 10;
        } while ( tmp != 0 );
        if ( value < 0 ) {
            -- d;
            *d = '-';
        }
        int len = std::end( buffer ) - d;
        if ( dest.rdbuf()->sputn( d, len ) != len ) {
            dest.setstate( std::ios_base::badbit );
        }
    }
    return dest;
}

NullOrderBook::NullOrderBook(Symbol& s1, Symbol& s2) {
  symbol1 = SymbolFactory::get_factory().get_symbol_index(s1.get_symbol());
  symbol2 = SymbolFactory::get_factory().get_symbol_index(s2.get_symbol());
}

NullOrderBook::NullOrderBook() : symbol1(-1), symbol2(-1) {
}

Symbol& NullOrderBook::get_symbol_1() const {
  return SymbolFactory::get_factory().get_symbol(symbol1);;
}

Symbol& NullOrderBook::get_symbol_2() const {
  return SymbolFactory::get_factory().get_symbol(symbol2);
}

__int128 NullOrderBook::estimate_conversion_from_1(__int128 amount) const {
  return 0;
};
__int128 NullOrderBook::estimate_conversion_from_2(__int128 amount) const {
  return 0;
};
__int128 NullOrderBook::estimate_fee_from_1(__int128 amount) const {
  return 1LL << 58;
};
__int128 NullOrderBook::estimate_fee_from_2(__int128 amount) const {
  return 1LL << 58;
};
void NullOrderBook::update() {}

std::string NullOrderBook::print() const { return "Non-existing pair"; }

bool operator<(const Symbol& first, const Symbol& second) {
  return first.get_symbol() < second.get_symbol();
}

bool operator==(const Symbol& first, const Symbol& second) {
  return first.get_symbol() == second.get_symbol();
}


Symbol::Symbol(std::string& symbol, std::string& name) : _name(name), _symbol(symbol) {}
Symbol::Symbol(Symbol&& other) {
  _name = std::move(other._name);
  _symbol = std::move(other._symbol);
  _reference_rate_estimate = other._reference_rate_estimate;
}
Symbol::Symbol(const Symbol& other) {
  _name = other._name;
  _symbol = other._symbol;
  _reference_rate_estimate = other._reference_rate_estimate;
}
std::string Symbol::get_symbol() const { return _symbol; }
std::string Symbol::get_name() const { return _name; }
__int128 Symbol::get_reference_rate_estimate() const { return _reference_rate_estimate; }
void Symbol::set_reference_rate_estimate(__int128 reference_rate_estimate) const {
  _reference_rate_estimate = reference_rate_estimate;
}

Symbol& Symbol::operator=(const Symbol& other) {
  _name = other._name;
  _symbol = other._symbol;
  _reference_rate_estimate = other._reference_rate_estimate;
  return *this;
}

NullOrderBook ReverseOrderBook::_nb = NullOrderBook();
ReverseOrderBook::ReverseOrderBook() : _orig(_nb) {};
ReverseOrderBook::ReverseOrderBook(GenericOrderBook& orig) : _orig(orig) {};
Symbol& ReverseOrderBook::get_symbol_1() const { return _orig.get_symbol_2();};
Symbol& ReverseOrderBook::get_symbol_2() const { return _orig.get_symbol_1();};
__int128 ReverseOrderBook::estimate_conversion_from_1(__int128 amount) const { return _orig.estimate_conversion_from_2(amount);};
__int128 ReverseOrderBook::estimate_conversion_from_2(__int128 amount) const { return _orig.estimate_conversion_from_1(amount);};
__int128 ReverseOrderBook::estimate_fee_from_1(__int128 amount) const { return _orig.estimate_fee_from_2(amount);};
__int128 ReverseOrderBook::estimate_fee_from_2(__int128 amount) const { return _orig.estimate_fee_from_1(amount);};
void ReverseOrderBook::update() {}
std::string ReverseOrderBook::print() const {
  return "Reverse order book for " + _orig.print();
};

SymbolFactory& SymbolFactory::get_factory() {
  return _factory;
}
Symbol& SymbolFactory::get_symbol(std::string& symbol, std::string& name) {
  const auto it = _symbol_indices.find(symbol);
  if (it != _symbol_indices.end())
    return _symbols[it->second];

  size_t length = _symbols.size();
  _symbol_indices.insert({symbol, length});
  _symbols.push_back(Symbol(symbol, name));
  return _symbols[length];
}

Symbol& SymbolFactory::get_symbol(std::string& symbol) {
  const auto it = _symbol_indices.find(symbol);
  if (it != _symbol_indices.end())
    return _symbols[it->second];

  size_t length = _symbols.size();
  _symbol_indices.insert({symbol, length});
  _symbols.push_back(Symbol(symbol, symbol));
  return _symbols[length];
}

Symbol& SymbolFactory::get_symbol(size_t index) {
  return _symbols[index];
}
size_t SymbolFactory::count() {
  return _symbols.size();
}

std::vector<Symbol>& SymbolFactory::get_all_symbols() {
  return _symbols;
}

size_t SymbolFactory::get_symbol_index(const std::string& symbol) {
  const auto it = _symbol_indices.find(symbol);
  if (it != _symbol_indices.end())
    return it->second;
  throw std::runtime_error("Not found");
}