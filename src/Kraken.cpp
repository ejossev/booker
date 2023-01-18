#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>

#include <iostream>
#include <set>
#include <map>
#include <numeric>
#include <thread>
#include <chrono>

#include "LeveledOrderBook.hpp"
#include "Kraken.hpp"
#include "Config.hpp"

namespace {
static const std::set<std::string> ignore_assets {"ETH2.S"};
static const std::map<std::string, std::string> rewrite_assets {
  {"XETC", "ETC"}, {"XETH", "ETH"}, {"XMLN", "MLN"}, {"XLTC", "LTC"}, 
  {"XREP", "REP"}, {"XXBT", "XBT"}, {"XXDG", "XDG"}, {"XXLM", "XLM"},
  {"XXMR", "XMR"}, {"XXRP", "XRP"}, {"XZEC", "ZEC"}, {"ZAUD", "AUD"},
  {"ZCAD", "CAD"}, {"ZEUR", "EUR"}, {"ZGBP", "GBP"}, {"ZJPY", "JPY"},
  {"ZUSD", "USD"}
};
static const std::string base_asset("USD");

const std::string& rewrite_symbol(const std::string& orig) {
  const auto& it = rewrite_assets.find(orig);
  if (it != rewrite_assets.end())
    return it->second;
  return orig;
}
} //namespace

NullOrderBook KrakenExchange::null_book;

std::vector<Symbol> KrakenExchange::get_all_symbols() {
  std::vector<Symbol> rv;
  for (uint64_t i : all_symbols) {
    rv.push_back(SymbolFactory::get_factory().get_symbol(i));
  }
  return rv;
}

std::map<Symbol, std::set<Symbol>> KrakenExchange::get_trading_pairs() {
  std::map<Symbol, std::set<Symbol>> rv;
  for (auto& t : trading_pairs) {
    auto& ob = t.second;
    Symbol s1 = ob.get_symbol_1();
    Symbol s2 = ob.get_symbol_2();
    rv[s1].insert(s2);
    rv[s2].insert(s1);
  }
  return rv;
}

bool KrakenExchange::has_trading_pair(const Symbol& symbol1, const Symbol& symbol2) {
  return (trading_pair_resolver.count({symbol1.get_symbol(), symbol2.get_symbol()}) > 0);
}



void KrakenExchange::start_connection_async() {
  std::thread init(&KrakenExchange::fetch_trading_pairs, this);
  init.join();
  std::thread go(&KrakenExchange::process_ws, this);
  go.detach();
}

GenericOrderBook& KrakenExchange::get_order_book(Symbol& symbol1, Symbol& symbol2) {
  if (trading_pair_resolver.count({symbol1.get_symbol(), symbol2.get_symbol()}) > 0)
    return trading_pairs.find(trading_pair_resolver[{symbol1.get_symbol(), symbol2.get_symbol()}])->second;
  if (trading_pair_resolver.count({symbol2.get_symbol(), symbol1.get_symbol()}) > 0)
    return reverse_order_books.find(trading_pair_resolver[{symbol2.get_symbol(), symbol1.get_symbol()}])->second;
  return null_book;
};

const GenericOrderBook& KrakenExchange::get_order_book(const Symbol& symbol1, const Symbol& symbol2) {
  if (trading_pair_resolver.count({symbol1.get_symbol(), symbol2.get_symbol()}) > 0) {
    const std::string pair_name = trading_pair_resolver[{symbol1.get_symbol(), symbol2.get_symbol()}];
    return trading_pairs.find(pair_name)->second;
  }
    
  if (trading_pair_resolver.count({symbol2.get_symbol(), symbol1.get_symbol()}) > 0) {
    const std::string pair_name = trading_pair_resolver[{symbol2.get_symbol(), symbol1.get_symbol()}];
    return reverse_order_books.find(pair_name)->second;
  }
  return null_book;
}


void KrakenExchange::process_ws()
{
  // Set up HTTP client and request
  Poco::Net::HTTPSClientSession session("ws.kraken.com");
  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/ws");
  Poco::Net::HTTPResponse response;

  // Set up WebSocket and connect
  Poco::Net::WebSocket ws(session, request, response);
  std::cout << "Connected to Kraken WebSockets API" << std::endl;


  std::string pairs = "";
  pairs = std::accumulate(
    std::next(trading_pairs.begin()),
    trading_pairs.end(),
    "\"" + trading_pairs.begin()->first + "\"",
    [](std::string a, const auto& b) -> std::string {
        return a + ", \"" + b.first + "\"";
    }
  );

  std::string subscribeMessage = "{ \"event\": \"subscribe\", \"pair\": [" 
          + pairs + "], \"subscription\": { \"name\": \"book\", \"depth\": "
          + std::to_string(pConf->getInt("Kraken.OBDepth")) + "} }";
  ws.sendFrame(subscribeMessage.data(), subscribeMessage.size());
  //std::cout << subscribeMessage << std::endl;
  std::cout << "Subscribed to trade channel for " << pairs << " pair" << std::endl;

  // Receive and process messages from the WebSocket
  char buffer[8192];
  int flags;
  int n;
  do
  {
      n = ws.receiveFrame(buffer, sizeof(buffer), flags);
      if (n > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_TEXT)
      {
          buffer[n] = 0;
          // Parse message as JSON
          try {
            Poco::JSON::Parser parser;
            Poco::Dynamic::Var result = parser.parse(buffer);

            if (result.isArray()) {
              Poco::JSON::Array::Ptr arr = result.extract<Poco::JSON::Array::Ptr>();
              size_t count = arr->size();
              std::string pair = arr->getElement<std::string>(count-1);
              LeveledOrderBook& ob = trading_pairs.find(pair)->second;

              // if (pair == "XBT/USD")
              //   std::cout << "got update " << buffer << std::endl;

              for (int i=1; i<count-2; i++) {
                Poco::JSON::Object::Ptr changeObject = arr->getObject(i);
                for (const std::string& el : {"a", "as"}) {
                  if (changeObject->has(el)) {
                    Poco::JSON::Array::Ptr askLevelsArr = changeObject->getArray(el);
                    for (int i=0; i<askLevelsArr->size(); i++) {
                      Poco::JSON::Array::Ptr askLevelArr = askLevelsArr->getArray(i);
                      std::string level = askLevelArr->getElement<std::string>(0);
                      std::string volume = askLevelArr->getElement<std::string>(1);

                      // get OB, fill level
                      ob.updateAskLevel(
                        uint64_t(stod(level) * dec_power), 
                        uint64_t(stod(volume) * dec_power)
                      );
                    }
                  }
                }

                for (const std::string& el : {"b", "bs"}) {
                  if (changeObject->has(el)) {
                    Poco::JSON::Array::Ptr bidLevelsArr = changeObject->getArray(el);
                    for (int i=0; i<bidLevelsArr->size(); i++) {
                      Poco::JSON::Array::Ptr bidLevelArr = bidLevelsArr->getArray(i);
                      std::string level = bidLevelArr->getElement<std::string>(0);
                      std::string volume = bidLevelArr->getElement<std::string>(1);
                      // get OB, fill level
                      ob.updateBidLevel(
                        uint64_t(stod(level) * dec_power), 
                        uint64_t(stod(volume) * dec_power)
                      );
                    }
                  }
                }
              }
            } else {
              Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();
              // Print message if it is a trade update
              if (object->has("event"))
              {
                // Ignore these messages for now, maybe use the confirmations later
                //std::cout << "Received event update: " << buffer << std::endl;
              } else {
                std::cout << "Unknown message: " << buffer << std::endl;
              }
            }
          } catch (Poco::Exception& e) {
            std::cout << "Cannot handle this frame " << buffer << " - error " << e.displayText() << std::endl;
          }
      }
  } while (n > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);

  // Close WebSocket
  ws.close();
  std::cout << "Disconnected from Kraken WebSockets API" << std::endl;

}


uint64_t KrakenExchange::try_fetch_reference_rate(const std::string& pair_name) {
  std::cout << "Trying to get ticker for pair " << pair_name << " - ";
  Poco::Net::HTTPSClientSession session("api.kraken.com");
  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/0/public/Ticker?pair=" + pair_name);
  session.sendRequest(request);
  Poco::Net::HTTPResponse response;
  std::istream& responseStream = session.receiveResponse(response);

  Poco::JSON::Parser parser;
  Poco::Dynamic::Var result = parser.parse(responseStream);
  Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

  if (object->has("result")) {

    Poco::JSON::Object::Ptr resultObject = object->get("result").extract<Poco::JSON::Object::Ptr>();
    Poco::DynamicStruct ds = *resultObject;
    std::string svalue = (ds.begin()->second)["p"][0];
    std::cout << " found price of " << svalue << "(";
    double value = std::stod(svalue);
    long rv = ((double)dec_power * value);
    std::cout << rv << ")\n";
    return rv;
  }

  std::cout << " ticker not found \n";
  return 0;
}

void KrakenExchange::fetch_trading_pairs() {
  SymbolFactory& symbol_factory = SymbolFactory::get_factory();

  Poco::Net::HTTPSClientSession session("api.kraken.com");
  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/0/public/AssetPairs");
  session.sendRequest(request);
  Poco::Net::HTTPResponse response;
  std::istream& responseStream = session.receiveResponse(response);

  Poco::JSON::Parser parser;
  Poco::Dynamic::Var result = parser.parse(responseStream);
  Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

  Poco::JSON::Object::Ptr resultObject = object->get("result").extract<Poco::JSON::Object::Ptr>();

  for (Poco::JSON::Object::ConstIterator it = resultObject->begin(); it != resultObject->end(); ++it) {
    std::cout << "Adding new trade pair - ";
    std::string name = it->first;
    auto ds = it->second;
    std::cout << name << "\n";

    Poco::JSON::Object::Ptr asset_pair_object = it->second.extract<Poco::JSON::Object::Ptr>();
    std::string s1 = asset_pair_object->getValue<std::string>("base");
    std::string s2 = asset_pair_object->getValue<std::string>("quote");
    std::string wsname = asset_pair_object->getValue<std::string>("wsname");

    if (ignore_assets.count(s1) > 0 || ignore_assets.count(s2) > 0)
      continue;
    
    s1 = rewrite_symbol(s1);
    s2 = rewrite_symbol(s2);

    Symbol& symbol1 = symbol_factory.get_symbol(s1, s1);
    Symbol& symbol2 = symbol_factory.get_symbol(s2, s2);
    all_symbols.insert(symbol_factory.get_symbol_index(s1));
    all_symbols.insert(symbol_factory.get_symbol_index(s2));

    LeveledOrderBook ob(symbol1, symbol2);
    trading_pair_resolver.insert(std::make_pair(std::make_pair(s1, s2), wsname));

    auto ob_it = (trading_pairs.insert(std::make_pair(wsname, std::move(ob)))).first;
    ReverseOrderBook rev_ob(ob_it->second);
    reverse_order_books.insert(std::make_pair(wsname, std::move(rev_ob)));
  }


  for (uint64_t s_index : all_symbols) {
    Symbol& s = symbol_factory.get_symbol(s_index);
    if (s.get_symbol() == base_asset) {
      s.set_reference_rate_estimate(dec_power);
      continue;
    }

    uint64_t v1 = try_fetch_reference_rate(s.get_symbol()+"USD");
    uint64_t v2 = v1 > 0 ? 0 : try_fetch_reference_rate("USD"+s.get_symbol());
    uint64_t v = v1 == 0 ? v2 : dec_power * dec_power / v1;
    //std::max(v2, v1 == 0 ? 0 : dec_power * dec_power / v1);
    s.set_reference_rate_estimate(v);
    std::cout << "1 USD = " << s.get_reference_rate_estimate() << " " << s.get_symbol() << std::endl;
  }

}
