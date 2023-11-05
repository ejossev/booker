#include <sstream>
#include "OrderBook.hpp"
#include <iostream>

namespace {
  static std::string null_name("NULL");
}

SymbolFactory SymbolFactory::_factory;


NullOrderBook::NullOrderBook(const Symbol& s1, const Symbol& s2) : symbol1(s1), symbol2(s2) {}


const Symbol& NullOrderBook::get_symbol_1() const {
  return symbol1;
}

const Symbol& NullOrderBook::get_symbol_2() const {
  return symbol2;
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


Symbol::Symbol(const std::string& symbol, const std::string& name, const std::string& exchange) :
    _name(name), _symbol(symbol), _exchange(exchange) {}
Symbol::Symbol(const std::string& symbol, const std::string& exchange) :
    _name(), _symbol(symbol), _exchange(exchange) {}
Symbol::Symbol(Symbol&& other) {
  _name = std::move(other._name);
  _symbol = std::move(other._symbol);
  _exchange = std::move(other._exchange);
  _reference_rate_estimate = other._reference_rate_estimate;
}

const std::string& Symbol::get_symbol() const { return _symbol; }
const std::string& Symbol::get_name() const { return _name; }
const std::string& Symbol::get_exchange() const { return _exchange; }
__int128 Symbol::get_reference_rate_estimate() const { return _reference_rate_estimate; }
void Symbol::set_reference_rate_estimate(__int128 reference_rate_estimate) const {
  _reference_rate_estimate = reference_rate_estimate;
}

ReverseOrderBook::ReverseOrderBook(GenericOrderBook& orig) : _orig(orig) {};
const Symbol& ReverseOrderBook::get_symbol_1() const { return _orig.get_symbol_2();};
const Symbol& ReverseOrderBook::get_symbol_2() const { return _orig.get_symbol_1();};
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

const Symbol& SymbolFactory::add_symbol(const std::string& symbol, const std::string& name, const std::string& exchange) {
  size_t length = _symbols.size();
  _symbol_indices.insert({std::make_pair(symbol, exchange), length});
  _symbols.push_back(Symbol(symbol, name, exchange));

  return _symbols[length];
}

const Symbol& SymbolFactory::add_symbol(const std::string& symbol, const std::string& exchange) {
  size_t length = _symbols.size();
  _symbol_indices.insert({std::make_pair(symbol, exchange), length});
  _symbols.push_back(Symbol(symbol, exchange));

  return _symbols[length];
}

const Symbol& SymbolFactory::get_symbol(const std::string& symbol, const std::string& name, const std::string& exchange) {
  const auto it = _symbol_indices.find(std::make_pair(symbol, exchange));
  if (it != _symbol_indices.end())
    return _symbols[it->second];

  return add_symbol(symbol, name, exchange);
}

const Symbol& SymbolFactory::get_symbol(const std::string& symbol, const std::string& exchange) {
  const auto it = _symbol_indices.find(std::make_pair(symbol, exchange));
  if (it != _symbol_indices.end())
    return _symbols[it->second];

  return add_symbol(symbol, exchange);
}

size_t SymbolFactory::count() {
  return _symbols.size();
}

std::vector<Symbol>& SymbolFactory::get_all_symbols() {
  return _symbols;
}
