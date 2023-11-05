//
// Created by Josef Sevcik on 21/03/2023.
//
#pragma once

#include <exception>
#include <string>
#include <Poco/Net/HTTPResponse.h>



class order_failed_exception : public std::exception {
private:
  const std::string reason;
  const Poco::Net::HTTPResponse::HTTPStatus statusCode;
public:
  order_failed_exception(Poco::Net::HTTPResponse::HTTPStatus _statusCode, const std::string& _reason) : reason(_reason), statusCode(_statusCode) {}
  const std::string& get_reason() const {
    return reason;
  }
  Poco::Net::HTTPResponse::HTTPStatus get_status_code() const {
    return statusCode;
  }
};

class not_found_exception : public std::exception {
private:
  const std::string reason;
public:
  not_found_exception(const std::string& _reason) : reason(_reason) {}
  const std::string& get_reason() const {
    return reason;
  }
};