#pragma once
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class Friends : public drogon::HttpController<Friends> {
public:
    METHOD_LIST_BEGIN
        // List accepted friends
        ADD_METHOD_TO(Friends::getFriends, "/friends", drogon::Get, "FirebaseAuthFilter");

        // List incoming friend requests
        ADD_METHOD_TO(Friends::getFriendRequests, "/friend-requests", drogon::Get, "FirebaseAuthFilter");

        // Send friend request
        ADD_METHOD_TO(Friends::sendFriendRequest, "/friend-request/{friend_uid}", drogon::Post, "FirebaseAuthFilter");

        // List sent friend requests
        ADD_METHOD_TO(Friends::getSentFriendRequests, "/friend-requests/sent", drogon::Get, "FirebaseAuthFilter");

        //Cancel sent friend request
        ADD_METHOD_TO(Friends::cancelSentFriendRequest, "/friend-request/cancel/{friend_uid}", drogon::Delete, "FirebaseAuthFilter");
        // Accept friend request
        ADD_METHOD_TO(Friends::acceptFriendRequest, "/friend-request/accept/{friend_uid}", drogon::Post, "FirebaseAuthFilter");

        // Reject friend request
        ADD_METHOD_TO(Friends::rejectFriendRequest, "/friend-request/reject/{friend_uid}", drogon::Post, "FirebaseAuthFilter");

        // Delete friend
        ADD_METHOD_TO(Friends::deleteFriend, "/friend/{friend_uid}", drogon::Delete, "FirebaseAuthFilter");
    METHOD_LIST_END

    Friends();

    void getFriends(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getFriendRequests(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void sendFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid);
    void getSentFriendRequests(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void cancelSentFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid);
    void acceptFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid);
    void rejectFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid);
    void deleteFriend(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid);
};
