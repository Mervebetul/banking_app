// No database: proves the class hierarchy works.
// Compile from backend/:  g++ -std=c++20 -Iinclude test/account_test.cpp -o account_test
#include <iostream>
#include <vector>
#include <memory>
#include "Transfer.h"
#include "InstallmentPayment.h"
using namespace std;

int main()
{
    Account a(1, 1, 1000);
    Account b(2, 2, 500);

    // polymorphism: one list, different transaction types
    vector<unique_ptr<Transaction>> list;
    list.push_back(make_unique<Transfer>(150, b));
    list.push_back(make_unique<InstallmentPayment>(100, 1));

    for (auto& t : list)
    {
        t->apply(a);
        cout << t->type() << " -> a: " << a.getBalance() << ", b: " << b.getBalance() << endl;
    }

    // exception paths
    try { Transfer(5000, b).apply(a); }
    catch (const exception& ex) { cout << "Error: " << ex.what() << endl; }

    try { Transfer(-5, b).apply(a); }
    catch (const exception& ex) { cout << "Error: " << ex.what() << endl; }

    return 0;
}
