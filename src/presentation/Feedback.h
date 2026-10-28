#pragma once
namespace woodland {
/**
 * Semantic presentation events, ordered to match Mixer clip slots. They carry no
 * file paths and never influence puzzle resolution. There is no defeat effect.
 */
enum class SoundCue { Selection, Match, Falling, Delivery, Victory };
/**
 * Read-only UI state: Unavailable means no usable clips/device; Muted means the
 * audio backend exists but the session has intentionally silenced its voices.
 */
enum class SoundStatus { On, Muted, Unavailable };
}
