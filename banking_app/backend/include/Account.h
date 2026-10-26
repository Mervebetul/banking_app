#pragma once
#include <iostream>
#include <exception>
#include <string>
using namespace std;

class InvalidAmount : public exception
{
public:
    const char* what() const noexcept override
    {
        return "Amount must be positive.";
    }
};

class InsufficientBalance : public exception
{
    string message;
public:
    InsufficientBalance(double currentBalance)
    {
        message = "Not enough balance, current balance in this account: " + to_string(currentBalance);
    }
    const char* what() const noexcept override
    {
        return message.c_str();
    }
};

class Account
{
private:
    int id;
    int customerId;
    double balance;

public:
    Account(int ID, int cus_ID, double Balance) : id(ID), customerId(cus_ID), balance(Balance) {}

    int getId() const { return id; }
    int getCustomerId() const { return customerId; }
    double getBalance() const { return balance; }

    void credit(double amount)
    {
        if (amount <= 0)
            throw InvalidAmount();
        balance += amount;
    }

    void debit(double amount)
    {
        if (amount <= 0)
            throw InvalidAmount();
        if (amount > balance)
            throw InsufficientBalance(balance);
        balance -= amount;
    }
};
