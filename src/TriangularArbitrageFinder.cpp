#include <iostream>
#include <set>
#include <vector>
#include <limits>
#include <exception>
#include <utility>
#include "TriangularArbitrageFinder.hpp"





void TriangularArbitrageFinder::calculate_optimal_rates(
    std::function<void(std::vector<std::pair<__int128, GenericOrderBook*>>)> callback, 
    __int128 start_value_usd = dec_power) 
{
  std::vector<std::vector<std::pair<__int128, Symbol>>> rv;
  rv.reserve(20);

  for (const auto& ob_path : cycles2) {
    std::vector<std::pair<__int128, GenericOrderBook*>> extended_path;
    extended_path.reserve(6);

    __int128 reference_amount = ob_path[0]->get_symbol_1().get_reference_rate_estimate() * start_value_usd / dec_power;
    __int128 exchanged_amount = reference_amount;

    for (GenericOrderBook* ob : ob_path) {
      extended_path.push_back({exchanged_amount, ob});
      exchanged_amount = ob->estimate_conversion_from_1(exchanged_amount);
      exchanged_amount = exchanged_amount - ob->estimate_fee_from_2(exchanged_amount);
    }
    if (exchanged_amount > reference_amount) {
      callback(extended_path);
    }
  }
}


void TriangularArbitrageFinder::depth_first_cycle_search(
    size_t start,
    size_t current, 
    std::set<size_t>& visited,
    std::vector<size_t>& path
  ) {
  // Visit every connected node
  // if the node is already in the visited list, return (error)
  // if the node is the start node, yupee, store the path so far and return 
  // otherwise, add node to the visited and path, and call all connected nodes
  // invariants: the path and visited contains all nodes including the current one; 
  //    upon returning, it is the task of the called method to clean the "new current" before returning
  if (current == start && path.size() > 2) {
    path.push_back(current);

    std::vector<GenericOrderBook*> ob_path;
    Symbol first = _symbols[path[0]];
    for (size_t i=1; i<path.size(); i++) {
      Symbol second = _symbols[path[i]];
      GenericOrderBook& ob = _order_books.get_order_book(first, second);
      ob_path.push_back(&ob);
      first = second;
    }

    cycles.push_back(std::vector<size_t>(path));
    cycles2.push_back(std::move(ob_path));

    path.pop_back();
    return;
  }
  if (visited.count(current) > 0 || path.size() > 2) {
    return;
  }
  path.push_back(current);
  visited.insert(current);
  const auto traded_symbols = _trading_pairs[_symbols[current]];
  for (const Symbol s : traded_symbols) {
    depth_first_cycle_search(start, symbol_index(s), visited, path);
  }
  path.pop_back();
  visited.erase(current);
}

void TriangularArbitrageFinder::find_cycles() {
  std::set<size_t> visited;
  std::vector<size_t> path;
  
  size_t len = _symbols.size();
  for (size_t i=0; i<len; i++) {
    std::set<size_t> visited;
    std::vector<size_t> path;
    visited.insert(i);
    path.push_back(i);
    const auto traded_symbols = _trading_pairs[_symbols[i]];
    for (const Symbol& s : traded_symbols) {
      depth_first_cycle_search(i, symbol_index(s), visited, path);
    }
  }
}

size_t TriangularArbitrageFinder::symbol_index(const Symbol& s) const {
  return symbol_index(s.get_symbol());
}

size_t TriangularArbitrageFinder::symbol_index(const std::string& s) const {
  for (int i=0; i<_symbols.size(); i++) {
    if (_symbols[i].get_symbol() == s)
      return i;
  }
  throw std::range_error("Not found");
}