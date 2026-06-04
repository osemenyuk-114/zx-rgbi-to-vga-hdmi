/**
 * kbd_osd.c - Keyboard-to-OSD bridge
 *
 * Centralizes all keyboard→OSD interaction:
 *   - Hotkey detection (PrtScr → OSD menu, ScrLk → Gotek)
 *   - Virtual button generation (arrows, Enter, ESC)
 *   - Controlled repeat for arrows (independent of PS/2 typematic)
 *   - Hold tracking for parameter acceleration (frequency, etc.)
 *   - ESC/BACK handling (exit tuning, go back, close menu)
 */

#include "kbd_osd.h"

#if defined(KBD_ENABLE) && defined(OSD_ENABLE)

#include "hardware/timer.h"
#include "osd.h"

#ifdef OSD_MENU_ENABLE
#include "osd_menu.h"
#endif

#ifdef OSD_FF_ENABLE
#include "ff_osd.h"
#endif

// Keyboard repeat timing (matches reduced GPIO timing for consistency)
#define KBD_REPEAT_DELAY_US  400000  // 400ms initial delay before repeat
#define KBD_REPEAT_RATE_US    80000  // 80ms repeat rate

// ── Cross-core volatile state ──────────────────────────────────────────────

volatile uint8_t osd_virtual_buttons = 0;  // Edge-triggered events (SEL, BACK)
volatile uint8_t osd_virtual_held = 0;     // Level state: currently held direction keys
volatile bool osd_menu_request = false;
volatile bool osd_gotek_request = false;

bool kbd_osd_active = false;

// ── Edge-detection state (Core 1 only) ─────────────────────────────────────

static bool prev_prtscr = false;
static bool prev_scrlk = false;
static bool prev_enter = false;
static bool prev_esc = false;

// ── Repeat/hold state (Core 0 only) ───────────────────────────────────────

static bool kbd_was_held[3] = {false};         // Previous held state: UP, DOWN, SEL
static uint64_t kbd_hold_start[3] = {0};       // When hold began
static uint64_t kbd_last_repeat[3] = {0};      // Last repeat fire time

// ── Core 1: intercept keyboard state ───────────────────────────────────────

void __not_in_flash_func(kbd_osd_intercept)(kb_state_t *state)
{
    // PrtScr → toggle OSD menu (edge-triggered)
    bool prtscr = GET_STATE_KEY(*state, KEY_PRT_SCR);
    if (prtscr && !prev_prtscr)
    {
        osd_menu_request = true;
        kbd_osd_active = true;
    }
    prev_prtscr = prtscr;

    // ScrLk → toggle Gotek OSD (edge-triggered)
    bool scrlk = GET_STATE_KEY(*state, KEY_SCROLL_LOCK);
    if (scrlk && !prev_scrlk)
    {
        osd_gotek_request = true;
    }
    prev_scrlk = scrlk;

    // When OSD menu is active, route cursor/enter/esc keys
    // ff_osd_display.on only affects GPIO buttons, not keyboard interception.
    // Keyboard→Gotek routing is controlled solely by ff_osd_kbd_active (ScrLk toggle).
    bool osd_active = osd_state.menu_active;
#ifdef OSD_FF_ENABLE
    osd_active |= ff_osd_kbd_active;
#endif

    if (osd_active)
    {
        kbd_osd_active = true;

        // Track held state for arrows (Core 0 handles repeat timing)
        uint8_t held = 0;
        if (GET_STATE_KEY(*state, KEY_UP) || GET_STATE_KEY(*state, KEY_LEFT))
            held |= OSD_VIRT_UP;
        if (GET_STATE_KEY(*state, KEY_DOWN) || GET_STATE_KEY(*state, KEY_RIGHT))
            held |= OSD_VIRT_DOWN;
        if (GET_STATE_KEY(*state, KEY_ENTER))
            held |= OSD_VIRT_SEL;
        osd_virtual_held = held;

        // Enter and ESC: edge-triggered (one action per press)
        bool enter = GET_STATE_KEY(*state, KEY_ENTER);
        if (enter && !prev_enter) osd_virtual_buttons |= OSD_VIRT_SEL;
        prev_enter = enter;

        bool esc = GET_STATE_KEY(*state, KEY_ESC);
        if (esc && !prev_esc) osd_virtual_buttons |= OSD_VIRT_BACK;
        prev_esc = esc;
    }
    else
    {
        kbd_osd_active = false;
        osd_virtual_held = 0;
        // Track state so first press after menu opens is edge-detected
        prev_enter = GET_STATE_KEY(*state, KEY_ENTER);
        prev_esc = GET_STATE_KEY(*state, KEY_ESC);
    }
}

// ── Core 0: inject virtual buttons into osd_buttons ───────────────────────

void kbd_osd_apply_virtual(void)
{
    // Handle edge-triggered SEL from osd_virtual_buttons.
    // Only clear the SEL bit here; BACK is consumed by osd_menu_update().
    if (osd_virtual_buttons & OSD_VIRT_SEL)
    {
        osd_buttons.sel_pressed = true;
        osd_virtual_buttons &= ~OSD_VIRT_SEL;
    }

    // Handle arrow keys with controlled repeat using osd_virtual_held
    uint8_t held = osd_virtual_held;
    uint64_t now = time_us_64();

    // Process UP (index 0) and DOWN (index 1) with repeat logic
    for (int i = 0; i < 2; i++)
    {
        uint8_t bit = (i == 0) ? OSD_VIRT_UP : OSD_VIRT_DOWN;
        bool is_held = (held & bit) != 0;

        if (is_held && !kbd_was_held[i])
        {
            // First press edge — fire immediately
            if (i == 0) osd_buttons.up_pressed = true;
            else        osd_buttons.down_pressed = true;

            kbd_hold_start[i] = now;
            kbd_last_repeat[i] = now;

            // Set key_held/key_hold_start so acceleration works
            osd_buttons.key_held[i] = true;
            osd_buttons.key_hold_start[i] = now;
        }
        else if (is_held)
        {
            // Key is held — apply controlled repeat
            uint64_t hold_duration = now - kbd_hold_start[i];
            uint64_t since_repeat = now - kbd_last_repeat[i];
            uint64_t repeat_delay = (hold_duration < KBD_REPEAT_DELAY_US)
                                    ? KBD_REPEAT_DELAY_US
                                    : KBD_REPEAT_RATE_US;

            if (since_repeat >= repeat_delay)
            {
                if (i == 0) osd_buttons.up_pressed = true;
                else        osd_buttons.down_pressed = true;

                kbd_last_repeat[i] = now;
            }

            // Keep key_held state updated with original hold start
            osd_buttons.key_held[i] = true;
            osd_buttons.key_hold_start[i] = kbd_hold_start[i];
        }
        else if (kbd_was_held[i])
        {
            // Released — clear keyboard contribution to held state
            osd_buttons.key_held[i] = false;
        }

        kbd_was_held[i] = is_held;
    }
}

#endif // KBD_ENABLE && OSD_ENABLE
