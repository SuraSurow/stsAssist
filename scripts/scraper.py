from playwright.sync_api import sync_playwright
import pandas as pd
import time
import sys
import os

def scrape_sts_premier_league():
    data = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=False)
        page = browser.new_page()

        # Nagłówek, żeby strona nie blokowała
        page.set_extra_http_headers({
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36"
        })

        page.goto(
            "https://www.sts.pl/zaklady-bukmacherskie/pilka-nozna/anglia/premier-league/1/1/17",
            timeout=90000
        )

        # --- Akceptacja cookies ---
        try:
            page.wait_for_selector('#CybotCookiebotDialogBodyLevelButtonLevelOptinAllowAll', timeout=5000)
            accept_button = page.locator('#CybotCookiebotDialogBodyLevelButtonLevelOptinAllowAll')
            if accept_button.is_visible():
                accept_button.click()
                print(" Cookies zaakceptowane")
                page.wait_for_timeout(1000)
        except:
            print("Brak bannera cookies lub nie udało się kliknąć.")

        time.sleep(3)  # daj czas na załadowanie

        # --- Scrollowanie aż wszystkie mecze się załadują ---
        try:
            page.wait_for_selector("bo-prematch-match-tiles-list", timeout=30000)  # max 30s
        except:
            print(" Nie załadowała się lista meczów.")
            browser.close()
            return []

        # --- Scrollowanie symulując strzałkę w dół ---
        previous_count = 0
        for i in range(100):  # do 100 "strzałek"
            page.mouse.wheel(0, 200)
            page.wait_for_timeout(500)  # poczekaj chwilę na dynamiczne ładowanie
            matches_now = page.locator("a.one-ticket-match-tile-link").count()
            print(f"Scroll {i + 1}, mecze: {matches_now}")
            if matches_now == previous_count:
                break
            previous_count = matches_now
            time.sleep(1)  # daj czas na dynamiczne ładowanie

        # --- Pobranie wszystkich meczów ---
        matches = page.locator("a.one-ticket-match-tile-link").all()
        print(f"Znaleziono wydarzeń: {len(matches)}")

        for m in matches:
            try:
                team1 = m.locator(".one-ticket-match-tile-event-details-desktop__team-home span").text_content().strip()
                team2 = m.locator(".one-ticket-match-tile-event-details-desktop__team-away span").text_content().strip()
                date_text = m.locator(".match-tile-start-time__date").text_content().strip()
                time_text = m.locator(".match-tile-start-time__time").text_content().strip()

                # Kursy 1X2
                odds_elements = [o.strip() for o in m.locator(".odds-button__odd-value").all_text_contents()]
                if len(odds_elements) >= 3:
                    data.append({
                        "Data": date_text,
                        "Godzina": time_text,
                        "Mecz": f"{team1} - {team2}",
                        "1": odds_elements[0],
                        "X": odds_elements[1],
                        "2": odds_elements[2]
                    })

            except Exception as ex:
                print(" Błąd przy meczu:", ex)

        browser.close()

    return data

# --- Pobranie danych ---
matches_data = scrape_sts_premier_league()

# --- Zapis do CSV ---
df = pd.DataFrame(matches_data)
out_path = sys.argv[1] if len(sys.argv) > 1 else "sts_premier_league.csv"
os.makedirs(os.path.dirname(out_path), exist_ok=True) if os.path.dirname(out_path) else None
df.to_csv(out_path, index=False, encoding="utf-8-sig")
print(f"CSV saved to: {out_path} | matches: {len(df)}")

print(f" Zapisano do sts_premier_league.csv, liczba meczów: {len(df)}")
print(df.head())
