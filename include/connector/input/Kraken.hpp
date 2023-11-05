#include <map>
#include <Poco/JSON/Object.h>
#include "LeveledOrderBook.hpp"
#include "OrderBook.hpp"
#include "Utils.hpp"

static const char *const https_host = "api.kraken.com";
static const char *const ws_host = "ws.kraken.com";
static const char *const http_buy_endpoint = "/0/private/AddOrder";
static const char *const ws_endpoint = "/ws";

#pragma once

class KrakenExchange : public GenericOrderBookCollection {
  std::set<std::reference_wrapper<const Symbol>> all_symbols;
  std::map<std::string, LeveledOrderBook> trading_pairs;
  std::map<std::string, ReverseOrderBook> reverse_order_books;
  std::map<std::pair<std::string, std::string>, std::string> trading_pair_resolver;
  static NullOrderBook null_book;

  void process_ws();
  void fetch_trading_pairs();
  uint64_t try_fetch_reference_rate(const std::string& pair_name);
  Poco::Logger& logger;

  std::string APIKey;
  std::string PrivateKey;

  Poco::JSON::Object::Ptr send_authenticated_post_request(const std::string& url, std::string content);
public:
  KrakenExchange();
  bool initialized{};
  void start_connection_async();
  virtual const GenericOrderBook& get_order_book(const Symbol& symbol1, const Symbol& symbol2) override;
  virtual std::vector<std::reference_wrapper<const Symbol>> get_all_symbols() override;
  virtual std::map<std::reference_wrapper<const Symbol>, std::set<std::reference_wrapper<const Symbol>>> get_trading_pairs() override;
  virtual bool has_trading_pair(const Symbol& symbol1, const Symbol& symbol2) override;
  bool send_trade_sync(const Symbol& symbol1, const Symbol& symbol2, const uint64_t amount);
};