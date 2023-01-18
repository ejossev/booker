#include <thread>
#include <vector>
#include <iostream>

#include "Kraken.hpp"
#include "TriangularArbitrageFinder.hpp"
#include "Config.hpp"

void test_ob(KrakenExchange* kraken) {
  do{
    std::string s1 = std::string("USD");
    std::string s2 = std::string("XBT");
    Symbol usd(s1, s1);
    Symbol btc(s2, s2);
    const auto& ob = kraken->get_order_book(btc, usd);

    std::cout << "OB: " << ob.print() << std::endl;
    std::cout << "1 BTC is about  " << ob.estimate_conversion_from_1(1) << " USD\n";  

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  } while(true);
}

void try_find_arbitrage(KrakenExchange* kraken) {
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  std::vector<Symbol> symbols = kraken->get_all_symbols();
  TriangularArbitrageFinder finder(*kraken);
  do {
    std::cout << "BP1 \n";
    const auto& arbitrages = finder.calculate_optimal_rates(100 * dec_power);
    for (const auto& arbitrage_path : arbitrages) {
      std::cout << "Arbitrage found: ";
      for (const auto& [exchanged, symbol] : arbitrage_path) {
        std::cout << exchanged << symbol.get_symbol() << " -> ";
      }
      std::cout << std::endl;
    }
    std::cout << "BP2 \n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  } while(true);
}
  
//static pConf(new Poco::Util::IniFileConfiguration("test.ini"));

int main() {
  KrakenExchange kraken;

  std::cout << pConf->getInt("Kraken.OBDepth") << std::endl;

  kraken.start_connection_async();
  std::thread test(try_find_arbitrage, &kraken);
  test.join();
}