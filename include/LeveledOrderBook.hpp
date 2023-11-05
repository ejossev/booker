#include "OrderBook.hpp"
#include <map>
#include <mutex>
#pragma once

class KrakenExchange;

class LeveledOrderBook : public GenericOrderBook {
protected:
  std::map<__int128, __int128> bids;
  std::map<__int128, __int128> asks;
  const Symbol& symbol1, &symbol2;
  mutable std::mutex update_mutex;
public:
  LeveledOrderBook(const Symbol& s1, const Symbol& s2);
  LeveledOrderBook(LeveledOrderBook&& other);
  LeveledOrderBook(const LeveledOrderBook&) = delete;
  //LeveledOrderBook& operator=(const LeveledOrderBook& other);

  const Symbol& get_symbol_1() const override;
  const Symbol& get_symbol_2() const override;

  __int128 estimate_conversion_from_1(__int128 amount) const override;
  __int128 estimate_conversion_from_2(__int128 amount) const override;

  __int128 estimate_fee_from_1(__int128 amount) const override;
  __int128 estimate_fee_from_2(__int128 amount) const override;

  

  void update() override;

  std::string print() const override;
protected:
  void updateAskLevel(__int128 price, __int128 volume);
  void updateBidLevel(__int128 price, __int128 volume);
  friend class KrakenExchange;
};

