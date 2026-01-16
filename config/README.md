stsAssist – konfiguracja i uruchomienie

stsAssist to aplikacja Qt, która:

scrapuje dane meczowe (Python),

generuje kupony bukmacherskie przy użyciu OpenAI,

zapisuje kupony do plików TXT/CSV,

opcjonalnie wysyła kupon e-mailem (SMTP, TLS),

może działać automatycznie (scheduler).

📁 Struktura projektu (istotne elementy)
stsAssist/
├── config/
│   ├── app.json          # główny plik konfiguracyjny
│   ├── prompt.txt        # prompt dla OpenAI (edytowalny)
│   └── README.md         # opis konfiguracji (ten plik)
├── data/
│   └── sts_premier_league.csv   # dane wejściowe (tworzone przez scraper)
├── coupons/
│   ├── coupon_*.txt
│   └── coupon_*.csv
├── scripts/
│   └── scraper.py
├── config/openai.key     # 🔐 klucz OpenAI (NIE w repo!)
├── config/emailpass.key  # 🔐 hasło SMTP (NIE w repo!)

⚠️ WAŻNE – pliki z sekretami

Pliki poniżej MUSZĄ zostać utworzone ręcznie
i NIE są commitowane do repozytorium (.gitignore):

Plik	Opis
config/openai.key	klucz API OpenAI
config/emailpass.key	hasło aplikacyjne SMTP
🔑 OpenAI – konfiguracja
1️⃣ Utwórz plik:
config/openai.key

2️⃣ Wklej TYLKO klucz API (jedna linia, bez spacji):
sk-xxxxxxxxxxxxxxxxxxxxxxxx


🔒 Nie dodawaj tego pliku do Gita!

✉️ Email (SMTP – Gmail)

Aplikacja obsługuje SMTP z TLS (STARTTLS).

1️⃣ Konto Gmail

Musisz mieć:

włączone 2-step verification

wygenerowane App Password

👉 Google → Security → App passwords → Mail

2️⃣ Utwórz plik:
config/emailpass.key

3️⃣ Wklej App Password (jedna linia):
abcd efgh ijkl mnop


(bez spacji w kodzie – Qt je ignoruje)

📄 Główny plik konfiguracyjny – config/app.json

Przykładowa zawartość:

{
  "paths": {
    "csv": "data/sts_premier_league.csv",
    "coupons_dir": "coupons",
    "scraper_script": "scripts/scraper.py",
    "prompt_file": "config/prompt.txt"
  },

  "openai": {
    "model": "gpt-5.1",
    "max_tokens": 1200
  },

  "coupon": {
    "default_matches": 2,
    "default_budget": 100.0,
    "default_risk": "Normalne"
  },

  "automation": {
    "enabled": false,
    "interval_minutes": 30,
    "interval_days": 0,
    "run_scraper_before_coupon": true,
    "send_email_after_coupon": true
  },

  "email": {
    "enabled": true,
    "smtp_host": "smtp.gmail.com",
    "smtp_port": 587,
    "use_tls": true,
    "from": "stsassist.bot@gmail.com",
    "to": "twojemail@gmail.com"
  }
}

🧠 Prompt – config/prompt.txt

Ten plik zawiera pełny prompt dla OpenAI
i można go edytować bez rekompilacji aplikacji.

Dostępne placeholdery:

{{MATCHES}}

{{RISK}}

{{BUDGET}}

{{CSV_DATA}}

▶️ Uruchomienie aplikacji

1️⃣ Upewnij się, że istnieją:

config/app.json

config/prompt.txt

config/openai.key

config/emailpass.key (jeśli email włączony)

2️⃣ Uruchom aplikację

3️⃣ Kliknij:

Refresh data → scraper

Generate coupon → OpenAI

(opcjonalnie) Auto-generate

Kupony zapiszą się w:

coupons/

🛑 Najczęstsze problemy
❌ „Brak klucza API”

➡️ brak config/openai.key

❌ „SMTP: brak APP PASSWORD”

➡️ brak config/emailpass.key

❌ Brak CSV

➡️ scraper nie został uruchomiony lub padł
