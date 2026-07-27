#pragma once

#include <string>

namespace audio
{
class Device;
}

namespace gfx
{
namespace renderer
{
class Context;
}
} // namespace gfx

//============================================================================
//
// The scrolling message
//
// Assembled at runtime from what SDL actually reports, so the demo is telling
// you about the machine it is on rather than reciting a constant. On a device
// this is the same information wreel-probe prints, in the form you can read
// without a serial console — which is the point of putting it in a scroller.
//
// Collected here rather than extracted from probe/main.cc. That is a knowing
// trade recorded in planning/2026-07-26-coppers-cracktro/ § 4: two code paths
// will report the same facts and can drift, against the risk of refactoring a
// working validation tool for a demo's convenience.
//
//============================================================================
namespace coppers
{

// Everything the sheet cannot draw is stripped by to_sheet_text() later, so
// this is written in plain prose and upper-cased on the way out.
//
// `device` may be null, meaning the demo is muted. It is passed in rather than
// constructed here, which matters more than it looks: an audio::Device opens
// the mixer in its constructor and calls Mix_CloseAudio in its destructor, so a
// temporary one built just to read the granted spec would open and close the
// mixer underneath music that is already playing. SDL_mixer happens to
// reference-count that (`audio_opened` in mixer.c), so it survives — but only
// while both Devices ask for the same format. Mix_OpenAudio tears the first one
// down outright if the format differs, and nothing in audio::Device's interface
// says otherwise. Not a bug that fired; a trap that was armed.
std::string build_greetz(const gfx::renderer::Context& context, int layer_width,
                         int layer_height, const audio::Device* device);

} // namespace coppers
