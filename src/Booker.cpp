#include <thread>
#include <vector>
#include <iostream>
#include <Poco/Logger.h>
#include <Poco/FileChannel.h>
#include <Poco/AutoPtr.h>
#include <Poco/AsyncChannel.h>
#include <Poco/FormattingChannel.h>
#include <Poco/PatternFormatter.h>

#include "Kraken.hpp"
#include "TriangularArbitrageFinder.hpp"
#include "Utils.hpp"


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
    std::function<void(std::vector<std::pair<__int128, GenericOrderBook*>>)> callback = 
        [](std::vector<std::pair<__int128, GenericOrderBook*>> path) {
          std::cout << "Arbitrage found: ";
          for (const auto [exchanged, ob] : path) {
            std::cout << exchanged << ob->get_symbol_1().get_symbol() << " -> ";
          }
          std::cout << path.back().second->estimate_conversion_from_1(path.back().first)
                    << path.back().second->get_symbol_2().get_symbol() << std::endl;
        };
    

    finder.calculate_optimal_rates(callback, 100 * dec_power);
   
    std::cout << "BP2 \n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  } while(true);
}
  
Poco::AutoPtr<Poco::Util::IniFileConfiguration> config(new Poco::Util::IniFileConfiguration("./Booker.ini"));

int main() {
  Poco::AutoPtr<Poco::FileChannel> p_filechannel(new Poco::FileChannel);
  Poco::AutoPtr<Poco::PatternFormatter> p_pf(new Poco::PatternFormatter);
  p_pf->setProperty("pattern", "%Y%m%d-%H:%M:%S %s: %t");
  p_filechannel->setProperty("path", "Booker.log");
  p_filechannel->setProperty("rotation", "2 M");
  p_filechannel->setProperty("archive", "timestamp");

  Poco::AutoPtr<Poco::FormattingChannel> p_formatting_filechannel(new Poco::FormattingChannel(p_pf, p_filechannel));
  Poco::AutoPtr<Poco::AsyncChannel> p_channel(new Poco::AsyncChannel(p_formatting_filechannel));


  Poco::Logger::root().setChannel(p_channel);

  Poco::Logger::root().setLevel(config->getString("Booker.LogLevel"));
  Poco::Logger& logger2 = Poco::Logger::root().get("Booker");

  poco_notice(logger2, "Starting application");

  KrakenExchange kraken;

  kraken.start_connection_async();
  std::thread test(try_find_arbitrage, &kraken);
  test.join();
}