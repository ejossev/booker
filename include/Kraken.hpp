#include <map>
#include "LeveledOrderBook.hpp"
#include "OrderBook.hpp"
#pragma once

class KrakenExchange : public GenericOrderBookCollection {
  std::set<uint64_t> all_symbols;
  std::map<std::string, LeveledOrderBook> trading_pairs;
  std::map<std::string, ReverseOrderBook> reverse_order_books;
  std::map<std::pair<std::string, std::string>, std::string> trading_pair_resolver;
  static NullOrderBook null_book;

  void process_ws();
  void fetch_trading_pairs();
  uint64_t try_fetch_reference_rate(const std::string& pair_name);
public:
  bool initialized;

  void start_connection_async();
  virtual GenericOrderBook& get_order_book(Symbol& symbol1, Symbol& symbol2) override;
  virtual const GenericOrderBook& get_order_book(const Symbol& symbol1, const Symbol& symbol2) override;
  virtual std::vector<Symbol> get_all_symbols() override;
  virtual std::map<Symbol, std::set<Symbol>> get_trading_pairs() override;
  virtual bool has_trading_pair(const Symbol& symbol1, const Symbol& symbol2) override;
};