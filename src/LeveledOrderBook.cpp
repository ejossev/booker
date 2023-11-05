
#include <algorithm>
#include <sstream>
#include "LeveledOrderBook.hpp"
#include "constants.hpp"
#include "Utils.hpp"

namespace {
static const int ob_depth = 10;
}

LeveledOrderBook::LeveledOrderBook(const Symbol& s1, const Symbol& s2) : symbol1(s1), symbol2(s2) {
}

LeveledOrderBook::LeveledOrderBook(LeveledOrderBook&& other) : symbol1(other.symbol1), symbol2(other.symbol2) {
  bids = std::move(other.bids);
  asks = std::move(other.asks);
}

std::string LeveledOrderBook::print() const {
  std::stringstream ss;
  ss << "Bid_size: "  << bids.size() << ", ask_size: " << asks.size() << "\n" << "  Bids: \n";
  for (const auto& b : bids) {
    ss << "    " << b.first << " : " << b.second << "\n";
  }
  ss << "  Asks: \n";
  for (const auto& a : asks) {
    ss << "    " << a.first << " : " << a.second << "\n";
  }
  return ss.str();
}

const Symbol& LeveledOrderBook::get_symbol_1() const { return symbol1; }
const Symbol& LeveledOrderBook::get_symbol_2() const { return symbol2; }

__int128 LeveledOrderBook::estimate_conversion_from_1(__int128 amount) const {

  // XBTC/USD 
  // symbol1=XBTC, symbol2=USD
  // asks - 1682200000000
  // bids - 1681800000000
  // zamenit BTX na USD znamena pouzit bids

  const std::lock_guard<std::mutex> lock(update_mutex);
  __int128 volume_consumed = 0;
  __int128 received = 0;
  auto level_it = bids.rbegin();
  while (volume_consumed < amount && level_it != bids.rend()) {
    __int128 price_at_level = level_it->first;
    __int128 volume_at_level = level_it->second;
    __int128 exchanging = std::min(volume_at_level, amount - volume_consumed);
    volume_consumed = volume_consumed + exchanging;
    received = received + exchanging * price_at_level / dec_power;
    level_it++;
  }

  return received;
}

// LeveledOrderBook& LeveledOrderBook::operator=(const LeveledOrderBook& other) {
//   if (this == &other) return *this;
//   symbol1 = other.symbol1;
//   symbol2 = other.symbol2;
//   bids = other.bids;
//   asks = other.asks;
//   return *this;
// }

__int128 LeveledOrderBook::estimate_conversion_from_2(__int128 amount) const {
  const std::lock_guard<std::mutex> lock(update_mutex);
  __int128 volume_consumed = 0;
  __int128 received = 0;
  auto level_it = asks.begin();
  while (volume_consumed < amount && level_it != asks.end()) {
    __int128 price_at_level = level_it->first;
    __int128 volume_at_level = level_it->second * price_at_level;
    __int128 exchanging = std::min(volume_at_level, amount - volume_consumed);
    volume_consumed = volume_consumed + exchanging;
    received = received + exchanging * dec_power / price_at_level;
    level_it++;
  }

  return received;
}

__int128 LeveledOrderBook::estimate_fee_from_1(__int128 amount) const {
  return amount * 24 / 10000;
}
__int128 LeveledOrderBook::estimate_fee_from_2(__int128 amount) const {
  return amount * 24 / 10000;
}

void LeveledOrderBook::update() {}

void LeveledOrderBook::updateAskLevel(__int128 price, __int128 volume) {
  const std::lock_guard<std::mutex> lock(update_mutex);
  if (volume == 0) {
    asks.erase(price);
    return;
  }

  asks[price] = volume;
  if (asks.size() > ob_depth) {
    asks.erase(std::prev(asks.end()));
  }
}
void LeveledOrderBook::updateBidLevel(__int128 price, __int128 volume) {
  const std::lock_guard<std::mutex> lock(update_mutex);
  if (volume == 0) {
    bids.erase(price);
    return;
  }

  bids[price] = volume;
  if (bids.size() > ob_depth) {
    bids.erase(bids.begin());
  }
}