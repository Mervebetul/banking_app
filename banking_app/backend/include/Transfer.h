#pragma once
#include "Transaction.h"

class Transfer : public Transaction
{
private:
    Account& toAccount;

public:
    Transfer(double amt, Account& to) : Transaction(amt), toAccount(to) {}

    // debit first: if it throws, nothing was credited
    void apply(Account& fromAccount) override
    {
        fromAccount.debit(amount);
        toAccount.credit(amount);
    }

    string type() override { return "transfer"; }
};
