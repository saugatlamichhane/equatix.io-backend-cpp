#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class Profile : public drogon::HttpController<Profile> {
    public:
        METHOD_LIST_BEGIN
        METHOD_ADD(Profile::getInfo, "/{uid}", drogon::Get, "FirebaseAuthFilter");
        METHOD_LIST_END

        void getInfo(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& uid) const;

        Profile();

};
