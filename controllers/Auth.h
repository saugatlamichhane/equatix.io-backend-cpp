#pragma once

#include <drogon/HttpController.h>
#include <json/json.h>

using namespace drogon;

void fetchFirebaseKeys();

class Auth : public drogon::HttpController<Auth> {
public:
  METHOD_LIST_BEGIN
  // use METHOD_ADD to add your custom processing function here;
  ADD_METHOD_TO(Auth::login, "/login", Post);
  METHOD_LIST_END

    void login(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
    };