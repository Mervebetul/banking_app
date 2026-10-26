#pragma once
#include "Transaction.h"

class InstallmentPayment : public Transaction
{
private:
    int planId;

public:
    InstallmentPayment(double amt, int plan) : Transaction(amt), planId(plan) {}

    int getPlanId() const { return planId; }

    void apply(Account& account) override { account.debit(amount); }

    string type() override { return "installment"; }
};
