-- staging table for the raw CSV import
DROP TABLE IF EXISTS raw_customers;
CREATE TABLE raw_customers (
    first_name TEXT,
    last_name TEXT,
    email TEXT
);

-- real customers table
DROP TABLE IF EXISTS customers CASCADE;
CREATE TABLE customers (
    id SERIAL PRIMARY KEY,
    first_name TEXT NOT NULL,
    last_name TEXT NOT NULL,
    email TEXT UNIQUE
);

-- accounts, linked to customers
DROP TABLE IF EXISTS accounts CASCADE;
CREATE TABLE accounts (
    id SERIAL PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(id),
    account_number TEXT UNIQUE,
    balance NUMERIC(12,2) NOT NULL DEFAULT 0,
    card_type TEXT,
    spending_limit NUMERIC(12,2)
);

-- transactions, linked to accounts
DROP TABLE IF EXISTS transactions CASCADE;
CREATE TABLE transactions (
    id SERIAL PRIMARY KEY,
    account_id INTEGER NOT NULL REFERENCES accounts(id),
    type VARCHAR(20) NOT NULL CHECK (type IN ('transfer','installment')),
    amount NUMERIC(12,2) NOT NULL,
    balance_after NUMERIC(12,2) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT now()
);

-- installment plans, linked to accounts
DROP TABLE IF EXISTS installment_plans CASCADE;
CREATE TABLE installment_plans (
    id SERIAL PRIMARY KEY,
    account_id INTEGER NOT NULL REFERENCES accounts(id),
    total_amount NUMERIC(12,2) NOT NULL,
    num_installments INTEGER NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT now(),
    status VARCHAR(20) NOT NULL DEFAULT 'active' CHECK (status IN ('active','completed'))
);

-- individual installment payments, linked to a plan
DROP TABLE IF EXISTS installment_payments CASCADE;
CREATE TABLE installment_payments (
    id SERIAL PRIMARY KEY,
    plan_id INTEGER NOT NULL REFERENCES installment_plans(id),
    installment_number INTEGER NOT NULL,
    amount NUMERIC(12,2) NOT NULL,
    due_date DATE NOT NULL,
    status VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending','paid')),
    paid_at TIMESTAMP
);
