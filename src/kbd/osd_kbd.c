/**
 * osd_kbd.c - Keyboard-to-OSD bridge
 *
 * Centralizes all keyboard→OSD interaction:
 *   - Hotkey detection (F11 → OSD menu, F12 → Gotek)
 *   - Virtual button generation (arrows, Enter, ESC)
 *   - Controlled repeat for arrows (independent of PS/2 typematic)
 *   - Hold tracking for parameter acceleration (frequency, etc.)
 *   - ESC/BACK handling (exit tuning, go back, close menu)
 */

#include "osd_kbd.h"

#if defined(KBD_ENABLE) && defined(OSD_ENABLE)

#include "hardware/timer.h"
#include "osd.h"

#ifdef OSD_MENU_ENABLE
#include "osd_menu.h"
#endif

#ifdef OSD_FF_ENABLE
#include "ff_osd.h"
#endif

extern settings_t settings;

// Keyboard repeat timing (matches reduced GPIO timing for consistency)
#define KBD_REPEAT_DELAY_US 400000 // 400ms initial delay before repeat
#define KBD_REPEAT_RATE_US 80000   // 80ms repeat rate

// ── Cross-core volatile state ──────────────────────────────────────────────

volatile uint8_t osd_virtual_buttons = 0; // Edge-triggered events (SEL, BACK)
volatile uint8_t osd_virtual_held = 0;    // Level state: currently held direction keys
volatile bool osd_menu_request = false;
volatile bool ff_osd_request = false;

bool osd_kbd_active = false;

// ── Edge-detection state (Core 1 only) ─────────────────────────────────────

static bool prev_hotkey_menu = false;
static bool prev_hotkey_gotek = false;
static bool prev_enter = false;
static bool prev_esc = false;

// ── Repeat/hold state (Core 0 only) ───────────────────────────────────────

static bool osd_kbd_was_held[3] = {false};    // Previous held state: UP, DOWN, SEL
static uint64_t osd_kbd_hold_start[3] = {0};  // When hold began
static uint64_t osd_kbd_last_repeat[3] = {0}; // Last repeat fire time

// ── Core 1: intercept keyboard state ───────────────────────────────────────

void __not_in_flash_func(osd_kbd_intercept)(kbd_state_t *state)
{
    // OSD_HOTKEY_MENU → toggle OSD menu (edge-triggered)
    bool hotkey_menu = GET_STATE_KEY(*state, OSD_HOTKEY_MENU);
    if (hotkey_menu && !prev_hotkey_menu)
    {
        osd_menu_request = true;
        osd_kbd_active = true;
    }

    prev_hotkey_menu = hotkey_menu;

#ifdef OSD_FF_ENABLE
    // OSD_HOTKEY_GOTEK → toggle Gotek OSD (edge-triggered, blocked while settings menu is open)
    bool hotkey_gotek = GET_STATE_KEY(*state, OSD_HOTKEY_GOTEK);

    if (hotkey_gotek && !prev_hotkey_gotek && !osd_state.menu_active && settings.ff_osd_config.enabled)
        ff_osd_request = true;

    prev_hotkey_gotek = hotkey_gotek;

    bool osd_active = osd_state.menu_active || ff_osd_kbd_active;
#else
    bool osd_active = osd_state.menu_active;
#endif

    if (osd_active)
    {
        osd_kbd_active = true;

        // Track held state for arrows (Core 0 handles repeat timing)
        uint8_t held = 0;
#ifdef OSD_MENU_ENABLE
        bool nav_mode = osd_state.menu_active && !osd_menu_state.tuning_mode;
#else
        bool nav_mode = false;
#endif

        if (GET_STATE_KEY(*state, KEY_UP) || GET_STATE_KEY(*state, nav_mode ? KEY_LEFT : KEY_RIGHT))
            held |= OSD_VIRT_UP;

        if (GET_STATE_KEY(*state, KEY_DOWN) || GET_STATE_KEY(*state, nav_mode ? KEY_RIGHT : KEY_LEFT))
            held |= OSD_VIRT_DOWN;

        if (GET_STATE_KEY(*state, KEY_ENTER))
            held |= OSD_VIRT_SEL;

        osd_virtual_held = held;

        // Enter and ESC: edge-triggered (one action per press)
        bool enter = GET_STATE_KEY(*state, KEY_ENTER);

        if (enter && !prev_enter)
            osd_virtual_buttons |= OSD_VIRT_SEL;

        prev_enter = enter;

        bool esc = GET_STATE_KEY(*state, KEY_ESC);

        if (esc && !prev_esc)
        {
#ifdef OSD_FF_ENABLE
            // If only Gotek keyboard mode is active (no settings menu),
            // ESC deactivates Gotek mode instead of sending BACK
            // (avoids stale BACK that would immediately close the next menu open)
            if (ff_osd_kbd_active && !osd_state.menu_active)
                ff_osd_kbd_active = false;
            else
#endif
                osd_virtual_buttons |= OSD_VIRT_BACK;
        }

        prev_esc = esc;
    }
    else
    {
        osd_kbd_active = false;
        osd_virtual_held = 0;
        // Track state so first press after menu opens is edge-detected
        prev_enter = GET_STATE_KEY(*state, KEY_ENTER);
        prev_esc = GET_STATE_KEY(*state, KEY_ESC);
    }
}

// ── Core 0: inject virtual buttons into osd_buttons ───────────────────────

void osd_kbd_apply_virtual(void)
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

        if (is_held && !osd_kbd_was_held[i])
        {
            // First press edge — fire immediately
            if (i == 0)
                osd_buttons.up_pressed = true;
            else
                osd_buttons.down_pressed = true;

            osd_kbd_hold_start[i] = now;
            osd_kbd_last_repeat[i] = now;

            // Set key_held/key_hold_start so acceleration works
            osd_buttons.key_held[i] = true;
            osd_buttons.key_hold_start[i] = now;
        }
        else if (is_held)
        {
            // Key is held — apply controlled repeat
            uint64_t hold_duration = now - osd_kbd_hold_start[i];
            uint64_t since_repeat = now - osd_kbd_last_repeat[i];
            uint64_t repeat_delay = (hold_duration < KBD_REPEAT_DELAY_US)
                                        ? KBD_REPEAT_DELAY_US
                                        : KBD_REPEAT_RATE_US;

            if (since_repeat >= repeat_delay)
            {
                if (i == 0)
                    osd_buttons.up_pressed = true;
                else
                    osd_buttons.down_pressed = true;

                osd_kbd_last_repeat[i] = now;
            }

            // Keep key_held state updated with original hold start
            osd_buttons.key_held[i] = true;
            osd_buttons.key_hold_start[i] = osd_kbd_hold_start[i];
        }
        else if (osd_kbd_was_held[i])
        {
            // Released — clear keyboard contribution to held state
            osd_buttons.key_held[i] = false;
        }

        osd_kbd_was_held[i] = is_held;
    }
}

#endif // KBD_ENABLE && OSD_ENABLE
