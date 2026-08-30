API: مستندات پروژه 
دو سرور مجزا برای این فاز تهیه شده
پروتکل استفاده شده : HTTP


**پاسخ موفق:**

```json
{
  "ok": true,
  "message": "ok message",
  "data": {  }
}
```

**پاسخ خطا:**

```json
{
  "ok": false,
  "error": "error message"
}
```



## status codes

| code | application |
|----|------|
| `200 OK` | if everything is alright |
| `400 Bad Request` |Invalid arguments/custom error |
| `401 Unauthorized` | Wrong password/No user logged in |
| `403 Forbidden` | Account does not belong to user |
| `404 Not Found` | Not found |
| `408 Not Found` | Timed out |
| `409 Conflict` | Already exists/User already logged in |
| `422 Unprocessable Entity` | Insufficient funds/Account is inactive/Destination account is inactive/Amount must be positive |
| `500 Internal Server Error` | Unknown error |

---

## احراز هویت و نشست‌ها

بسیاری از مسیرها نیاز به توکن نشست دارند. توکن از طریق ورود به سیستم دریافت شده و در هدر `Authorization` به صورت زیر ارسال می‌شود:

```
Authorization: Bearer <token>
```

- توکن‌ها پس از `X` دقیقه عدم فعالیت به‌طور خودکار منقضی می‌شوند.
- با فراخوانی مسیر خروج، توکن به‌صورت فوری باطل می‌شود.
- نشست‌ها در حافظه موقت سرور ذخیره می‌شوند و با ریست سرور همه باطل می‌شوند.

---

# ۱. سرور کاربر (User)

**آدرس پایه:** `http://127.0.0.1:47002`

## ۱.۱. احراز هویت و مدیریت حساب کاربری

### `POST /auth/signup`

ثبت‌نام کاربر جدید.

- **ورودی (JSON):**
  ```json
  {
    "national_code": "1234567890",
    "password": "رمز عبور"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Signup successful."
  }
  ```
- **خطاها:** `400` (کد ملی نامعتبر)، `409` (کاربر تکراری)

---

### `POST /auth/login`

ورود کاربر و دریافت توکن نشست.

- **ورودی (JSON):**
  ```json
  {
    "national_code": "1234567890",
    "password": "رمز عبور"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Login successful.",
    "data": {
      "token": "<توکن>"
    }
  }
  ```
- **خطاها:** `401` (رمز یا کد ملی اشتباه)

---

### `DELETE /auth/session`

خروج از سیستم (باطل کردن توکن فعلی).

- **احراز:** نیاز به توکن
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Logged out."
  }
  ```

---

### `DELETE /users/me`

حذف حساب کاربری جاری (در صورت نداشتن حساب بانکی فعال).

- **احراز:** نیاز به توکن
- **ورودی (JSON):**
  ```json
  {
    "password": "رمز عبور کاربر"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "User deleted."
  }
  ```
- **خطاها:** `403` (دسترسی غیرمجاز)، `409` (کاربر دارای حساب است)، `401` (رمز اشتباه)

---

## ۱.۲. مدیریت درخواست‌های افتتاح حساب

### `POST /accounts/requests`

ثبت درخواست افتتاح حساب در یک شعبه مشخص.

- **احراز:** نیاز به توکن
- **ورودی (JSON):**
  ```json
  {
    "branch_id": 10001
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Request created.",
    "data": {
      "request": {
        "id": 2001,
        "national_code": "1234567890",
        "branch_id": 10001,
        "status": "PENDING",
        "timestamp": "2026-08-16 10:00:00"
      }
    }
  }
  ```
- **خطاها:** `404` (شعبه وجود ندارد)، `409` (درخواست معلق یا فعال قبلی وجود دارد)

---

### `GET /accounts/requests`

دریافت لیست همه درخواست‌های کاربر جاری.

- **احراز:** نیاز به توکن
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "requests": [ {  },  ]
    }
  }
  ```

---

### `DELETE /accounts/requests/{request_id}`

لغو درخواست افتتاح حساب (فقط در وضعیت‌های `PENDING` یا `APPROVED`).

- **احراز:** نیاز به توکن
- **پارامتر مسیر:** `request_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Request cancelled."
  }
  ```
- **خطاها:** `404` (درخواست یافت نشد)، `403` (مالکیت ندارد)، `422` (قابل لغو نیست)

---

### `PATCH /accounts/{account_id}/activation`

فعال‌سازی حسابی که قبلاً توسط ادمین تأیید شده است.

- **احراز:** نیاز به توکن
- **پارامتر مسیر:** `account_id` (شماره حسابی که قرار است فعال شود)
- **ورودی (JSON):**
  ```json
  {
    "request_id": 2001,
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Account activated.",
    "data": {
      "account": {
        "account_number": "5022-...",
        "branch_id": 10001,
        "active": true,
        "balance": 0.0
      }
    }
  }
  ```
- **خطاها:** `404` (درخواست یا حساب یافت نشد)، `403` (مالکیت ندارد)، `422` (درخواست تأیید نشده)

---

## ۱.۳. مدیریت حساب‌های فعال

### `GET /accounts`

مشاهده حساب‌های فعال کاربر جاری.

- **احراز:** نیاز به توکن
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "accounts": [
        {
          "account_number": "5022-...",
          "branch_id": 10001,
          "active": true,
          "balance": 1500.00
        }
      ]
    }
  }
  ```

---

### `DELETE /accounts/{account_id}`

حذف حساب بانکی (فقط در صورت موجودی صفر).

- **احراز:** نیاز به توکن
- **پارامتر مسیر:** `account_id` (شماره حساب)
- **ورودی (JSON):**
  ```json
  {
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Account deleted."
  }
  ```
- **خطاها:** `404` (حساب یافت نشد)، `403` (مالکیت ندارد)، `422` (موجودی مثبت)

---

## ۱.۴. تراکنش‌های مالی

### `POST /accounts/{account_id}/deposits`

واریز وجه به حساب (کاربر باید مالک حساب باشد).

- **احراز:** نیاز به توکن
- **پارامتر مسیر:** `account_id`
- **ورودی (JSON):**
  ```json
  {
    "amount": 1000.00
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Deposit successful.",
    "data": {
      "transaction_id": 1001,
      "new_balance": 1500.00
    }
  }
  ```
- **خطاها:** `404` (حساب یافت نشد)، `403` (مالکیت ندارد)، `422` (مبلغ نامعتبر)

---

### `POST /accounts/{account_id}/withdrawals`

برداشت وجه از حساب (با تأیید رمز).

- **احراز:** نیاز به توکن
- **پارامتر مسیر:** `account_id`
- **ورودی (JSON):**
  ```json
  {
    "amount": 500.00,
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Withdrawal successful.",
    "data": {
      "transaction_id": 1002,
      "new_balance": 1000.00
    }
  }
  ```
- **خطاها:** `401` (رمز اشتباه)، `404` (حساب یافت نشد)، `422` (موجودی ناکافی یا حساب غیرفعال)

---

### `POST /transfers/card-to-card`

انتقال وجه کارت به کارت (کارت مبدأ باید متعلق به کاربر باشد).

- **احراز:** نیاز به توکن
- **ورودی (JSON):**
  ```json
  {
    "from": "5022-...",
    "to": "5022-...",
    "amount": 200.00,
    "password": "رمز عبور حساب مبدأ"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Transfer successful.",
    "data": {
      "transaction_id": 1003,
      "fee": 500.00,
      "new_balance": 800.00
    }
  }
  ```
- **خطاها:** `401` (رمز اشتباه)، `404` (یکی از حساب‌ها یافت نشد)، `422` (موجودی ناکافی، سقف تراکنش، حساب غیرفعال)

---

### `POST /accounts/{account_id}/balance-inquiries`

استعلام موجودی حساب (با کسر کارمزد).

- **احراز:** نیاز به توکن
- **پارامتر مسیر:** `account_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Balance inquiry fee: 200.00",
    "data": {
      "balance": 600.00,
      "fee": 200.00
    }
  }
  ```
- **خطاها:** `404` (حساب یافت نشد)، `403` (مالکیت ندارد)، `422` (موجودی برای کسر کارمزد کافی نیست)

---

### `POST /auth/otp`

درخواست رمز پویا (OTP) برای حساب مشخص.

- **احراز:** نیاز به توکن
- **ورودی (JSON):**
  ```json
  {
    "account_id": "5022-..."
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OTP generated.",
    "data": {
      "code": "123456",
      "expires_in_seconds": 120
    }
  }
  ```
- **خطاها:** `404` (حساب یافت نشد)، `403` (مالکیت ندارد)

---

### `POST /payments/online`

پرداخت آنلاین با استفاده از رمز پویا.

- **احراز:** نیاز به توکن
- **ورودی (JSON):**
  ```json
  {
    "from": "5022-...",
    "to": "5022-...",
    "amount": 1000.00,
    "otp": "123456"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Online payment successful.",
    "data": {
      "transaction_id": 1004,
      "new_balance": 500.00
    }
  }
  ```
- **خطاها:** `401` (OTP نامعتبر یا منقضی)، `404` (حساب مقصد یافت نشد)، `422` (موجودی ناکافی، سقف تراکنش)

---

### `GET /accounts/{account_id}/iban`

دریافت شماره شبا (IBAN) معادل شماره حساب.

- **بدون احراز** (عمومی)
- **پارامتر مسیر:** `account_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "iban": "IR123456789012345678901234"
    }
  }
  ```
- **خطاها:** `404` (حساب یافت نشد)

---

### `POST /transfers/paya`

ثبت درخواست انتقال پایا.

- **احراز:** نیاز به توکن
- **ورودی (JSON):**
  ```json
  {
    "from_account": "5022-...",
    "destination_iban": "IR...",
    "amount": 2000.00,
    "password": "رمز عبور حساب مبدأ"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Paya request registered.",
    "data": {
      "request_id": 5001,
      "status": "PENDING"
    }
  }
  ```
- **خطاها:** `401` (رمز اشتباه)، `404` (حساب مبدأ یا شبا نامعتبر)، `422` (موجودی ناکافی، سقف تراکنش)

---

## ۱.۵. سایر امکانات کاربر

### `GET /users/me/rank`

مشاهده رتبه و امتیاز کاربر جاری.

- **احراز:** نیاز به توکن
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "rank": {
        "national_code": "1234567890",
        "score": 150,
        "level": "نقره‌ای",
        "rank": 1
      }
    }
  }
  ```

---

### `GET /accounts/{account_id}/statement`

دریافت گردش حساب در قالب JSON یا CSV.

- **احراز:** نیاز به توکن
- **پارامتر مسیر:** `account_id`
- **پارامتر کوئری (اختیاری):** `?format=json` یا `?format=csv` (پیش‌فرض `json`)
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "transactions": [
        {
          "id": 1001,
          "timestamp": "2026-08-16 10:00:00",
          "type": "DEPOSIT",
          "amount": "+1000.00",
          "balance_after": 1000.00
        }
      ]
    }
  }
  ```
- **خطاها:** `404` (حساب یافت نشد)، `403` (مالکیت ندارد)

---

# ۲. سرور مدیریت (Admin)

**آدرس پایه:** `http://127.0.0.1:47001`

## ۲.۱. احراز هویت مدیریت

### `POST /administrator/login`

ورود مدیر و دریافت توکن نشست.

- **ورودی (JSON):**
  ```json
  {
    "username": "admin",
    "password": "admin"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Login successful.",
    "data": {
      "token": "<strgin_token>"
    }
  }
  ```
- **خطاها:** `401` (نام کاربری یا رمز اشتباه)

---

### `DELETE /administrator/session` (پیشنهادی)

خروج مدیر و باطل کردن توکن (مشابه کاربران).

---

## ۲.۲. مدیریت شعبه‌ها

### `POST /administrator/branches`

ایجاد شعبه جدید.

- **احراز:** نیاز به توکن مدیر
- **ورودی (JSON):**
  ```json
  {
    "name": "شعبه شمال"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Branch created.",
    "data": {
      "branch": {
        "id": 10001,
        "name": "شعبه شمال"
      }
    }
  }
  ```

---

### `GET /administrator/branches`

مشاهده لیست همه شعبه‌ها.

- **احراز:** نیاز به توکن مدیر
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "branches": [
        { "id": 10001, "name": "شعبه شمال" },
        ...
      ]
    }
  }
  ```

---

### `GET /administrator/branches/{branch_id}/dashboard`

مشاهده آمار و داشبورد یک شعبه خاص.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `branch_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "active_accounts": 5,
      "pending_requests": 2,
      "rejected_today": 0
    }
  }
  ```

---

## ۲.۳. مدیریت حساب‌ها

### `GET /administrator/accounts`

مشاهده تمام حساب‌های سیستم.

- **احراز:** نیاز به توکن مدیر
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "accounts": [ ... ]
    }
  }
  ```

---

### `POST /administrator/accounts`

ایجاد مستقیم حساب (بدون نیاز به درخواست کاربر).

- **احراز:** نیاز به توکن مدیر
- **ورودی (JSON):**
  ```json
  {
    "branch_id": 10001,
    "national_code": "1234567890",
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Account created.",
    "data": {
      "account": { ... }
    }
  }
  ```

---

### `PATCH /administrator/accounts/{account_id}/status`

تغییر وضعیت حساب (فعال/غیرفعال) یا بستن آن.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `account_id`
- **ورودی (JSON):**
  ```json
  {
    "status": "closed",
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Account status updated.",
    "data": { "status": "closed" }
  }
  ```

---

### `DELETE /administrator/accounts/{account_id}`

حذف کامل حساب (همراه با تأیید رمز).

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `account_id`
- **ورودی (JSON):**
  ```json
  {
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Account deleted."
  }
  ```

---

### `POST /administrator/accounts/{account_id}/deposits`

واریز مستقیم وجه به حساب (بدون محدودیت مالکیت).

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `account_id`
- **ورودی (JSON):**
  ```json
  {
    "amount": 1000.00
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Deposit successful.",
    "data": { "transaction_id": 1001, "new_balance": 1500.00 }
  }
  ```

---

### `POST /administrator/accounts/{account_id}/withdrawals`

برداشت مستقیم وجه از حساب.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `account_id`
- **ورودی (JSON):**
  ```json
  {
    "amount": 500.00,
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Withdrawal successful.",
    "data": { "transaction_id": 1002, "new_balance": 1000.00 }
  }
  ```

---

### `POST /administrator/transfers`

انتقال مستقیم وجه بین دو حساب (مدیر).

- **احراز:** نیاز به توکن مدیر
- **ورودی (JSON):**
  ```json
  {
    "from": "5022-...",
    "to": "5022-...",
    "amount": 200.00,
    "password": "رمز عبور حساب مبدأ"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Transfer successful.",
    "data": { "transaction_id": 1003, "new_balance": 800.00 }
  }
  ```

---

### `GET /administrator/accounts/{account_id}/balance`

استعلام موجودی حساب (بدون کسر کارمزد).

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `account_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": { "balance": 800.00 }
  }
  ```

---

### `GET /administrator/accounts/{account_id}/transactions`

مشاهده تاریخچه تراکنش‌های یک حساب.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `account_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "transactions": [ ... ]
    }
  }
  ```

---

## ۲.۴. مدیریت درخواست‌های افتتاح حساب

### `GET /administrator/branches/{branch_id}/account-requests`

مشاهده درخواست‌های افتتاح حساب در یک شعبه خاص.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `branch_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "requests": [ ... ]
    }
  }
  ```

---

### `POST /administrator/account-requests/{request_id}/approve`

تأیید درخواست افتتاح حساب.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `request_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Request approved."
  }
  ```

---

### `POST /administrator/account-requests/{request_id}/reject`

رد درخواست افتتاح حساب با ذکر دلیل.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `request_id`
- **ورودی (JSON) اختیاری:**
  ```json
  {
    "reason": "مدارک ناقص"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Request rejected."
  }
  ```

---

## ۲.۵. مدیریت تراکنش‌های پایا

### `GET /administrator/transfer/paya`

مشاهده همه درخواست‌های انتقال پایا.

- **احراز:** نیاز به توکن مدیر
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "requests": [ ... ]
    }
  }
  ```

---

### `POST /administrator/transfer/paya/{paya_id}/approve`

تأیید و اجرای انتقال پایا.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `paya_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Paya request approved.",
    "data": { "transaction_id": 1004 }
  }
  ```

---

### `POST /administrator/transfer/paya/{paya_id}/reject`

رد درخواست انتقال پایا.

- **احراز:** نیاز به توکن مدیر
- **پارامتر مسیر:** `paya_id`
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Paya request rejected."
  }
  ```

---

## ۲.۶. مدیریت کارمزدها

### `PUT /administrator/fees`

تنظیم کارمزد انتقال یا استعلام موجودی.

- **احراز:** نیاز به توکن مدیر
- **ورودی (JSON):**
  ```json
  {
    "type": "transfer",         
    "amount": 500.00
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "Fee updated.",
    "data": { "transfer_fee": 500.00 }
  }
  ```

---

### `GET /administrator/fees`

مشاهده کارمزدهای فعلی.

- **احراز:** نیاز به توکن مدیر
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "transfer_fee": 500.00,
      "balance_inquiry_fee": 200.00
    }
  }
  ```

---

## ۲.۷. رتبه‌بندی کاربران

### `GET /administrator/rankings`

مشاهده رتبه‌بندی همه کاربران.

- **احراز:** نیاز به توکن مدیر
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "OK",
    "data": {
      "rankings": [
        {
          "national_code": "1234567890",
          "score": 150,
          "level": "silver",
          "rank": 1
        }
      ]
    }
  }
  ```

---

## ۲.۸. عملیات سیستمی

### `DELETE /administrator/history`

پاک‌سازی تاریخچه تراکنش‌های یک حساب خاص.

- **احراز:** نیاز به توکن مدیر
- **ورودی (JSON):**
  ```json
  {
    "account_id": "5022-...",
    "password": "رمز عبور حساب"
  }
  ```
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "History cleared."
  }
  ```

---

### `POST /administrator/system/reset`

بازنشانی کامل سیستم (حذف تمام داده‌ها).

- **احراز:** نیاز به توکن مدیر
- **پاسخ موفق (۲۰۰):**
  ```json
  {
    "ok": true,
    "message": "System fully reset."
  }
  ```

---

## مسیرهای ناشناخته

هر درخواست به مسیری که وجود ندارد یا متد HTTP نامناسبی دارد، باید با کد وضعیت `404 Not Found` و یک پاسخ JSON مانند زیر پاسخ داده شود:

```json
{
  "ok": false,
  "error": "Invalid route."
}
```
