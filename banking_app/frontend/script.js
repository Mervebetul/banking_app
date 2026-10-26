// The one place that decides "who is the current customer".
// Login later = replace this with the logged-in user's account id.
let currentAccountId = 1;

const $ = (id) => document.getElementById(id);
const fmt = (n) => Number(n).toLocaleString("en-US", { style: "currency", currency: "USD" });
const groupIban = (s) => (s || "").replace(/(.{4})/g, "$1 ").trim();

async function api(path, options) {
  const res = await fetch(path, options);
  const data = await res.json();
  if (!res.ok) throw new Error(data.error || "Request failed");
  return data;
}

function post(path, fields) {
  return api(path, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: new URLSearchParams(fields),
  });
}

function showMsg(el, text, ok) {
  el.textContent = text;
  el.className = "msg " + (ok ? "ok" : "err");
}

async function loadAccountList() {
  const list = await api("/api/accounts");
  $("accountSelect").innerHTML = list
    .map((a) => `<option value="${a.id}">${a.name}</option>`)
    .join("");
  $("accountSelect").value = currentAccountId;
  return list;
}

function fillRecipients(list) {
  $("toAccount").innerHTML = list
    .filter((a) => a.id !== currentAccountId)
    .map((a) => `<option value="${a.account_number}">${a.name} - ${groupIban(a.account_number).slice(0, 14)}...</option>`)
    .join("");
}

async function loadAccount() {
  const a = await api(`/api/accounts/${currentAccountId}`);
  $("holder").textContent = `${a.first_name} ${a.last_name}`;
  $("email").textContent = a.email || "";
  $("iban").textContent = groupIban(a.account_number);
  $("balance").textContent = fmt(a.balance);
  $("cardType").textContent = a.card_type || "-";
  $("limit").textContent = a.spending_limit == null ? "-" : fmt(a.spending_limit);
}

async function loadHistory() {
  const rows = await api(`/api/accounts/${currentAccountId}/transactions`);
  if (!rows.length) {
    $("history").innerHTML = `<tr><td colspan="5" class="muted">No transactions yet.</td></tr>`;
    return;
  }
  $("history").innerHTML = rows
    .map((t) => {
      const cls = t.amount >= 0 ? "in" : "out";
      let details = "";
      if (t.type === "transfer") {
        details = t.amount >= 0 ? `From ${groupIban(t.other || "")}` : `To ${groupIban(t.other || "")}`;
      } else {
        details = "Installment payment";
      }
      return `<tr>
        <td>${t.created_at.slice(0, 16)}</td>
        <td><span class="pill">${t.type}</span></td>
        <td class="muted">${details}</td>
        <td class="num ${cls}">${t.amount >= 0 ? "+" : ""}${fmt(t.amount)}</td>
        <td class="num">${fmt(t.balance_after)}</td>
      </tr>`;
    })
    .join("");
}

async function loadPlans() {
  const plans = await api(`/api/accounts/${currentAccountId}/installments`);
  if (!plans.length) {
    $("plans").innerHTML = "No plans yet.";
    return;
  }
  $("plans").innerHTML = plans
    .map(
      (p) => `<div class="plan">
        <div class="plan-head">
          <span>${fmt(p.total_amount)} in ${p.num_installments} payments</span>
          <span class="pill">${p.status}</span>
        </div>
        <div class="pay-list">
          ${p.payments
            .map((x) => `<span class="pay ${x.status}">#${x.number} - ${fmt(x.amount)} - ${x.due_date} (${x.status})</span>`)
            .join("")}
        </div>
      </div>`
    )
    .join("");
}

async function refresh() {
  const list = await loadAccountList();
  fillRecipients(list);
  await loadAccount();          // also charges any due installments
  await loadHistory();
  await loadPlans();
}

$("accountSelect").addEventListener("change", (e) => {
  currentAccountId = Number(e.target.value);
  refresh();
});

$("transferForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    await post("/api/transfer", {
      from_id: currentAccountId,
      to_account_number: $("toAccount").value,
      amount: $("transferAmount").value,
    });
    showMsg($("transferMsg"), "Transfer completed.", true);
    $("transferAmount").value = "";
    await refresh();
  } catch (err) {
    showMsg($("transferMsg"), err.message, false);
  }
});

$("planForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    await post("/api/installments", {
      account_id: currentAccountId,
      total: $("planTotal").value,
      installments: $("planCount").value,
    });
    showMsg($("planMsg"), "Plan started. First payment is charged today.", true);
    $("planTotal").value = "";
    await refresh();
  } catch (err) {
    showMsg($("planMsg"), err.message, false);
  }
});

$("advanceBtn").addEventListener("click", async () => {
  try {
    await post("/api/demo/advance", { days: 30 });
    await refresh();
  } catch (err) {
    alert(err.message);
  }
});

refresh().catch((err) => {
  $("holder").textContent = "Could not reach the server";
  console.error(err);
});
