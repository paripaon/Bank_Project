import requests

admin_server = "http://127.0.0.1:47001"
user_server = "http://127.0.0.1:47002"

# helper
def step(n, label, resp, expect_ok=True):
    ok = resp.status_code < 400
    verdict = "OK" if ok == expect_ok else "FAIL"
    print(f"\n[{n}] {label} -> status={resp.status_code} ({verdict})")
    try:
        print("     JSON:", resp.json())
    except Exception:
        print("     Raw:", resp.text)
    return resp

# helper
def data(resp):
    try:
        return resp.json().get("data", {})
    except Exception:
        return {}

# tests
# 0. login admin
admin_token = data(requests.post(f"{admin_server}/admin/auth/login", json={"username": "admin", "password": "admin"})).get("token")
if not admin_token:
    print("Admin login failed, aborting.")
admin_headers = {"Authorization": f"Bearer {admin_token}"}

# 1. create branch
branch_resp = step(1, "Create branch", requests.post(f"{admin_server}/admin/branches", headers=admin_headers, json={"name": "Central Branch"}))
branches = data(branch_resp).get("branches", [])
branch_id = branches[-1]["id"] if branches else None
if branch_id is None:
    print("Could not determine branch_id, aborting.")

# 2. create sender account directly via admin
sender_resp = step(2, "Create sender account via admin", requests.post(f"{admin_server}/admin/accounts", headers=admin_headers, json={"branch_id": branch_id, "password": "senderPass"}))
sender_account_id = data(sender_resp).get("account", {}).get("account_number")

# 3. create receiver account directly via admin
receiver_resp = step(3, "Create receiver account via admin", requests.post(f"{admin_server}/admin/accounts", headers=admin_headers, json={"branch_id": branch_id, "password": "receiverPass"}))
receiver_account_id = data(receiver_resp).get("account", {}).get("account_number")

# 4. deposit into sender account
step(4, "Deposit into sender account", requests.post(f"{admin_server}/admin/accounts/{sender_account_id}/deposits", headers=admin_headers, json={"amount": 5000}))

# 5. transfer sender -> receiver
step(5, "Transfer sender -> receiver", requests.post(f"{admin_server}/admin/transfers", headers=admin_headers, json={"from": sender_account_id, "to": receiver_account_id, "amount": 1000, "password": "senderPass"}))

# 6. withdraw more than balance -> expect failure
step(6, "Withdraw more than balance (expect error)", requests.post(f"{admin_server}/admin/accounts/{sender_account_id}/withdrawals", headers=admin_headers, json={"amount": 999999999, "password": "senderPass"}), expect_ok=False)

# 7. admin logout
step(7, "Admin logout", requests.post(f"{admin_server}/admin/auth/logout", headers=admin_headers))

# 8. admin action after logout -> expect 401
step(8, "Admin lists branches after logout (expect 401)", requests.get(f"{admin_server}/admin/branches", headers=admin_headers), expect_ok=False)


national_code = "441471773"
user_password = "1234"

# 9. signup
step(9, "User signup", requests.post(f"{user_server}/users/signup", json={"national_code": national_code, "password": user_password}))

# 10. login
user_login_resp = step(10, "User login", requests.post(f"{user_server}/users/login", json={"national_code": national_code, "password": user_password}))
user_token = data(user_login_resp).get("token")
user_headers = {"Authorization": f"Bearer {user_token}"} if user_token else {}

# 11. authenticated action while logged in (list my accounts)
step(11, "List my accounts while logged in", requests.get(f"{user_server}/accounts/mine", headers=user_headers))

# 12. logout
step(12, "User logout", requests.post(f"{user_server}/users/logout", headers=user_headers))

# 13. same action after logout -> expect 401
step(13, "List my accounts after logout (expect 401)", requests.get(f"{user_server}/accounts/mine", headers=user_headers), expect_ok=False)

