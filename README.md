# Pilot Logbook

A digital pilot logbook in the CASA (Australian) logbook format, for Windows and macOS.
Written in C++ with Qt 6. Each user's logbook is stored on their own computer.

## Features
- Columns match the CASA logbook: Date, Aircraft Type/Reg, PIC, Other Pilot or Crew, Details,
  3 Specialist/Instructor columns (you name them), then the numbered columns (1)–(14):
  SE/ME × ICUS/Dual/Command(/Co-pilot) × Day/Night, (15) Instrument In Flight, (16) Sim.
- **Logbook** tab: every flight with landings, approaches, flight total and **running total**.
- **Pages** tab: the logbook as printed pages (21 lines per page by default), with Totals This Page,
  Totals Brought Fwd From Previous Page, Totals To Date, Total Flying (cols 1–14) and
  Total Aeronautical Experience (cols 1–14 + 16).
- **Aircraft list** (registration + type + SE/ME) used as a dropdown when entering flights.
- **Quick fill**: enter total, night and role, and the right columns are filled based on the aircraft's engine type.
- **Brought forward** totals from a previous paper logbook.
- **Totals** and **Recency** tabs: passenger carriage (day/night), NVFR, IFR approaches, flight review and IPC.
- **Back up / Restore** (File menu), plus automatic weekly backups. CSV export.
- **Settings** (File → Settings, or Preferences on macOS): Light / Dark / System theme, decimal or
  hh:mm times, lines per page, and the Specialist / Instructor column names.

## Google Sheets backup
`File → Google Sheets Backup…` connects the user's own Google account. Every change is then uploaded
automatically (a few seconds later, retrying if offline) to a **"Pilot Logbook backup"** spreadsheet in
their Google Drive. It has four tabs: Flights, Aircraft, Brought Forward and Settings. On a new computer,
connecting the same account offers to restore from that spreadsheet.

- The spreadsheet is a one-way backup. Edits made in Google Sheets aren't imported, and the next backup overwrites them.
- The app only asks for `drive.file` access, so it can see only the spreadsheets it created, never the rest of the user's Drive.
- The sign-in refresh token is stored in the app's settings for the current user (the registry on Windows, a plist on macOS).

### One-time setup (by the app's developer)
Google requires every app that uses its APIs to be registered. This is free.
1. Create a project at <https://console.cloud.google.com>.
2. **APIs & Services → Library**: enable **Google Sheets API** and **Google Drive API**.
3. **Google Auth Platform**:
   - **Branding**: enter the app name ("Pilot Logbook") and a support email.
   - **Audience**: set User type to *External*, then **Publish app** so it's *In production*. In
     *Testing* mode only listed test users can sign in, and their access expires after 7 days.
   - **Data access**: add the scopes `openid`, `.../auth/userinfo.email` and `.../auth/drive.file`.
     None of them are "sensitive", so Google's app review isn't required.
   - **Clients → Create client**: choose type *Desktop app*, then copy the client ID and client secret.
4. Give the build the credentials:
   - **Local builds:** copy `google-oauth.cmake.example` to `google-oauth.cmake` (git-ignored) and paste them in.
   - **GitHub Actions:** add repository secrets `GOOGLE_CLIENT_ID` and `GOOGLE_CLIENT_SECRET`.

For a desktop app the "client secret" isn't really secret, because it ships inside the app. Google
expects this for desktop apps, and sign-in is protected by PKCE instead. Builds without credentials
still work, but the Google option shows as unavailable.

## Where the data lives
| | Logbook | Automatic backups |
|---|---|---|
| Windows | `%APPDATA%\LogBook\Pilot Logbook\logbook.db` | `…\Pilot Logbook\backups\` |
| macOS | `~/Library/Application Support/LogBook/Pilot Logbook/logbook.db` | `…/Pilot Logbook/backups/` |

Uninstalling the app does not delete the logbook. `File → Show Data Folder` opens this folder.
For testing, `LogBook --db <file>` opens a different logbook file.

## Building

### Windows (on this PC)
Requires MSYS2 UCRT64 with `mingw-w64-ucrt-x86_64-qt6-base`, `-cmake` and `-ninja`, and Inno Setup 6.

- `build.bat`: development build → `build\LogBook.exe`
- `packaging\build-windows.bat`: release installer → `dist\PilotLogbook-<version>-Windows-Setup.exe`

### macOS
`packaging/build-macos.sh` builds `dist/PilotLogbook-<version>-macOS.dmg` (Apple Silicon + Intel) on a Mac
with Qt 6, CMake and Ninja. Without a Mac, use GitHub Actions (below).

### GitHub Actions (both platforms)
[`.github/workflows/build.yml`](.github/workflows/build.yml) builds the Windows installer and the macOS
disk image in the cloud.
- **Test build:** on GitHub, open **Actions → Build → Run workflow**, then download the files from the run's *Artifacts*.
- **Release:** bump `VERSION` in `CMakeLists.txt`, commit, then tag and push (`git tag v1.0.1`,
  `git push origin v1.0.1`). A GitHub Release with both downloads is published automatically.

## Unsigned builds
The downloads aren't code-signed yet, so the first launch shows a warning:
- **Windows** (SmartScreen): click *More info → Run anyway*.
- **macOS** (Gatekeeper): try to open the app once, then go to *System Settings → Privacy & Security → Open Anyway*.

To remove these warnings you need a Windows code-signing certificate, and an Apple Developer ID
(US$99/yr) plus notarisation for macOS.

## Changing the column layout
All time columns are defined in [src/Columns.cpp](src/Columns.cpp). The database, tables, entry form
and totals are generated from that list. Any new columns are added to an existing database automatically.
