#pragma once

// Container/decoder declarations can describe DTS source precision. AAC and
// AC-3/E-AC-3 placeholder fields must not masquerade as PCM source precision.
inline unsigned OsdAudioDepth(unsigned tag, unsigned reported) {
    if (reported > 64) return 0;
    switch (tag) {
    case 1: case 3: case 0xf1ac: case 0x163: case 0x92: case 0x2001:
        return reported;
    default: return 0;
    }
}
