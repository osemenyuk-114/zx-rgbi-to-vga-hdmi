#pragma once

uint32_t calculate_settings_crc(const settings_t *);
void reset_settings_to_defaults(settings_t *);
void check_settings(settings_t *);
void load_settings(settings_t *);
void save_settings(settings_t *);