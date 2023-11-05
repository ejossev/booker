#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/HMACEngine.h>
#include <Poco/Base64Encoder.h>
#include <Poco/SHA2Engine.h>
#include <iostream>
#include <sstream>
#include "Utils.hpp"




std::string signMessage(std::string message, std::string secret) {
    Poco::HMACEngine<Poco::SHA2Engine512> hmac(secret);
    hmac.update(message);
    const Poco::DigestEngine::Digest& digest = hmac.digest();
    std::stringstream ss;
    Poco::Base64Encoder encoder(ss);
    encoder.write(reinterpret_cast<const char *>(digest.data()), digest.size());
    encoder.close();
    return ss.str();
}

int test_send() {
    std::string APIKey = config->getString("Kraken.APIKey");
    std::string PrivateKey = config->getString("Kraken.PrivateKey");

    Poco::Net::HTTPSClientSession session("api.kraken.com");
    std::string content = "pair=XBTUSD&type=buy&ordertype=market&volume=0.1";
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, "/0/private/AddOrder");
    request.setContentType("application/x-www-form-urlencoded");
    request.set("API-Key", APIKey);
    std::string nonce = std::to_string(time(nullptr));

    request.set("API-Sign", signMessage(nonce + request.getURI() + content, PrivateKey));
    request.set("nonce", nonce);
    std::ostream& requestContentStream = session.sendRequest(request);
    requestContentStream << content;

    Poco::Net::HTTPResponse response;
    std::istream& responseContentStream = session.receiveResponse(response);
    std::string responseContent;

    Poco::StreamCopier::copyToString(responseContentStream, responseContent);
    std::cout << responseContent << std::endl;
    return 0;
}