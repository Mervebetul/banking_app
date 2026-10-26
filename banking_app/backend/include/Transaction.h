#pragma once
#include <string>
#include "Account.h"
using namespace std;

class Transaction
{
protected:
    double amount;

public:
    Transaction(double amt) : amount(amt) {}
    virtual ~Transaction() = default;

    double getAmount() const { return amount; }

    virtual void apply(Account& account) = 0;
    virtual string type() = 0;
};
