#pragma once
#include <pqxx/pqxx>
#include <mutex>
#include <string>
#include "Account.h"
using namespace std;

// Only this class talks to PostgreSQL. Every public method returns JSON text or throws.
class Database
{
private:
    pqxx::connection conn;
    mutex mtx;

    Account loadAccount(pqxx::work& txn, int id);
    void saveBalance(pqxx::work& txn, const Account& acc);
    void logTransaction(pqxx::work& txn, int accountId, const string& type,
                        double signedAmount, double balanceAfter, int relatedId);
    void processDueLocked(int accountId);

public:
    Database(const string& connStr);

    string accountsJson();
    string accountJson(int id);
    string transactionsJson(int id);
    string installmentsJson(int id);

    void transfer(int fromId, const string& toAccountNumber, double amount);
    void createPlan(int accountId, double total, int numInstallments);
    void advanceDays(int days);   // demo only: makes pending installments come due sooner
};
