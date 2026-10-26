#include "Database.h"
#include "Transfer.h"
#include "InstallmentPayment.h"
#include <cmath>
#include <iomanip>
#include <sstream>
using namespace std;

// ---------- small helpers ----------

static string esc(const string& s)
{
    string out;
    for (char c : s)
    {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

static string str(const pqxx::row& r, const char* col)
{
    return r[col].is_null() ? "null" : "\"" + esc(r[col].as<string>()) + "\"";
}

static string num(const pqxx::row& r, const char* col)
{
    return r[col].is_null() ? "null" : r[col].as<string>();
}

static string money(double v)
{
    ostringstream o;
    o << fixed << setprecision(2) << v;
    return o.str();
}

// ---------- setup ----------

Database::Database(const string& connStr) : conn(connStr) {}

Account Database::loadAccount(pqxx::work& txn, int id)
{
    pqxx::result r = txn.exec("SELECT id, customer_id, balance FROM accounts WHERE id = "
                              + to_string(id) + " FOR UPDATE");
    if (r.empty())
        throw runtime_error("Account not found.");
    return Account(r[0]["id"].as<int>(), r[0]["customer_id"].as<int>(), r[0]["balance"].as<double>());
}

void Database::saveBalance(pqxx::work& txn, const Account& acc)
{
    txn.exec("UPDATE accounts SET balance = " + money(acc.getBalance())
             + " WHERE id = " + to_string(acc.getId()));
}

void Database::logTransaction(pqxx::work& txn, int accountId, const string& type,
                              double signedAmount, double balanceAfter, int relatedId)
{
    string related = relatedId > 0 ? to_string(relatedId) : "NULL";
    txn.exec("INSERT INTO transactions (account_id, type, amount, balance_after, related_account_id) VALUES ("
             + to_string(accountId) + ", '" + type + "', " + money(signedAmount) + ", "
             + money(balanceAfter) + ", " + related + ")");
}

// ---------- reads ----------

string Database::accountsJson()
{
    lock_guard<mutex> lock(mtx);
    pqxx::work txn(conn);
    pqxx::result r = txn.exec(
        "SELECT a.id, a.account_number, c.first_name, c.last_name "
        "FROM accounts a JOIN customers c ON c.id = a.customer_id ORDER BY a.id");
    string out = "[";
    for (int i = 0; i < (int)r.size(); i++)
    {
        if (i) out += ",";
        out += "{\"id\":" + num(r[i], "id") + ",\"account_number\":" + str(r[i], "account_number")
             + ",\"name\":\"" + esc(r[i]["first_name"].as<string>() + " " + r[i]["last_name"].as<string>()) + "\"}";
    }
    txn.commit();
    return out + "]";
}

string Database::accountJson(int id)
{
    lock_guard<mutex> lock(mtx);
    processDueLocked(id);   // check-on-read: due installments are charged here

    pqxx::work txn(conn);
    pqxx::result r = txn.exec(
        "SELECT a.id, a.account_number, a.balance, a.card_type, a.spending_limit, "
        "c.first_name, c.last_name, c.email "
        "FROM accounts a JOIN customers c ON c.id = a.customer_id WHERE a.id = " + to_string(id));
    if (r.empty())
        throw runtime_error("Account not found.");
    const pqxx::row& row = r[0];
    string out = "{\"id\":" + num(row, "id")
        + ",\"account_number\":" + str(row, "account_number")
        + ",\"balance\":" + num(row, "balance")
        + ",\"card_type\":" + str(row, "card_type")
        + ",\"spending_limit\":" + num(row, "spending_limit")
        + ",\"first_name\":" + str(row, "first_name")
        + ",\"last_name\":" + str(row, "last_name")
        + ",\"email\":" + str(row, "email") + "}";
    txn.commit();
    return out;
}

string Database::transactionsJson(int id)
{
    lock_guard<mutex> lock(mtx);
    pqxx::work txn(conn);
    pqxx::result r = txn.exec(
        "SELECT t.id, t.type, t.amount, t.balance_after, t.created_at, a.account_number AS other "
        "FROM transactions t LEFT JOIN accounts a ON a.id = t.related_account_id "
        "WHERE t.account_id = " + to_string(id) + " ORDER BY t.created_at DESC, t.id DESC LIMIT 50");
    string out = "[";
    for (int i = 0; i < (int)r.size(); i++)
    {
        if (i) out += ",";
        out += "{\"id\":" + num(r[i], "id") + ",\"type\":" + str(r[i], "type")
             + ",\"amount\":" + num(r[i], "amount") + ",\"balance_after\":" + num(r[i], "balance_after")
             + ",\"created_at\":" + str(r[i], "created_at") + ",\"other\":" + str(r[i], "other") + "}";
    }
    txn.commit();
    return out + "]";
}

string Database::installmentsJson(int id)
{
    lock_guard<mutex> lock(mtx);
    pqxx::work txn(conn);
    pqxx::result plans = txn.exec(
        "SELECT id, total_amount, num_installments, status FROM installment_plans "
        "WHERE account_id = " + to_string(id) + " ORDER BY id DESC");
    string out = "[";
    for (int i = 0; i < (int)plans.size(); i++)
    {
        if (i) out += ",";
        string planId = plans[i]["id"].as<string>();
        out += "{\"id\":" + planId + ",\"total_amount\":" + num(plans[i], "total_amount")
             + ",\"num_installments\":" + num(plans[i], "num_installments")
             + ",\"status\":" + str(plans[i], "status") + ",\"payments\":[";
        pqxx::result pay = txn.exec(
            "SELECT installment_number, amount, due_date, status FROM installment_payments "
            "WHERE plan_id = " + planId + " ORDER BY installment_number");
        for (int j = 0; j < (int)pay.size(); j++)
        {
            if (j) out += ",";
            out += "{\"number\":" + num(pay[j], "installment_number") + ",\"amount\":" + num(pay[j], "amount")
                 + ",\"due_date\":" + str(pay[j], "due_date") + ",\"status\":" + str(pay[j], "status") + "}";
        }
        out += "]}";
    }
    txn.commit();
    return out + "]";
}

// ---------- writes ----------

void Database::transfer(int fromId, const string& toAccountNumber, double amount)
{
    lock_guard<mutex> lock(mtx);
    pqxx::work txn(conn);

    pqxx::result dest = txn.exec("SELECT id FROM accounts WHERE account_number = " + txn.quote(toAccountNumber));
    if (dest.empty())
        throw runtime_error("Destination account number not found.");
    int toId = dest[0]["id"].as<int>();
    if (toId == fromId)
        throw runtime_error("Cannot transfer to the same account.");

    Account from = loadAccount(txn, fromId);
    Account to = loadAccount(txn, toId);

    Transfer t(amount, to);
    t.apply(from);                        // may throw InvalidAmount / InsufficientBalance -> rollback

    saveBalance(txn, from);
    saveBalance(txn, to);
    logTransaction(txn, fromId, t.type(), -amount, from.getBalance(), toId);
    logTransaction(txn, toId, t.type(), amount, to.getBalance(), fromId);
    txn.commit();
}

void Database::createPlan(int accountId, double total, int n)
{
    lock_guard<mutex> lock(mtx);
    if (total <= 0) throw InvalidAmount();
    if (n < 2 || n > 12) throw runtime_error("Number of installments must be between 2 and 12.");

    pqxx::work txn(conn);
    loadAccount(txn, accountId);   // makes sure the account exists

    pqxx::result r = txn.exec("INSERT INTO installment_plans (account_id, total_amount, num_installments) VALUES ("
                              + to_string(accountId) + ", " + money(total) + ", " + to_string(n) + ") RETURNING id");
    string planId = r[0]["id"].as<string>();

    double each = floor(total / n * 100.0) / 100.0;
    double used = 0;
    for (int i = 0; i < n; i++)
    {
        double amt = (i == n - 1) ? total - used : each;   // last one absorbs rounding
        used += amt;
        txn.exec("INSERT INTO installment_payments (plan_id, installment_number, amount, due_date) VALUES ("
                 + planId + ", " + to_string(i + 1) + ", " + money(amt)
                 + ", CURRENT_DATE + " + to_string(i * 30) + ")");
    }
    txn.commit();
}

void Database::processDueLocked(int accountId)
{
    pqxx::work txn(conn);
    pqxx::result due = txn.exec(
        "SELECT p.id, p.plan_id, p.amount FROM installment_payments p "
        "JOIN installment_plans pl ON pl.id = p.plan_id "
        "WHERE pl.account_id = " + to_string(accountId) +
        " AND p.status = 'pending' AND p.due_date <= CURRENT_DATE "
        "ORDER BY p.due_date, p.installment_number");

    for (const auto& row : due)
    {
        Account acc = loadAccount(txn, accountId);
        InstallmentPayment ip(row["amount"].as<double>(), row["plan_id"].as<int>());
        try { ip.apply(acc); }
        catch (const exception&) { continue; }   // not enough balance: stays pending, retried on next read

        saveBalance(txn, acc);
        logTransaction(txn, accountId, ip.type(), -ip.getAmount(), acc.getBalance(), 0);
        txn.exec("UPDATE installment_payments SET status = 'paid', paid_at = now() WHERE id = "
                 + row["id"].as<string>());
        txn.exec("UPDATE installment_plans SET status = 'completed' WHERE id = " + to_string(ip.getPlanId())
                 + " AND NOT EXISTS (SELECT 1 FROM installment_payments WHERE plan_id = "
                 + to_string(ip.getPlanId()) + " AND status = 'pending')");
    }
    txn.commit();
}

void Database::advanceDays(int days)
{
    lock_guard<mutex> lock(mtx);
    if (days < 1 || days > 365) throw runtime_error("Days must be between 1 and 365.");
    pqxx::work txn(conn);
    txn.exec("UPDATE installment_payments SET due_date = due_date - " + to_string(days) + " WHERE status = 'pending'");
    txn.commit();
}
