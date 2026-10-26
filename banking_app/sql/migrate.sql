-- Run once in DBeaver (Execute SQL Script, Alt+X). Safe to re-run.

ALTER TABLE accounts ADD COLUMN IF NOT EXISTS card_type TEXT;
ALTER TABLE accounts ADD COLUMN IF NOT EXISTS spending_limit NUMERIC(12,2);

ALTER TABLE transactions ADD COLUMN IF NOT EXISTS related_account_id INTEGER REFERENCES accounts(id);

-- old test rows from the deposit/withdrawal days (type no longer allowed)
DELETE FROM transactions WHERE type NOT IN ('transfer','installment');
ALTER TABLE transactions DROP CONSTRAINT IF EXISTS transactions_type_check;
ALTER TABLE transactions ADD CONSTRAINT transactions_type_check CHECK (type IN ('transfer','installment'));

-- fake IBANs + display-only card fields
UPDATE accounts
SET account_number = 'TR33' || lpad(id::text, 22, '0')
WHERE account_number IS NULL;

UPDATE accounts SET card_type = 'Debit - Visa' WHERE card_type IS NULL;
UPDATE accounts SET spending_limit = 5000 WHERE spending_limit IS NULL;

-- check
SELECT a.id, c.first_name, c.last_name, a.account_number, a.balance, a.card_type, a.spending_limit
FROM accounts a JOIN customers c ON c.id = a.customer_id
ORDER BY a.id LIMIT 5;
