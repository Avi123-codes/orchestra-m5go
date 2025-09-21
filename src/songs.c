#include "songs.h"

// Blue Bells of Scotland - Solo for Part 1
const note_t blue_bells_notes[] = {
    {NOTE_C5, QUARTER_NOTE}, {NOTE_F5, HALF_NOTE}, {NOTE_E5, QUARTER_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_C5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_E5, EIGHTH_NOTE}, {NOTE_F5, EIGHTH_NOTE}, {NOTE_A4, QUARTER_NOTE},
    {REST, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE}, {NOTE_AS4, QUARTER_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_F4, HALF_NOTE}, {NOTE_C5, QUARTER_NOTE},
    {NOTE_F5, HALF_NOTE}, {NOTE_E5, QUARTER_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_C5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, EIGHTH_NOTE},
    {NOTE_F5, EIGHTH_NOTE}, {NOTE_A4, QUARTER_NOTE}, {REST, QUARTER_NOTE},
    {NOTE_A4, QUARTER_NOTE}, {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_AS4, SIXTEENTH_NOTE},
    {NOTE_AS4, EIGHTH_NOTE}, {NOTE_G4, QUARTER_NOTE}, {NOTE_F4, HALF_NOTE}
};

// Carnival of Venice Theme - Solo for Part 3
const note_t carnival_theme_notes[] = {
    {NOTE_C5, EIGHTH_NOTE}, {NOTE_AS4, EIGHTH_NOTE}, {NOTE_A4, QUARTER_NOTE},
    {NOTE_F4, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE}, {NOTE_C5, QUARTER_NOTE},
    {NOTE_F5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, EIGHTH_NOTE},
    {NOTE_F5, EIGHTH_NOTE}, {NOTE_E5, QUARTER_NOTE}, {NOTE_C5, QUARTER_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_B4, QUARTER_NOTE}, {NOTE_C5, HALF_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, HALF_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_D5, QUARTER_NOTE}, {NOTE_C5, HALF_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_F5, EIGHTH_NOTE},
    {NOTE_A4, QUARTER_NOTE}, {REST, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE},
    {NOTE_AS4, QUARTER_NOTE}, {NOTE_G4, QUARTER_NOTE}, {NOTE_F4, HALF_NOTE}
};

// Carnival of Venice Variation 1 - Solo for Part 2
const note_t carnival_var1_notes[] = {
    {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_D5, SIXTEENTH_NOTE}, {NOTE_E5, SIXTEENTH_NOTE}, {NOTE_F5, SIXTEENTH_NOTE},
    {NOTE_G5, SIXTEENTH_NOTE}, {NOTE_A5, SIXTEENTH_NOTE}, {NOTE_F5, SIXTEENTH_NOTE}, {NOTE_E5, SIXTEENTH_NOTE},
    {NOTE_D5, EIGHTH_NOTE}, {NOTE_C5, EIGHTH_NOTE}, {NOTE_F5, QUARTER_NOTE},
    {NOTE_E5, SIXTEENTH_NOTE}, {NOTE_F5, SIXTEENTH_NOTE}, {NOTE_G5, SIXTEENTH_NOTE}, {NOTE_A5, SIXTEENTH_NOTE},
    {NOTE_F5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE},
    {NOTE_C5, QUARTER_NOTE}, {NOTE_AS4, EIGHTH_NOTE}, {NOTE_A4, EIGHTH_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_F4, HALF_NOTE}
};

// Medallion Calls - Solo for Part 4
const note_t medallion_calls_notes[] = {
    {NOTE_C5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_F5, EIGHTH_NOTE},
    {NOTE_F5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE},
    {NOTE_C5, QUARTER_NOTE}, {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, QUARTER_NOTE},
    {NOTE_F5, HALF_NOTE}, {NOTE_G5, QUARTER_NOTE}, {NOTE_A5, QUARTER_NOTE},
    {NOTE_F5, QUARTER_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE},
    {NOTE_C5, HALF_NOTE}, {REST, QUARTER_NOTE}, {NOTE_C5, EIGHTH_NOTE},
    {NOTE_D5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_F5, EIGHTH_NOTE},
    {NOTE_G5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_E5, QUARTER_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_C5, WHOLE_NOTE}
};

// TV Time - Solo for Part 5
const note_t tv_time_notes[] = {
    {NOTE_E5, SIXTEENTH_NOTE}, {NOTE_D5, SIXTEENTH_NOTE}, {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_B4, SIXTEENTH_NOTE},
    {NOTE_C5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE},
    {NOTE_G5, QUARTER_NOTE}, {NOTE_F5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_C5, QUARTER_NOTE}, {NOTE_E5, SIXTEENTH_NOTE},
    {NOTE_D5, SIXTEENTH_NOTE}, {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_B4, SIXTEENTH_NOTE},
    {NOTE_A4, EIGHTH_NOTE}, {NOTE_B4, EIGHTH_NOTE}, {NOTE_C5, QUARTER_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, HALF_NOTE}
};

// Jupiter Hymn - Quintet (simplified, all parts play same melody in harmony)
const note_t jupiter_hymn_notes[] = {
    {NOTE_D5, HALF_NOTE}, {NOTE_G5, HALF_NOTE}, {NOTE_F5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_G5, QUARTER_NOTE},
    {NOTE_E5, HALF_NOTE}, {NOTE_D5, HALF_NOTE}, {NOTE_C5, WHOLE_NOTE},
    {NOTE_D5, HALF_NOTE}, {NOTE_G5, HALF_NOTE}, {NOTE_F5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_G5, QUARTER_NOTE},
    {NOTE_A5, HALF_NOTE}, {NOTE_G5, HALF_NOTE}, {NOTE_F5, WHOLE_NOTE}
};

// Canon in D - Duet (Parts 1&5, then 2&4 play)
const note_t canon_notes[] = {
    {NOTE_D5, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE}, {NOTE_B4, QUARTER_NOTE},
    {NOTE_F4, QUARTER_NOTE}, {NOTE_G4, QUARTER_NOTE}, {NOTE_D4, QUARTER_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_A4, QUARTER_NOTE}, {NOTE_B4, QUARTER_NOTE}, {NOTE_F4, QUARTER_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_D4, QUARTER_NOTE}, {NOTE_G4, QUARTER_NOTE},
    {NOTE_A4, QUARTER_NOTE}, {NOTE_B4, HALF_NOTE}, {NOTE_A4, HALF_NOTE},
    {NOTE_G4, HALF_NOTE}, {NOTE_F4, HALF_NOTE}, {NOTE_E4, WHOLE_NOTE},
    {NOTE_D4, WHOLE_NOTE}
};

// Song definitions
const song_t songs[] = {
    [SONG_JUPITER_HYMN] = {
        .name = "Jupiter Hymn",
        .type = SONG_TYPE_QUINTET,
        .notes = jupiter_hymn_notes,
        .note_count = sizeof(jupiter_hymn_notes) / sizeof(note_t),
        .parts_mask = ALL_PARTS
    },
    [SONG_CANON_IN_D] = {
        .name = "Canon in D",
        .type = SONG_TYPE_DUET,
        .notes = canon_notes,
        .note_count = sizeof(canon_notes) / sizeof(note_t),
        .parts_mask = PART_1 | PART_2 | PART_4 | PART_5
    },
    [SONG_CARNIVAL_THEME] = {
        .name = "Carnival Theme",
        .type = SONG_TYPE_SOLO,
        .notes = carnival_theme_notes,
        .note_count = sizeof(carnival_theme_notes) / sizeof(note_t),
        .parts_mask = ALL_PARTS
    },
    [SONG_CARNIVAL_VAR1] = {
        .name = "Carnival Variation",
        .type = SONG_TYPE_SOLO,
        .notes = carnival_var1_notes,
        .note_count = sizeof(carnival_var1_notes) / sizeof(note_t),
        .parts_mask = ALL_PARTS
    },
    [SONG_BLUE_BELLS] = {
        .name = "Blue Bells",
        .type = SONG_TYPE_SOLO,
        .notes = blue_bells_notes,
        .note_count = sizeof(blue_bells_notes) / sizeof(note_t),
        .parts_mask = ALL_PARTS
    },
    [SONG_MEDALLION_CALLS] = {
        .name = "Medallion Calls",
        .type = SONG_TYPE_SOLO,
        .notes = medallion_calls_notes,
        .note_count = sizeof(medallion_calls_notes) / sizeof(note_t),
        .parts_mask = ALL_PARTS
    },
    [SONG_TV_TIME] = {
        .name = "TV Time",
        .type = SONG_TYPE_SOLO,
        .notes = tv_time_notes,
        .note_count = sizeof(tv_time_notes) / sizeof(note_t),
        .parts_mask = ALL_PARTS
    }
};

const uint8_t total_songs = sizeof(songs) / sizeof(song_t);