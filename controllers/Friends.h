#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include  <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class Friends : public drogon::HttpController<Friends> {
    public:
        METHOD_LIST_BEGIN
            METHOD_ADD (Friends::getFriends, "", drogon::Get, "FirebaseAuthFilter");
            METHOD_ADD(Friends::saveFriend, "/{friend_uid}", drogon::Post, "FirebaseAuthFilter");
            METHOD_ADD(Friends::deleteFriend, "/{friend_uid}", drogon::Delete, "FirebaseAuthFilter");
        METHOD_LIST_END

            void getFriends(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
        void saveFriend(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid);
        void deleteFriend(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid);
        Friends();
};
