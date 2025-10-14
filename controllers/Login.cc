#include "Login.h"
#include <json/json.h>

Login::Login() {
    // Optional constructor — can be empty unless you need initialization
}

void Login::getInfo(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
    // Extract user info that FirebaseAuthFilter should have attached to the request
    // (Filters typically set attributes like "uid", "email", etc.)
    auto uid = req->attributes()->get<std::string>("uid");

    Json::Value result;

    if (uid != "") {
        result["success"] = true;
        result["uid"] = uid;

        // Optionally, get other user info if FirebaseAuthFilter stored them
    } else {
        result["success"] = false;
        result["error"] = "User information not found. Authentication filter may not be applied.";
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
}

