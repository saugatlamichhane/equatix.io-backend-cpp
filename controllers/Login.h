#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class Login : public drogon::HttpController<Login> {
    public:
        METHOD_LIST_BEGIN
        ADD_METHOD_TO(Login::getInfo, "/login", drogon::Get, "FirebaseAuthFilter");
        METHOD_LIST_END

        void getInfo(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

        Login();

};
