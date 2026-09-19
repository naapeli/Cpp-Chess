"""Exercise the actual frontend, worker, and compiled WebAssembly together."""
from playwright.sync_api import sync_playwright, expect

START = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"


def check_website(url, channel):
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(channel=channel, headless=True)
        try:
            page = browser.new_page(viewport={"width": 1280, "height": 1000})
            errors = []
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.goto(url)
            expect(page.locator("#thinking")).to_have_text("Ready", timeout=20000)
            expect(page.locator(".square")).to_have_count(64)
            expect(page.locator("#error")).to_be_hidden()
            page.select_option("#time", "250")
            page.locator('[data-square="e2"]').click()
            page.locator('[data-square="e4"]').click()
            page.wait_for_function("document.querySelector('#board').dataset.fen.split(' ')[5] === '2'")
            expect(page.locator("#thinking")).to_have_text("Ready")
            expect(page.locator("#error")).to_be_hidden()
            assert page.locator("#depth").inner_text().isdigit()
            page.locator("#undo").click()
            expect(page.locator("#board")).to_have_attribute("data-fen", START)

            def load(fen):
                page.locator("#fen").fill(fen)
                page.locator("#load-fen").click()

            # Zero is valid for the halfmove clock, independently of fullmove.
            zero_clock = "8/5k2/5p2/4pP2/4P3/5K2/8/8 w - - 0 12"
            load(zero_clock)
            expect(page.locator("#board")).to_have_attribute("data-fen", zero_clock)
            expect(page.locator("#error")).to_be_hidden()
            load(zero_clock.replace("0 12", "-1 12"))
            expect(page.locator("#error")).to_be_visible()
            expect(page.locator("#board")).to_have_attribute("data-fen", zero_clock)
            load(START)
            expect(page.locator("#error")).to_be_hidden()
            expect(page.locator("#board")).to_have_attribute("data-fen", START)

            # The C++ validator also rejects positions chess.js can parse.
            load("8/8/8/8/8/8/4k3/4K3 w - - 0 1")
            expect(page.locator("#error")).to_be_visible()
            expect(page.locator("#board")).to_have_attribute("data-fen", START)
            load(START)
            expect(page.locator("#error")).to_be_hidden()

            page.locator("#self-play").click()
            expect(page.locator("#self-play")).to_have_text("Pause self-play")
            page.wait_for_function("Number(document.querySelector('#board').dataset.fen.split(' ')[5]) >= 4", timeout=15000)
            page.locator("#self-play").click()
            expect(page.locator("#thinking")).to_have_text("Ready")
            paused = page.locator("#board").get_attribute("data-fen")
            page.wait_for_timeout(600)
            expect(page.locator("#board")).to_have_attribute("data-fen", paused)

            # Position replacement during a long search must discard its result.
            page.select_option("#time", "10000")
            page.locator("#self-play").click()
            expect(page.locator("#thinking")).to_have_text("Thinking")
            load(zero_clock)
            expect(page.locator("#thinking")).to_have_text("Ready", timeout=3000)
            expect(page.locator("#board")).to_have_attribute("data-fen", zero_clock)
            page.wait_for_timeout(500)
            expect(page.locator("#board")).to_have_attribute("data-fen", zero_clock)
            expect(page.locator("#self-play")).to_have_text("Start self-play")

            promotion = "7k/P7/8/8/8/8/8/7K w - - 0 1"
            load(promotion)
            expect(page.locator("#board")).to_have_attribute("data-fen", promotion)
            page.locator('[data-square="a7"]').click()
            page.locator('[data-square="a8"]').click()
            expect(page.locator("#promotion")).to_be_visible()
            page.locator('[data-promotion="n"]').click()
            expect(page.locator('[data-square="a8"]')).to_have_attribute("aria-label", "a8 White knight")
            expect(page.locator("#status")).to_have_text("Draw by insufficient material.")

            mate = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1"
            load(mate)
            expect(page.locator("#status")).to_have_text("Checkmate. White wins.")
            expect(page.locator("#self-play")).to_be_disabled()
            expect(page.locator("#engine-move")).to_be_disabled()
            page.select_option("#time", "250")
            page.locator("#new-game").click()
            page.select_option("#side", "b")
            page.wait_for_function("document.querySelector('#board').dataset.fen.split(' ')[1] === 'b'")
            expect(page.locator("#thinking")).to_have_text("Ready")
            assert page.locator(".square").first.get_attribute("data-square") == "h1"
            page.select_option("#side", "w")
            expect(page.locator("#thinking")).to_have_text("Ready", timeout=10000)
            page.locator("#new-game").click()
            expect(page.locator("#board")).to_have_attribute("data-fen", START)
            expect(page.locator("#fen")).to_have_value(START)
            page.screenshot(path="build/frontend-desktop.png", full_page=True)
            page.set_viewport_size({"width": 390, "height": 844})
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
            page.screenshot(path="build/frontend-mobile.png", full_page=True)
            assert not errors, errors
            expect(page.locator("#error")).to_be_hidden()
        finally:
            browser.close()
