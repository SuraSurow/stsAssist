# stsAssist – konfiguracja i uruchomienie

**stsAssist** to aplikacja Qt, która:

- scrapuje dane meczowe (Python)
- generuje kupony bukmacherskie przy użyciu OpenAI
- zapisuje kupony do plików TXT / CSV
- opcjonalnie wysyła kupon e-mailem (SMTP, TLS)
- może działać automatycznie (scheduler)

---

## 📁 Struktura projektu (istotne elementy)

stsAssist/

├── config/

│   ├── app.json

│   ├── prompt.txt

│   └── README.md

├── data

│   └── sts_premier_league.csv

├── coupons/

│   ├── coupon_*.txt

│   └── coupon_*.csv

├── scripts/

│   └── scraper.py

├── config/openai.key

├── config/emailpass.key

---

## WAŻNE – pliki z sekretami

Poniższe pliki MUSZĄ zostać utworzone ręcznie i NIE są commitowane do repozytorium:

- config/openai.key – klucz API OpenAI
- config/emailpass.key – hasło aplikacyjne SMTP

---

## OpenAI – konfiguracja

Utwórz plik:

config/openai.key

Wklej TYLKO klucz API (jedna linia):

sk-xxxxxxxxxxxxxxxxxxxxxxxx

---

## Email (SMTP – Gmail)

Wymagane:
- włączone 2-step verification
- wygenerowane App Password (Google → Security → App passwords)

Utwórz plik:

config/emailpass.key

Wklej App Password (jedna linia).

---

## Główny plik konfiguracyjny – config/app.json

Plik zawiera konfigurację ścieżek, OpenAI, automatyzacji i SMTP.
Może być edytowany bez rekompilacji aplikacji.

---

## Prompt – config/prompt.txt

Plik zawiera prompt dla OpenAI.
Dostępne placeholdery:
- {{MATCHES}}
- {{RISK}}
- {{BUDGET}}
- {{CSV_DATA}}

---

## Uruchomienie aplikacji

1. Upewnij się, że istnieją:
   - config/app.json
   - config/prompt.txt
   - config/openai.key
   - config/emailpass.key (jeśli email włączony)

2. Uruchom aplikację

3. Kliknij:
   - Refresh data
   - Generate coupon

Kupony zapiszą się w katalogu coupons/.

---

## Najczęstsze problemy

Brak klucza API → brak config/openai.key  
Brak hasła SMTP → brak config/emailpass.key  
Brak CSV → scraper nie został uruchomiony
