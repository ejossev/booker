//
// Created by Josef Sevcik on 05/11/2023.
//

#pragma once

#include <iostream>
#include <map>

class Symbol {
private:
  std::string _name;
  std::string _symbol;
  std::string _exchange;
  mutable __int128 _reference_rate_estimate;
protected:
  Symbol(const std::string& symbol, const std::string& name, const std::string& exchange);
  Symbol(const std::string& symbol, const std::string& exchange);
public:
  Symbol(Symbol&& other);
  Symbol(const Symbol& other) = delete;

  Symbol& operator=(const Symbol& other) = delete;
  const std::string& get_symbol() const;
  const std::string& get_name() const;
  const std::string& get_exchange() const;
  __int128 get_reference_rate_estimate() const;
  void set_reference_rate_estimate(__int128 reference_rate_estimate) const;
  friend bool operator<(const Symbol& first, const Symbol& second);
  friend bool operator==(const Symbol& first, const Symbol& second);

  friend class SymbolFactory;
};

class SymbolFactory {
private:
  SymbolFactory(const SymbolFactory&) = delete;
  void operator=(const SymbolFactory&) = delete;
  SymbolFactory() { _symbols.reserve(1000); };
  static SymbolFactory _factory;
  std::vector<Symbol> _symbols;
  std::map<std::pair<std::string, std::string>, size_t> _symbol_indices;

  const Symbol& add_symbol(const std::string& symbol, const std::string& name, const std::string& exchange);
  const Symbol& add_symbol(const std::string& symbol, const std::string& exchange);
public:
  static SymbolFactory& get_factory();
  const Symbol& get_symbol(const std::string& symbol, const std::string& name, const std::string& exchange);
  const Symbol& get_symbol(const std::string& symbol, const std::string& exchange);

  size_t count();
  std::vector<Symbol>& get_all_symbols();
};


