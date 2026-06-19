# Third-party notices

This repository is an **unofficial, non-commercial, educational port**. It mixes original
work by the repository contributors (MIT-licensed — see [LICENSE](LICENSE)) with several
third-party components that remain under their **own** copyrights and licenses. Those
components are **not** relicensed by this project. This file documents each one.

If you are a rights holder and want something changed or removed, please open an issue —
the maintainers will comply promptly.

---

## 1. Original book source — *Black Art of 3D Game Programming* (André LaMothe, 1995)

The engine modules (`engine/black*.c`, `*.asm`) and chapter demos are a modernized port of
the source from the companion CD of André LaMothe's *Black Art of 3D Game Programming*
(Waite Group Press, 1995).

- **Copyright:** © 1995 André LaMothe / Waite Group Press. The publisher was later absorbed
  into Macmillan Computer Publishing → Pearson; rights today are held by **Pearson plc
  and/or the author**.
- **Status:** **Not** released under any open license. This port is an unofficial educational
  derivative. Receiving source on a book CD did **not** grant redistribution rights, and a
  port is a derivative work — so this repository exists in the spirit of preservation and
  learning, not under a grant. The maintainers will honor any takedown request from the
  rights holder.
- If you want the original material, the book is still findable used and is on archive.org
  ([e-book](https://archive.org/details/BlackArt3DEBook),
  [CD](https://archive.org/details/BlackArtOf3DGameProgramming)) — please support the author.

## 2. DIGPAK digital-sound drivers — John W. Ratcliff / The Audio Solution

- **Files:** the card-specific DIGPAK `.COM` drivers in `audio/DRIVERS` (e.g. `SBPRO.COM`, `SB16.COM`, `ADLIB.COM`). `SOUNDRV.COM` — the digital driver the games load — is **generated** from one of these per machine by the `SETD` setup tool (run `SETUP.BAT`), not shipped pre-built.
- **License:** **MIT** — the author open-sourced DIGPAK in 2021 at
  <https://github.com/jratcliff63367/digpak>. © 1993 John W. Ratcliff.
- The legacy "for maintenance purposes ONLY" text in `audio/DRIVERS/README.PRN` predates and
  is **superseded** by that MIT release.

## 3. AIL synth drivers — John Miles / Miles Design, Inc.

- **Files:** the `.ADV` synth drivers and `.ADD` / `.OPL` / `.BNK` timbre files in
  `audio/DRIVERS` (e.g. `ADLIB.ADV`, `SBFM.ADV`, `MT32MPU.ADV`). MIDPAK is a wrapper around
  John Miles' **Audio Interface Library (AIL)**; these driver files are Miles' work.
- **License:** John Miles released the AIL2 source and drivers as **"open-source freeware"**
  on 2000-05-26 — *"usable by anyone for any purpose, commercial or otherwise, without
  restriction or limitation."* © Miles Design, Inc.
- Refs: <https://github.com/Tronix286/AIL2>, <http://www.ke5fx.com>.

## 4. General MIDI instrument patches — "The Fat Man"

- **Files:** `audio/DRIVERS/MIDPAK.AD`, `audio/DRIVERS/FAT.OPL`.
- **Copyright/credit (required by its original license):**
  *General MIDI patches © 1994 The Fat Man (George Alistair Sanger) and K. Weston Phelan.*
- These were licensed **separately** from DIGPAK/MIDPAK and were **never** released as free
  software. They are retained here only with the credit their license requires. For any reuse
  beyond this port, contact the rights holder (<https://fatman.com>).

## 5. In-game music — Rob Wallace / Wallace Music & Sound

- **Files:** `chap*/BLAZEMUS.XMI` (Starblazer soundtrack), `chap18/KRKMUS.XMI` and
  `ch18_32/KRKMUS.XMI` (Kill or Be Killed soundtrack).
- Composed by **Rob Wallace** for the book and shipped on its CD; © 1993 Wallace Music &
  Sound, Inc. These share the book's copyright status (§1) and are retained as part of the
  book material.

## Not included

The DIGPAK/MIDPAK distribution kit originally shipped **demo music and sound effects**
(`.XMI` / `.SND`) showcasing other companies' games and Rob Wallace's commercial
*Kaleidosonics* product — e.g. Software Toolworks (*Mario's Missing*), Trilobyte/Virgin
(*The 7th Guest*), Electronic Arts (*Seawolf*), and Capstone (*Wayne's World*). That material
is **third-party copyrighted, was not used by this port, and is not included** in this
repository. Only the drivers themselves (§2–§4) and the book's own game music (§5) are kept.
