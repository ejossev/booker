#include <string>
#include <vector>
#include <map>
#include <set>
#include "constants.hpp"
#pragma once



class Symbol {
private:
  std::string _name;
  std::string _symbol;
  mutable __int128 _reference_rate_estimate;
public:
  Symbol(std::string& symbol, std::string& name);
  Symbol(Symbol&& other);
  Symbol(const Symbol& other);

  Symbol& operator=(const Symbol& other);
  std::string get_symbol() const;
  std::string get_name() const;
  __int128 get_reference_rate_estimate() const;
  void set_reference_rate_estimate(__int128 reference_rate_estimate) const;
  friend bool operator<(const Symbol& first, const Symbol& second);
  friend bool operator==(const Symbol& first, const Symbol& second);
};

class SymbolFactory {
private:
  SymbolFactory(const SymbolFactory&) = delete;
  void operator=(const SymbolFactory&) = delete;
  SymbolFactory() = default;
  static SymbolFactory _factory;
  std::vector<Symbol> _symbols;
  std::map<std::string, size_t> _symbol_indices;
public:
  static SymbolFactory& get_factory();
  Symbol& get_symbol(std::string& symbol, std::string& name);
  Symbol& get_symbol(std::string& symbol);
  Symbol& get_symbol(size_t index);
  size_t count();
  std::vector<Symbol>& get_all_symbols();
  size_t get_symbol_index(const std::string& symbol);
};

class GenericOrderBook {
public:

  virtual Symbol& get_symbol_1() const = 0;
  virtual Symbol& get_symbol_2() const = 0;

  virtual __int128 estimate_conversion_from_1(__int128 amount) const = 0;
  virtual __int128 estimate_conversion_from_2(__int128 amount) const = 0;

  virtual __int128 estimate_fee_from_1(__int128 amount) const = 0;
  virtual __int128 estimate_fee_from_2(__int128 amount) const = 0;

  virtual void update() = 0;

  virtual std::string print() const = 0;
};

class NullOrderBook : public GenericOrderBook {
private:
  size_t symbol1, symbol2;

public:
  NullOrderBook();
  NullOrderBook(Symbol& s1, Symbol& s2);
  Symbol& get_symbol_1() const override;
  Symbol& get_symbol_2() const override;
  __int128 estimate_conversion_from_1(__int128 amount) const override;
  __int128 estimate_conversion_from_2(__int128 amount) const override;
  __int128 estimate_fee_from_1(__int128 amount) const override;
  __int128 estimate_fee_from_2(__int128 amount) const override;
  void update() override;
  std::string print() const override;
};

class ReverseOrderBook : public GenericOrderBook {
private:
  static NullOrderBook _nb;
  GenericOrderBook& _orig;
public:

  ReverseOrderBook();
  ReverseOrderBook(GenericOrderBook& orig);
  Symbol& get_symbol_1() const override;
  Symbol& get_symbol_2() const override;
  __int128 estimate_conversion_from_1(__int128 amount) const override;
  __int128 estimate_conversion_from_2(__int128 amount) const override;
  __int128 estimate_fee_from_1(__int128 amount) const override;
  __int128 estimate_fee_from_2(__int128 amount) const override;
  void update() override;
  std::string print() const override;
};

class GenericOrderBookCollection {
public:
  virtual GenericOrderBook& get_order_book(Symbol& symbol1, Symbol& symbol2) = 0;
  virtual const GenericOrderBook& get_order_book(const Symbol& symbol1, const Symbol& symbol2) = 0;
  virtual std::vector<Symbol> get_all_symbols() = 0;
  virtual std::map<Symbol, std::set<Symbol>> get_trading_pairs() = 0;
  virtual bool has_trading_pair(const Symbol& symbol1, const Symbol& symbol2) = 0;
};