// Run from the backend/ folder so "../frontend" resolves.
#include "httplib.h"
#include "Database.h"
#include <cstdlib>
#include <iostream>
using namespace std;

static string errJson(const string& msg)
{
    string out;
    for (char c : msg) { if (c == '"' || c == '\\') out += '\\'; out += c; }
    return "{\"error\":\"" + out + "\"}";
}

// runs a handler, turns any exception into a JSON error with status 400
template <typename F>
static void respond(httplib::Response& res, F fn)
{
    try { res.set_content(fn(), "application/json"); }
    catch (const exception& ex)
    {
        res.status = 400;
        res.set_content(errJson(ex.what()), "application/json");
    }
}

int main()
{
    const char* env = getenv("DB_CONN");
    string connStr = env ? env : "host=localhost port=5432 dbname=bankingdb user=admin password=admin123";

    Database* db;
    try { db = new Database(connStr); }
    catch (const exception& ex)
    {
        cerr << "Database connection failed: " << ex.what() << endl;
        return 1;
    }

    httplib::Server svr;
    svr.set_mount_point("/", "../frontend");

    svr.Get("/api/accounts", [&](const httplib::Request&, httplib::Response& res) {
        respond(res, [&] { return db->accountsJson(); });
    });

    svr.Get(R"(/api/accounts/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = stoi(req.matches[1]);
        respond(res, [&] { return db->accountJson(id); });
    });

    svr.Get(R"(/api/accounts/(\d+)/transactions)", [&](const httplib::Request& req, httplib::Response& res) {
        int id = stoi(req.matches[1]);
        respond(res, [&] { return db->transactionsJson(id); });
    });

    svr.Get(R"(/api/accounts/(\d+)/installments)", [&](const httplib::Request& req, httplib::Response& res) {
        int id = stoi(req.matches[1]);
        respond(res, [&] { return db->installmentsJson(id); });
    });

    // form fields: from_id, to_account_number, amount
    svr.Post("/api/transfer", [&](const httplib::Request& req, httplib::Response& res) {
        respond(res, [&] {
            if (!req.has_param("from_id") || !req.has_param("to_account_number") || !req.has_param("amount"))
                throw runtime_error("Missing fields.");
            db->transfer(stoi(req.get_param_value("from_id")),
                         req.get_param_value("to_account_number"),
                         stod(req.get_param_value("amount")));
            return string("{\"ok\":true}");
        });
    });

    // form fields: account_id, total, installments
    svr.Post("/api/installments", [&](const httplib::Request& req, httplib::Response& res) {
        respond(res, [&] {
            if (!req.has_param("account_id") || !req.has_param("total") || !req.has_param("installments"))
                throw runtime_error("Missing fields.");
            db->createPlan(stoi(req.get_param_value("account_id")),
                           stod(req.get_param_value("total")),
                           stoi(req.get_param_value("installments")));
            return string("{\"ok\":true}");
        });
    });

    // demo helper: pretend N days passed so pending installments come due
    svr.Post("/api/demo/advance", [&](const httplib::Request& req, httplib::Response& res) {
        respond(res, [&] {
            db->advanceDays(stoi(req.get_param_value("days")));
            return string("{\"ok\":true}");
        });
    });

    cout << "Server running on http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
