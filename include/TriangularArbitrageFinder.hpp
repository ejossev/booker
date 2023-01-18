#include <vector>
#include <utility>

#include "OrderBook.hpp"
#include "constants.hpp"

#pragma once


struct optimal_rate {
  __int128 rate;
  int64_t path;
};
  
class TriangularArbitrageFinder {
private:
  GenericOrderBookCollection& _order_books;
  std::vector<Symbol> _symbols;
  std::map<Symbol, std::set<Symbol>> _trading_pairs;

  std::vector<std::vector<size_t>> cycles;
  std::vector<std::vector<GenericOrderBook*>> cycles2;
  
  // optimal_rate* _optimal_rates;
  size_t len;

  size_t symbol_index(const Symbol& s) const ;
  size_t symbol_index(const std::string& s) const;

  void depth_first_cycle_search(
    size_t start,
    size_t current, 
    std::set<size_t>& visited,
    std::vector<size_t>& path
  );

public:
  TriangularArbitrageFinder(GenericOrderBookCollection& order_books) : 
      _order_books(order_books), 
      _symbols(order_books.get_all_symbols()) {
        len = _symbols.size();
        _trading_pairs = order_books.get_trading_pairs();
        // _optimal_rates = new optimal_rate[len*len];
        find_cycles();
      }

  std::vector<std::vector<std::pair<__int128, Symbol>>> calculate_optimal_rates(__int128);
  // optimal_rate& get_rate(size_t i, size_t j) const;
  // std::vector<uint64_t> restore_path(uint64_t u, uint64_t v) const;

  void find_cycles() ;
  // std::vector<std::tuple<Symbol, Symbol, Symbol>> get_arbitrages(uint64_t) const;
};