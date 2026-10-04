Fixed8 controls: Y early hold, ZL IV repair plus late hold
========================================================

Built from the supplied Fixed6 Loader-only source. The changed C source is
source/loader_diag.c. The copied ARM gates in loader_diag_asm.s are unchanged.

Fixed8 mapping repair
---------------------
Temporary aliases stay above 0x01000000 and have free guard pages.
This fixes the ZL crash-75 reproduction; real hardware is still unverified.

Test sequence
-------------
1. Use this CXI in place of the current diagnostic loader.cxi in your existing
   Loader setup.
2. Press Y before launching CTGP, then launch and wait for the early Y hold.
3. Press ZL while Y is actively holding CTGP before its first instruction.
   ZL now applies both the pre-unpack IV repair and the later hold installer.
4. Press R once to release Y. R restores the Y/Rosalina hook and preserves
   the IV repair and the 0x07000130 target installer for this startup.
5. CTGP runs the scan and decryption. At 0x07000130 the target installer
   self-restores 0x07000130/0x07000134, locates the bounded footer epilogue,
   and installs the existing late sleep gate.
6. At the late hold, use X and the dump menu to obtain a fresh snapshot set.
7. Press R again to restore the footer's E8BDDFFF and release the late gate.

ZL cancellation
---------------
Pressing ZL a second time while Y is still actively holding cancels both
startup writes and the pending late installer. Once the first R releases Y,
startup words are being consumed and ZL cannot cancel them. An extra R during
startup waits for the late gate rather than restoring executing startup code.
ZL without the active Y hold is refused. This combined patch needs the earlier
hold because the 0x07000130 target installer runs after decryption.

Repair details
--------------
  0x07001450: C9A8AD0F -> 30F72D2B (replacement literal)
  0x07001D0C: E021C002 -> E51FC8C4 (LDR ip,[pc,#-0x8C4])
The final IV calculation loads the captured GOOD value. The scan still runs.
Expected post-unpack words: [0x07000100] EA000000, [0x070F24B8] 30F72D2B.
These are predictions for the new hardware test, not new console observations.

The full captured 0x07001000 page is checked with FNV-1a 5093C1D4 and exact
original words before writing. The two slots are normalized for cancellation.
This is a layout fingerprint, not a cryptographic proof of whole-file identity.
Y and ZL handles must refer to the same process. Temporary aliases are removed
immediately. Late cleanup never restores pre-unpack words over runtime code.

Existing other buttons
----------------------
X: Installs the Dump CTGP-7 + MK7 memory and Restore CTGP-7 + MK7 memory menu.
   For the paired repair test, press X at the late hold and save both dumps.
   Restore uses the saved file size; mapping/size failure remains non-fatal.
ZR: Existing launcher signature scan and original 0xEBFFD05E word restoration.
R: Existing X/Y cleanup and launcher word 0xE3E00000, with the staged ZL
   release described above. Both holds wait for release, using the existing
   sleep-loop code. Fixed8 introduces no timed 25-second delay.

Known footer relation
---------------------
Stock 0x07000128 is E59F3064, loading the 0x07000194 literal 0x070F24D8.
The installer searches that base +0x40 through +0x60 for exact E8BDDFFF.
The captured footer site 0x070F2520 is base +0x48.

Limits
------
The real console has not run Fixed8 yet. This repairs the demonstrated CBC-IV
path. It does not establish a fix for the earlier privileged kernel PC-zero
crash or explain the eight other differing runtime state words. The eventual
.3nx port remains the objective after this Loader hardware test.
