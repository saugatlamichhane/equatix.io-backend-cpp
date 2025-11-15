#pragma once

/*#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class Clans : public drogon::HttpController<Clans> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(Clans::getClanInfo, "clans/info/{id}", drogon::Get,
"FirebaseAuthFilter"); ADD_METHOD_TO(Clans::getMyClanInfo, "clans/myclan/info",
drogon::Get, "FirebaseAuthFilter"); ADD_METHOD_TO(Clans::createClan,
"clans/create", drogon::Post, "FirebaseAuthFilter");
        ADD_METHOD_TO(Clans::leaveClan, "clans/leave", drogon::Delete,
"FirebaseAuthFilter");

        ADD_METHOD_TO(Clans::sendRequest, "clans/request/{id}", drogon::Post,
"FirebaseAuthFilter"); ADD_METHOD_TO(Clans::getRequests,
"clans/myclan/requests", drogon::Get, "FirebaseAuthFilter");
        ADD_METHOD_TO(Clans::acceptRequest, "clans/myclan/accept/{uid}",
drogon::Put, "FirebaseAuthFilter"); ADD_METHOD_TO(Clans::rejectRequest,
"clans/myclan/reject/{uid}", drogon::Put, "FirebaseAuthFilter");

        ADD_METHOD_TO(Clans::invite, "clans/myclan/invite/{uid}", drogon::Post,
"FirebaseAuthFilter"); ADD_METHOD_TO(Clans::getInvites, "clans/invites",
drogon::Get, "FirebaseAuthFilter"); ADD_METHOD_TO(Clans::acceptInvite,
"clans/invite/accept/{id}", drogon::Put, "FirebaseAuthFilter");
        ADD_METHOD_TO(Clans::rejectInvite, "clans/invite/reject/{id}",
drogon::Put, "FirebaseAuthFilter");

        ADD_METHOD_TO(Clans::promote, "clans/myclan/promote/{uid}", drogon::Put,
"FirebaseAuthFilter"); ADD_METHOD_TO(Clans::demote, "clans/myclan/demote/{uid}",
drogon::Put, "FirebaseAuthFilter"); ADD_METHOD_TO(Clans::kick,
"clans/myclan/kick/{uid}", drogon::Delete, "FirebaseAuthFilter");

        ADD_METHOD_TO(Clans::search, "clans/search/{key}", drogon::Get,
"FirebaseAuthFilter"); METHOD_LIST_END

public:
    // === Constructor ===
    Clans();

    // === Clan Info ===
    void getClanInfo(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&&
callback, const std::string& id) const;

    void getMyClanInfo(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&&
callback) const;

    // === Clan Management ===
    void createClan(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&&
callback) const;

    void leaveClan(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&&
callback) const;

    void promote(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                 const std::string& uid) const;

    void demote(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& uid) const;

    void kick(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
              const std::string& uid) const;

    // === Join Requests ===
    void sendRequest(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&&
callback, const std::string& clanId) const;

    void getRequests(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&&
callback) const;

    void acceptRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&&
callback, const std::string& uid) const;

    void rejectRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&&
callback, const std::string& uid) const;

    // === Invites ===
    void invite(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& uid) const;

    void getInvites(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&&
callback) const;

    void acceptInvite(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&&
callback, const std::string& id) const;

    void rejectInvite(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&&
callback, const std::string& id) const;

    // === Search ===
    void search(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& key) const;
};
*/
