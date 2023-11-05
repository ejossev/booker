#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include "Poco/JSON/Stringifier.h"
#include <Poco/Net/SSLException.h>
#include <Poco/HMACEngine.h>
#include <Poco/SHA2Engine.h>
#include <Poco/Base64Encoder.h>

#include <iostream>
#include <set>
#include <map>
#include <numeric>
#include <thread>
#include <chrono>

#include "LeveledOrderBook.hpp"
#include "connector/input/Kraken.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"

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
static std::string exchange_string("kraken");

std::string sign_message(const std::string& message, const std::string& secret) {
  Poco::HMACEngine<Poco::SHA2Engine512> hmac(secret);
  hmac.update(message);
  const Poco::DigestEngine::Digest& digest = hmac.digest();
  std::stringstream ss;
  Poco::Base64Encoder encoder(ss);
  encoder.write(reinterpret_cast<const char *>(digest.data()), digest.size());
  encoder.close();
  return ss.str();
}

std::string sha256(const std::string& message) {
  Poco::SHA2Engine256 sha;
  sha.update(message);
  const Poco::DigestEngine::Digest& digest = sha.digest();
  std::stringstream ss;
  Poco::Base64Encoder encoder(ss);
  encoder.write(reinterpret_cast<const char *>(digest.data()), digest.size());
  encoder.close();
  return ss.str();
}

const std::string& rewrite_symbol(const std::string& orig) {
  const auto& it = rewrite_assets.find(orig);
  if (it != rewrite_assets.end())
    return it->second;
  return orig;
}
} //namespace


KrakenExchange::KrakenExchange() : logger(Poco::Logger::root().get("Kraken")) {
 APIKey = config->getString("Kraken.APIKey");
 PrivateKey = config->getString("Kraken.PrivateKey");
};

std::vector<std::reference_wrapper<const Symbol>> KrakenExchange::get_all_symbols() {
  std::vector<std::reference_wrapper<const Symbol>> rv;
  for (const Symbol& s : all_symbols) {
    rv.push_back(s);
  }
  return rv;
}

std::map<std::reference_wrapper<const Symbol>, std::set<std::reference_wrapper<const Symbol>>> KrakenExchange::get_trading_pairs() {
  std::map<std::reference_wrapper<const Symbol>, std::set<std::reference_wrapper<const Symbol>>> rv;
  for (auto& t : trading_pairs) {
    auto& ob = t.second;
    const Symbol& s1 = ob.get_symbol_1();
    const Symbol& s2 = ob.get_symbol_2();
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

const GenericOrderBook& KrakenExchange::get_order_book(const Symbol& symbol1, const Symbol& symbol2) {
  if (trading_pair_resolver.count({symbol1.get_symbol(), symbol2.get_symbol()}) > 0) {
    const std::string pair_name = trading_pair_resolver[{symbol1.get_symbol(), symbol2.get_symbol()}];
    return trading_pairs.find(pair_name)->second;
  }
    
  if (trading_pair_resolver.count({symbol2.get_symbol(), symbol1.get_symbol()}) > 0) {
    const std::string pair_name = trading_pair_resolver[{symbol2.get_symbol(), symbol1.get_symbol()}];
    return reverse_order_books.find(pair_name)->second;
  }
  throw new not_found_exception("unknown ob");
}

Poco::JSON::Object::Ptr KrakenExchange::send_authenticated_post_request(const std::string& url, std::string content) {
  Poco::Net::HTTPSClientSession session(https_host);
  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, url);
  request.setContentType("application/x-www-form-urlencoded");
  request.set("API-Key", APIKey);
  std::string nonce = std::to_string(time(nullptr));
  content = "nonce=" + nonce + "&" + content;
  request.set("API-Sign", sign_message(url + sha256(nonce + content), PrivateKey));
  session.sendRequest(request) << content; //pair=XBTUSD&type=buy&ordertype=market&volume=1
  Poco::Net::HTTPResponse response;
  std::istream &rs = session.receiveResponse(response);

  if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
    std::string response_content;
    Poco::StreamCopier::copyToString(rs, response_content);
    throw order_failed_exception(response.getStatus(), response_content);
  }
  Poco::JSON::Parser parser;
  Poco::Dynamic::Var result = parser.parse(rs);
  Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

  return object->get("result").extract<Poco::JSON::Object::Ptr>();
}



void KrakenExchange::process_ws() {
  while (true) try {
  // Set up HTTP client and request
  Poco::Net::HTTPSClientSession session(ws_host);
  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, ws_endpoint);
  Poco::Net::HTTPResponse response;

  // Set up WebSocket and connect
  Poco::Net::WebSocket ws(session, request, response);
  poco_notice(logger, "Connected to Kraken WebSockets API");


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
          + std::to_string(config->getInt("Kraken.OBDepth")) + "} }";
  ws.sendFrame(subscribeMessage.data(), subscribeMessage.size());
  poco_notice(logger, "Subscribed to trade channel for " + pairs + " pairs");

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
                poco_warning(logger, "Unknown message: " + std::string(buffer));
              }
            }
          } catch (Poco::Exception& e) {
            poco_error(logger, "Cannot handle this frame " + std::string(buffer) + " - error " + e.displayText());
          }
      }
  } while (n > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);
  ws.close();
  poco_notice(logger, "Disconnected from Kraken WebSockets API");

  } catch (Poco::Net::SSLConnectionUnexpectedlyClosedException& e) {
    poco_warning(logger, std::string("Caught SSL exception, restarting session ") + e.what());
  }
  catch (Poco::TimeoutException& e) {
    poco_warning(logger, std::string("Caught SSL exception, restarting session ") + e.what());
  }
  // Close WebSocket
  

}


uint64_t KrakenExchange::try_fetch_reference_rate(const std::string& pair_name) {
  poco_information(logger, "Trying to get ticker for pair " + pair_name);
  Poco::Net::HTTPSClientSession session(https_host);
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
    
    double value = std::stod(svalue);
    long rv = ((double)dec_power * value);
    poco_information(logger, " - found price of " + svalue + "(" + std::to_string(rv) + ")");
    return rv;
  }

  poco_information(logger, " - ticker not found");
  return 0;
}

void KrakenExchange::fetch_trading_pairs() {
  SymbolFactory& symbol_factory = SymbolFactory::get_factory();

  Poco::Net::HTTPSClientSession session(https_host);
  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/0/public/AssetPairs");
  session.sendRequest(request);
  Poco::Net::HTTPResponse response;
  std::istream& responseStream = session.receiveResponse(response);

  Poco::JSON::Parser parser;
  Poco::Dynamic::Var result = parser.parse(responseStream);
  Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

  Poco::JSON::Object::Ptr resultObject = object->get("result").extract<Poco::JSON::Object::Ptr>();

  for (Poco::JSON::Object::ConstIterator it = resultObject->begin(); it != resultObject->end(); ++it) {
    
    std::string name = it->first;
    auto ds = it->second;
    poco_information(logger, "Adding new trade pair - " + name + " - " + ds.toString());

    Poco::JSON::Object::Ptr asset_pair_object = it->second.extract<Poco::JSON::Object::Ptr>();
    std::string s1 = asset_pair_object->getValue<std::string>("base");
    std::string s2 = asset_pair_object->getValue<std::string>("quote");
    std::string wsname = asset_pair_object->getValue<std::string>("wsname");

    if (ignore_assets.count(s1) > 0 || ignore_assets.count(s2) > 0)
      continue;

    s1 = rewrite_symbol(s1);
    s2 = rewrite_symbol(s2);

    const Symbol& symbol1 = symbol_factory.get_symbol(s1, s1, exchange_string);
    const Symbol& symbol2 = symbol_factory.get_symbol(s2, s2, exchange_string);
    all_symbols.insert(symbol1);
    all_symbols.insert(symbol2);

    LeveledOrderBook ob(symbol1, symbol2);
    trading_pair_resolver.insert(std::make_pair(std::make_pair(s1, s2), wsname));

    auto ob_it = (trading_pairs.insert(std::make_pair(wsname, std::move(ob)))).first;
    ReverseOrderBook rev_ob(ob_it->second);
    reverse_order_books.insert(std::make_pair(wsname, std::move(rev_ob)));
  }


  for (const Symbol& s : all_symbols) {
    if (s.get_symbol() == base_asset) {
      s.set_reference_rate_estimate(dec_power);
      continue;
    }

    uint64_t v1 = try_fetch_reference_rate(s.get_symbol()+"USD");
    uint64_t v2 = v1 > 0 ? 0 : try_fetch_reference_rate("USD"+s.get_symbol());
    uint64_t v = v1 == 0 ? v2 : dec_power * dec_power / v1;
    //std::max(v2, v1 == 0 ? 0 : dec_power * dec_power / v1);
    s.set_reference_rate_estimate(v);
    poco_information(logger, "1 USD = " + std::to_string(s.get_reference_rate_estimate()) + " " + s.get_symbol());
  }

}

bool KrakenExchange::send_trade_sync(const Symbol& symbol1, const Symbol& symbol2, const uint64_t amount) {
  std::string content = "";
  if (has_trading_pair(symbol1, symbol2)) {
    content = "pair=" + trading_pair_resolver[std::make_pair(symbol1.get_symbol(), symbol2.get_symbol())]
        + "&type=buy&ordertype=market&volume=" + amount_to_string(amount);
  } else if (has_trading_pair(symbol2, symbol1)) {
    auto& ob = this->get_order_book(symbol1, symbol2);
    uint64_t exchanged_amount = ob.estimate_conversion_from_1(amount);
    content = "pair=" + trading_pair_resolver[std::make_pair(symbol2.get_symbol(), symbol1.get_symbol())]
        + "&type=sell&ordertype=market&volume=" + amount_to_string(exchanged_amount);
  } else {
    // unknown trade pair...
    poco_critical(logger, "Trying to trade unknown trade pair " + symbol1.get_symbol() + "/" + symbol2.get_symbol());
    return false;
  }

  try {
    Poco::JSON::Object::Ptr resultObject = send_authenticated_post_request(http_buy_endpoint, content);
    // parse the response here and see if the error field is empty and txid is returned.
    if (resultObject->has("error")) {
      Poco::JSON::Array::Ptr errors = resultObject->getArray("error");
      if (errors->size() > 0) {
        return false;
      }
    }
    if (!resultObject->has("txid")) {
      return false;
    }
    return true;

  } catch (order_failed_exception& e) {
    poco_error(logger, "failed to enter order for trade " + symbol1.get_symbol() + "/" + symbol2.get_symbol() + " of volume " + std::to_string(amount));
    return false;
  }
}
