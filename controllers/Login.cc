#include "Login.h"

Login::Login() {
}

// Explicitly use the drogon namespace for types in the signature
void Login::getInfo(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
    
    // Extract attributes attached by FirebaseAuthFilter
    auto uid = req->attributes()->get<std::string>("uid");
    auto name = req->attributes()->get<std::string>("name");
    auto email = req->attributes()->get<std::string>("email");
    auto photo = req->attributes()->get<std::string>("photo");

    auto client = drogon::app().getDbClient();

    // Consolidated Sync Point: UPSERT user details
    client->execSqlAsync(
        "INSERT INTO users(uid, name, email, photo) VALUES($1, $2, $3, $4) "
        "ON CONFLICT (uid) DO UPDATE SET name = $2, photo = $4",
        [callback, uid](const drogon::orm::Result &r) {
            Json::Value res;
            res["success"] = true;
            res["uid"] = uid;
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        },
        [callback](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "DB Sync Error: " << e.base().what();
            Json::Value res;
            res["success"] = false;
            res["error"] = "User sync failed";
            callback(drogon::HttpResponse::newHttpJsonResponse(res));
        },
        uid, name, email, photo
    );
}