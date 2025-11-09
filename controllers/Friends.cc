#include "Friends.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/orm/Exception.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <models/Users.h>

using namespace drogon::orm;
using namespace drogon_model::equatix;

Friends::Friends() { LOG_DEBUG << "Friends controller initialized"; }

void Friends::getFriends(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");

    client->execSqlAsync(
        "SELECT u.uid, u.name, u.elo, u.photo FROM friends f JOIN users u ON f.friend_uid = u.uid WHERE f.uid = $1 AND f.status='accepted'",
        [callback](const Result &r){
            Json::Value root;
            Json::Value arr(Json::arrayValue);
            for(const auto &row: r){
                Json::Value u;
                u["uid"] = row["uid"].as<std::string>();
                u["name"] = row["name"].as<std::string>();
                u["elo"] = row["elo"].as<int>();
                u["photo"] = row["photo"].as<std::string>();
                arr.append(u);
            }
            root["friends"] = arr;
            callback(drogon::HttpResponse::newHttpJsonResponse(root));
        },
        [callback](const DrogonDbException &e){
            Json::Value err; err["error"] = e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(err));
        }, uid
    );
}

void Friends::getFriendRequests(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");

    client->execSqlAsync(
        "SELECT u.uid, u.name, u.elo, u.photo FROM friends f JOIN users u ON f.requested_by = u.uid WHERE f.friend_uid=$1 AND f.status='pending'",
        [callback](const Result &r){
            Json::Value root; Json::Value arr(Json::arrayValue);
            for(const auto &row: r){
                Json::Value u;
                u["uid"] = row["uid"].as<std::string>();
                u["name"] = row["name"].as<std::string>();
                u["elo"] = row["elo"].as<int>();
                u["photo"] = row["photo"].as<std::string>();
                arr.append(u);
            }
            root["requests"] = arr;
            callback(drogon::HttpResponse::newHttpJsonResponse(root));
        },
        [callback](const DrogonDbException &e){
            Json::Value err; err["error"] = e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(err));
        }, uid
    );
}

void Friends::sendFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid) {
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");

    if(uid == friend_uid){
        Json::Value res; res["success"] = false; res["error"] = "Cannot add yourself"; 
        callback(drogon::HttpResponse::newHttpJsonResponse(res));
        return;
    }

    // Check if friend exists
    client->execSqlAsync(
        "SELECT uid FROM users WHERE uid=$1 LIMIT 1",
        [client, uid, friend_uid, callback](const Result &r){
            if(r.empty()){
                Json::Value res; res["success"]=false; res["error"]="User not found";
                callback(drogon::HttpResponse::newHttpJsonResponse(res));
                return;
            }
            // Check existing friendship/request
            client->execSqlAsync(
                "SELECT 1 FROM friends WHERE (uid=$1 AND friend_uid=$2) OR (uid=$2 AND friend_uid=$1) LIMIT 1",
                [client, uid, friend_uid, callback](const Result &r2){
                    if(!r2.empty()){
                        Json::Value res; res["success"]=false; res["error"]="Friendship or request already exists";
                        callback(drogon::HttpResponse::newHttpJsonResponse(res));
                        return;
                    }
                    // Insert request
                    client->execSqlAsync(
                        "INSERT INTO friends(uid, friend_uid, status, requested_by) VALUES($1,$2,'pending',$3)",
                        [callback](const Result &){
                            Json::Value res; res["success"]=true; res["message"]="Friend request sent";
                            callback(drogon::HttpResponse::newHttpJsonResponse(res));
                        },
                        [callback](const DrogonDbException &e){
                            Json::Value res; res["success"]=false; res["error"]=e.base().what();
                            callback(drogon::HttpResponse::newHttpJsonResponse(res));
                        }, uid, friend_uid, uid
                    );
                },
                [callback](const DrogonDbException &e){ 
                    Json::Value res; res["success"]=false; res["error"]=e.base().what();
                    callback(drogon::HttpResponse::newHttpJsonResponse(res));
                }, uid, friend_uid
            );
        },
        [callback](const DrogonDbException &e){
            Json::Value res; res["success"]=false; res["error"]=e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        }, friend_uid
    );
}

void Friends::getSentFriendRequests(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");

    client->execSqlAsync(
        "SELECT u.uid, u.name, u.elo, u.photo FROM friends f JOIN users u ON f.friend_uid = u.uid WHERE f.uid=$1 AND f.status='pending'",
        [callback](const Result &r){
            Json::Value root; 
            Json::Value arr(Json::arrayValue);
            for(const auto &row: r){
                Json::Value u;
                u["uid"] = row["uid"].as<std::string>();
                u["name"] = row["name"].as<std::string>();
                u["elo"] = row["elo"].as<int>();
                u["photo"] = row["photo"].as<std::string>();
                arr.append(u);
            }
            root["sentRequests"] = arr;
            callback(drogon::HttpResponse::newHttpJsonResponse(root));
        },
        [callback](const DrogonDbException &e){
            Json::Value err; err["error"] = e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(err));
        }, uid
    );
}

void Friends::cancelSentFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid){
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");

    client->execSqlAsync(
        "DELETE FROM friends WHERE uid=$1 AND friend_uid=$2 AND status='pending'",
        [callback](const Result &){
            Json::Value res; res["success"] = true; res["message"] = "Friend request canceled";
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        },
        [callback](const DrogonDbException &e){
            Json::Value res; res["success"] = false; res["error"] = e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        }, uid, friend_uid
    );
}


void Friends::acceptFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid){
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");

    // Update the request to accepted
    client->execSqlAsync(
        "UPDATE friends SET status='accepted' WHERE uid=$1 AND friend_uid=$2 AND status='pending'",
        [client, uid, friend_uid, callback](const Result &r){
            if(r.affectedRows()==0){
                Json::Value res; res["success"]=false; res["error"]="No pending request found";
                callback(drogon::HttpResponse::newHttpJsonResponse(res));
                return;
            }
            // Optional: insert reverse row
            client->execSqlAsync(
                "INSERT INTO friends(uid, friend_uid, status, requested_by) VALUES($1,$2,'accepted',$3)",
                [callback](const Result &){
                    Json::Value res; res["success"]=true; res["message"]="Friend request accepted";
                    callback(drogon::HttpResponse::newHttpJsonResponse(res));
                },
                [callback](const DrogonDbException &e){
                    Json::Value res; res["success"]=false; res["error"]=e.base().what();
                    callback(drogon::HttpResponse::newHttpJsonResponse(res));
                }, uid, friend_uid, friend_uid
            );
        },
        [callback](const DrogonDbException &e){
            Json::Value res; res["success"]=false; res["error"]=e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        }, friend_uid, uid
    );
}

void Friends::rejectFriendRequest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid){
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");

    client->execSqlAsync(
        "DELETE FROM friends WHERE uid=$1 AND friend_uid=$2 AND status='pending'",
        [callback](const Result &r){
            Json::Value res; res["success"]=true; res["message"]="Friend request rejected";
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        },
        [callback](const DrogonDbException &e){
            Json::Value res; res["success"]=false; res["error"]=e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        }, friend_uid, uid
    );
}

void Friends::deleteFriend(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid){
    auto client = drogon::app().getDbClient();
    auto uid= req->attributes()->get<std::string>("uid");

    client->execSqlAsync(
        "DELETE FROM friends WHERE (uid=$1 AND friend_uid=$2) OR (uid=$2 AND friend_uid=$1)",
        [callback](const Result&){
            Json::Value res; res["success"]=true; res["message"]="Friend deleted";
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        },
        [callback](const DrogonDbException &e){
            Json::Value res; res["success"]=false; res["error"]=e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        }, uid, friend_uid
    );
}
