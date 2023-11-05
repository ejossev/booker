#include <string>
#include <vector>
#include <map>
#include <set>
#include "constants.hpp"
#include "Symbol.hpp"
#pragma once


class GenericOrderBook {
public:

  virtual const Symbol& get_symbol_1() const = 0;
  virtual const Symbol& get_symbol_2() const = 0;

  virtual __int128 estimate_conversion_from_1(__int128 amount) const = 0;
  virtual __int128 estimate_conversion_from_2(__int128 amount) const = 0;

  virtual __int128 estimate_fee_from_1(__int128 amount) const = 0;
  virtual __int128 estimate_fee_from_2(__int128 amount) const = 0;

  virtual void update() = 0;

  virtual std::string print() const = 0;
};

class NullOrderBook : public GenericOrderBook {
private:
  const Symbol& symbol1, &symbol2;

public:
  NullOrderBook(const Symbol& s1, const Symbol& s2);
  const Symbol& get_symbol_1() const override;
  const Symbol& get_symbol_2() const override;
  __int128 estimate_conversion_from_1(__int128 amount) const override;
  __int128 estimate_conversion_from_2(__int128 amount) const override;
  __int128 estimate_fee_from_1(__int128 amount) const override;
  __int128 estimate_fee_from_2(__int128 amount) const override;
  void update() override;
  std::string print() const override;
};

class ReverseOrderBook : public GenericOrderBook {
private:
  GenericOrderBook& _orig;
public:

  ReverseOrderBook(GenericOrderBook& orig);
  const Symbol& get_symbol_1() const override;
  const Symbol& get_symbol_2() const override;
  __int128 estimate_conversion_from_1(__int128 amount) const override;
  __int128 estimate_conversion_from_2(__int128 amount) const override;
  __int128 estimate_fee_from_1(__int128 amount) const override;
  __int128 estimate_fee_from_2(__int128 amount) const override;
  void update() override;
  std::string print() const override;
};

class GenericOrderBookCollection {
public:
  virtual const GenericOrderBook& get_order_book(const Symbol& symbol1, const Symbol& symbol2) = 0;
  virtual std::vector<std::reference_wrapper<const Symbol>> get_all_symbols() = 0;
  virtual std::map<std::reference_wrapper<const Symbol>, std::set<std::reference_wrapper<const Symbol>>> get_trading_pairs() = 0;
  virtual bool has_trading_pair(const Symbol& symbol1, const Symbol& symbol2) = 0;
};