#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class MyProfile : public drogon::HttpController<MyProfile> {
    public:
        METHOD_LIST_BEGIN
            METHOD_ADD(MyProfile::getInfo, "", drogon::Get, "FirebaseAuthFilter");
        METHOD_LIST_END

        void getInfo(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
        MyProfile();
};
