# Keepsake User Manual

![Keepsake](../assets/logos/keepsake-logo.png)

Keepsake is a pocket voice notebook. Press a button, speak your thought, tag
it, and Keepsake keeps it organized — transcribed, summarized, and sorted
into Notes, Todos, and Follow-ups — on a screen that sips power and stays
legible even when it's "asleep."

This manual describes the device as it actually behaves today. Keepsake is
built on **Chroninkle** (itself renamed from **Followup**); a device still
running older firmware broadcasts its Wi-Fi setup hotspot under the old name
until it's reflashed.

## Contents

- [At a glance: the buttons](#at-a-glance-the-buttons)
- [First-time setup](#first-time-setup)
- [Recording a thought](#recording-a-thought)
- [The Home screen](#the-home-screen)
- [Notes, Todos, and Follow-up](#notes-todos-and-follow-up)
- [Viewing a recording's details](#viewing-a-recordings-details)
- [Topics](#topics)
- [Books](#books)
- [Settings](#settings)
- [Locking, sleep, and battery](#locking-sleep-and-battery)
- [Shutting down and charging](#shutting-down-and-charging)
- [Tips and things that surprise people](#tips-and-things-that-surprise-people)

## At a glance: the buttons

Keepsake has three physical controls. There is no touchscreen — everything
is driven by these.

| Control | Quick action | Press and hold |
|---|---|---|
| **Record button** | **Back** — exits whatever you're currently inside (an open list, a scroll view, a switch) | **Record** a thought (see below) |
| **Rocker** (tilt up / tilt down / press in) | Tilt up or down to move the selection; press the rocker straight in (**Select**) to select/confirm | Hold a tilt to keep moving in that direction |
| **PWR** | Short press: **lock / unlock** the screen | Hold ~1 second: open a **shut down?** confirmation |

A few things worth knowing up front:

- **The rocker is one control, not three buttons.** Tilting it up or down
  moves your selection; pressing it straight in — referred to as
  **Select** in this manual — selects/confirms.
- **The Record button does double duty.** A quick tap is **Back** — it
  backs out of whatever you're currently inside. Press and *hold* it, and
  it starts recording instead. You don't need to reach for a different
  control to back out of a list versus to record.
- **Holding PWR for a full 6 seconds forces the power off**, no matter what
  the screen is doing. This is a hardware failsafe, not something you
  should need in normal use.
- The screen is e-paper: black-and-white, and it **visibly flashes** on a
  full-screen refresh (opening a new screen, waking up). Smaller updates
  (moving a selection, a checkbox) redraw more quietly. This is normal.

## First-time setup

On first boot, Keepsake says hello (by name, if it was set up with yours) and
walks you through a short introduction:

- the two friends who live there: **Bookworm**, who looks after your books,
  and **Mochi**, who keeps you company;
- the rocker, Back and voice notes, and resting and power;
- Wi-Fi.

Tilt the **rocker** to move through it and press it in to choose. Close it on
the last slide. You can replay it any time from **Settings → Manual**.

If Keepsake already knows a Wi-Fi network (it came with a setup file), the
last slide says you're all set, and Home opens with your welcome note, if
there is one.

Otherwise a **Let's get online** card offers to set up Wi-Fi from your phone:

- **Set up** opens the setup hotspot (below) and the Wi-Fi page that explains
  it.
- **Later** skips it for now; Keepsake works without Wi-Fi, but the clock
  and updates need it.

The welcome note shows once you're back on Home either way.

To set up Wi-Fi by hand, Keepsake opens its own WiFi hotspot named
**`Keepsake-XXXXXX`** (the last few characters are unique to your device)
whenever it doesn't have saved WiFi credentials yet. Connect a phone or laptop to
that hotspot — it's open, no password needed — and your device should
automatically open a setup page (or visit `http://192.168.4.1` yourself).
From there you can:

- **Connect Keepsake to your WiFi** — scan for networks, pick yours, enter
  the password.
- **Add your Gemini API key** — this is what powers transcription and
  summaries. Without it, recordings still save to the SD card, they just
  won't be transcribed.
- **Set your timezone and the current time/date** — needed for accurate
  timestamps on your recordings and for the "Today" grouping on Notes/Todos.

You can come back to WiFi and time settings later from the device itself
(see [Settings](#settings) below — they're the Network and Time pages in
the Settings hub) — the setup portal is only needed the first time, or if
you want to change your Gemini key.

## Recording a thought

This is the core interaction:

1. **Press and hold the Record button.** You'll hear a start cue.
   Recording actually begins fractionally before the cue finishes, so
   your first word isn't cut off — and Keepsake even keeps a rolling
   one-second buffer while the button is held, so a beat of audio right
   before you "properly" started holding it is still caught.
2. **Speak.** Recordings are capped at **10 seconds** — Keepsake is built
   for quick captures, not long dictation. If you keep holding past 10
   seconds, it stops automatically.
3. **Release the Record button.** You'll hear a stop cue, and Keepsake
   immediately plays your recording back to you.
4. **Choose a tag**, from a menu with five options:
   - **Note** — a general thought.
   - **Task** — something to do. (This is what shows up on the **Todos**
     screen — the tag itself is called "Task," even though the screen
     that lists them is called "Todos.")
   - **Idea** — something to mull over later.
   - **Topic** — doesn't save a note at all. Keepsake transcribes what
     you said, distills it into a short name, and adds it as a new
     [topic](#topics) (e.g. hold Record, say "kitchen renovation," pick
     Topic — that project name now exists as a topic you can tag other
     entries with). Nothing is saved to the SD card for this option,
     same as Discard below.
   - **Discard** — throws the recording away. Nothing is saved to the SD
     card if you pick this — that's exactly why the review-then-tag order
     exists: you get to hear it back before deciding.
5. If you tagged it Note, Task, or Idea, and Keepsake has WiFi and a
   Gemini key configured, it transcribes the recording in the
   background — you'll see a brief "Transcribing recording..." message,
   then either a transcript being saved or a note that transcription
   failed. Topic and Discard skip this: Topic transcribes immediately to
   produce its name (see above), and Discard has nothing to transcribe.

**Without a Gemini key** (the default), every recording is simply a voice
note: it saves to the SD card, its details page offers **Play** and reads
"A voice note. Press Play to listen.", and nothing ever shows as waiting.
Topic pages don't offer **Summarize**, and the first-time introduction doesn't
mention AI.

**If you're offline when you record** (with a Gemini key set), the clip still
saves to the SD card — it just gets marked as pending. The moment Keepsake
reconnects to WiFi, it automatically retries every pending recording. A small
badge on the status bar shows how many are waiting.

## The Home screen

Home is Keepsake's dashboard. Near the top you'll see:

- A **task tracker** — "`done`/`total` completed" with a progress bar, or
  "No tasks yet" if you haven't tagged anything as a Task.
- Today's date and a rotating welcome message. When Keepsake was set up with your
  name, it also greets you ("Good evening, Sam"), and on your birthday it says so all
  day.
- If a welcome note was included at setup, it appears once as a card, with Bookworm
  and Mochi saying hello, the first time you reach Home. Press **Close** to put it
  away.
- On special days Home says so all day, and the first time you reach Home that day a
  card with one of Keepsake's two mascots (Bookworm, who reads, and Mochi, a little
  cloud) wishes you well: your birthday, your anniversary, the day of your first date,
  any days added at setup, and Christmas, New Year's Day, Valentine's Day and Halloween.

Below that is the main menu, in this order:

- **Follow up** — anything you've flagged for later, regardless of what
  it's tagged as (badge shows how many).
- **Todos** (badge shows how many) — your Task recordings.
- **Notes** (badge shows how many) — your Note and Idea recordings.
- **Topics** — browse everything tagged with a given [topic](#topics),
  including Gemini-generated summaries.
- **Books** — browse and read `.txt` files from the device's [Books
  folder](#books).

Move between them by tilting the rocker and pressing it in to **Select**.

## Notes, Todos, and Follow-up

These three screens share the same layout: recordings are grouped by the
day they were made. Select a day to open it, and you'll see the individual
recordings inside — each showing a snippet of its transcript (or "Audio
only" if it hasn't been transcribed yet), its time, and its length. Tap the
Record button to back out of an opened day and return to the list of days.

What shows up where:

- **Notes** — everything tagged Note or Idea.
- **Todos** — everything tagged Task.
- **Follow-up** — anything you've flagged as a follow-up, whatever its tag,
  shown with its actual tag labeled on the row.

Selecting an item opens an actions menu. The exact options depend on the
screen:

| Action | Notes | Todos | Follow-up |
|---|:---:|:---:|:---:|
| Play recording *(only if audio still exists)* | ✓ | ✓ | ✓ |
| View details | ✓ | ✓ | ✓ |
| Follow up / Remove follow-up | ✓ | ✓ | — |
| Turn to task | ✓ | — | — |
| Complete / Mark incomplete | — | ✓ | — |
| Complete follow-up | — | — | ✓ |
| Archive now *(only once completed)* | — | ✓ | — |
| Delete | ✓ | ✓ | ✓ |

A couple of notes on these:

- **Turn to task** (on Notes) is how you promote a Note or Idea into a
  Todo after the fact, without re-recording it.
- **Complete follow-up** (on the Follow-up screen) is one-way — it marks
  the item done and clears the follow-up flag, rather than toggling back
  and forth.
- **Delete removes the recording for good** — there's no separate
  "archive vs. delete" distinction here; that's a different feature (see
  next).

### Archiving completed Todos

At the top of the Todos screen is a **Current / Archived** switch. Select
it and tilt the rocker to flip between the two views.

A completed Task doesn't disappear right away — it stays visible (checked
off) in Current for a while, so you can see what you've gotten done.
After it's been completed for a configurable number of days (7 by
default — see [Settings](#settings)), Keepsake automatically **archives**
it: the audio recording is deleted to free up SD card space, but the
transcript text is kept, and the item moves to the Archived view where you
can still read it. You can also archive a completed item immediately with
**Archive now** from its actions menu, rather than waiting.

Un-completing an archived item (via "Mark incomplete") brings it back to
Current — but the deleted audio does not come back.

## Viewing a recording's details

"View details" opens the full transcript for a recording, scrollable if
it's long — select the scroll area and tilt the rocker to move through it
in 10% steps. Below that:

- **Edit topics** opens a checklist of your [topics](#topics) — select
  one to check or uncheck it, then select **Done** to save. Multiple
  topics can be checked at once.
- **Back** returns you to wherever you came from.
- The last button is either **Play** (if a transcript exists) or
  **Transcribe** (if it doesn't yet). If the transcript exists but the
  audio file itself is gone (for example, after archiving, or if you
  removed files over USB), this button disappears entirely rather than
  offering to play nothing.

## Topics

Topics are labels — a project name, say — you can attach to any Note, Idea,
or Todo, independent of its type (see [Recording a
thought](#recording-a-thought) for creating one by voice, and [Manage
topics](#manage-topics) for renaming/deleting one). The Home screen's
**Topics** item is where you browse by them:

1. Select a topic from the list.
2. You'll see everything tagged with it — Notes, Ideas, and Todos mixed
   together — grouped by day, the same way the Notes/Todos/Follow-up
   screens group their own timelines. Each row shows which type it is
   (Note/Idea/Task) so you can tell them apart.
3. Select an entry to open its [Details](#viewing-a-recordings-details).
4. Once the topic has at least one entry, a **Summarize** button appears
   next to **Back**. Select it for a Gemini-written recap of everything
   ever tagged with this topic — unlike Notes/Todos there's no recent-only
   time window, since a topic can be revisited months apart. Press **Get
   summary** to generate one; the result appears in the scrollable area
   above (same 10%-step scrolling as the Details page). Revisiting a
   topic's summary later shows the one you already generated, without
   regenerating it — press **Refresh summary** for a new one. If Gemini
   isn't connected yet, you'll see a prompt to set it up instead.
5. **Back** returns you to the topic list — or, from a topic's summary,
   to that topic's entries.

If a topic has nothing tagged with it yet, you'll see "No entries tagged
with this topic yet" and no Summarize button.

## Books

The Home screen's **Books** item opens the on-device reader for **EPUB** (`.epub`)
books and plain `.txt` files. Copy them onto the SD card's `books` folder (over USB
mass storage, from Settings > Storage) and they'll show up here.

- When you're partway through a book, the top of the Books screen shows it under
  **Continue reading**: its cover (for EPUBs that have one, in black and white),
  title, author and a bar showing how far you've got. The first time a cover is shown
  it can take a moment to prepare; after that it's remembered in a hidden `.cache`
  folder inside `books` (safe to delete).
- Selecting **Books** shows your shelf: every book in that folder **and its
  subfolders**, by title, with how far you've read (for example `Emma - 42%`, or
  `Done`). The book you read most recently is at the top, then the others you've
  started, then the rest A to Z. Tilt the rocker to move between them, and select
  one to open a menu with:
  - **Continue reading** — opens the book where you last left it.
  - **Start from the beginning** — opens the book at the start, overwriting your
    saved place.
  - **Delete** — removes the book (and your saved place for it) from the SD card,
    after a confirmation. There's no undo.
- In the reading view, tilting the rocker turns the page forward (down) or back
  (up). In an EPUB, **holding** a tilt skips by chapter: hold down to jump to the
  start of the next chapter, hold up to go back to the start of this one (or, if
  you're already there, the one before). Each book remembers where you were, so leaving and reopening it picks up
  where you stopped.
- **EPUB books** show their title at the top and, at the bottom, the chapter name,
  the page within that chapter, and how far through the whole book you are (for
  example `Chapter Two - 3/12 - 41%`; a long chapter counts for more of the book
  than a short one). Pages turn straight from one chapter into the
  next. Headings, bold and italic text, lists and scene breaks are shown, and
  pictures appear as a framed caption.
- If a book can't be opened, the reader says why in plain words instead: it's
  locked by the store or library it came from (DRM), it's damaged (try copying it
  again), or it isn't a book Keepsake can read.
- **Press the rocker in** while reading to open the reader menu:
  - **Contents** (EPUB books) lists the chapters, with the one you're in
    highlighted; pick one to jump straight to it.
  - **Add bookmark** marks the page you're on: a small ribbon appears in the top
    right corner. On a bookmarked page the item reads **Remove bookmark** instead.
  - **Bookmarks** (once a book has some) lists them with how far through the book
    each one is and its chapter (or, for `.txt`, the first words of the page). Pick
    one to jump to it. Bookmarks stay put when you change the text size.
  - **Go to...** jumps to a point in the book: pick 0% (the start), 10%, 20% and so
    on up to 100% (the end). The one nearest to where you are is highlighted.
  - **Text size** picks **Small**, **Medium** (the starting size) or **Large** for
    the book text. You stay on the same passage, and the size is used for every
    book (EPUB and `.txt`) until you change it again.
  - **Back to books** returns to the shelf.
  - **Close** goes back to the page.
- Tap the Record button to leave the reading view and return to the book list.
- `.txt` books can be saved in any of the usual ways (UTF-8, the UTF-16 that Windows
  Notepad calls "Unicode", or older Windows/Latin-1 files); Keepsake works out which
  when it opens the book. Accented letters and curly quotes show as their plain
  versions (`cafe`, `"quoted"`) until the fonts gain more characters.
- For `.txt` books you can page back all the way to the start at any time, including
  right after reopening; reopening one you've read a long way into may take a moment
  while it re-figures out where earlier pages started.
- The reader looks in the `books` folder and up to four levels of subfolders, and
  only reads `.epub` and `.txt` files. Files and folders whose names start with a
  dot are ignored. Keepsake keeps a small hidden `.library` file there to remember
  titles and progress; deleting it is harmless, it's rebuilt the next time you open
  Books. Next to each book there's also a `.pos` file (where you are) and, once you
  add one, a `.bm` file (your bookmarks).

## Settings

Settings is a hub, like a phone's settings app: seven headings, each
opening its own page, plus one action below them:

- **Network** — WiFi and Access Point.
- **Saved Wi-Fi** — the networks Keepsake remembers; forget one you no
  longer use.
- **Time** — timezone and clock.
- **Storage** — SD card status and management.
- **Todos** — todo archiving.
- **Topics** — create, rename, and delete topics.
- **About** — firmware version, updates, and acknowledgments.
- **Manual** — replays the first-time onboarding carousel. This one isn't
  a heading — it's a direct action, since there's nothing to configure on
  it.

Select a heading to open its page. Every one of those pages has its own
**Back** button at the bottom that returns you to this Settings hub. The
footer's **Home** icon, on any of them, is different — it jumps straight
back to the Home screen, skipping the hub.

### Network

From the top:

1. **WiFi** — toggle WiFi on or off.
2. **Access Point** — toggle Keepsake's own setup hotspot on or off.
3. A list of nearby networks — each row indicates whether it's open or
   password-protected, its signal strength, and whether it's the one
   you're currently connected to. Select a network, enter its password if
   it needs one (there's a show/hide toggle for what you've typed), and
   press **Connect**. If you're already connected to the highlighted
   network, that same button reads **Disconnect** instead. Use **Scan**
   to refresh the list.

Keepsake remembers up to five networks you've connected to (for example home, a
friend's place and a phone hotspot). It joins the one it used most recently, and if
that one stops answering it tries the others before giving up. When a sixth is added,
the one used longest ago is forgotten.

To see them, open **Settings → Saved Wi-Fi**: the list starts with the most
recent, and the one you're on says "(connected)". Pick one and choose
**Forget** to remove it. Keepsake won't join it again until you set it up
again here. **Close** leaves the list as it is.

If your Wi-Fi password changes, Keepsake notices that the network turned it away and
shows **Let's reconnect** (once per start-up). Choose **Set up** and it opens its own
Wi-Fi, `Keepsake-XXXXXX`: join it from a phone and pick your network again, as in the
first setup. **Later** puts it off. Being out of range doesn't bring this up; Keepsake
simply tries again later.

### Time

Set your timezone from a picker, or enter the date and time by hand in the
fields provided (hour, minute, AM/PM, month, day, year). Press **Sync &
Save** when you're done — it both saves what you entered and re-syncs the
clock.

### Storage

1. Free space and usage percent on the SD card (only shown when a card is
   inserted and mounted).
2. **Enable OTG** — mounts the SD card as a USB drive on a computer you've
   connected via USB-C, so you can pull files off directly. While this is
   active, the card is unavailable to Keepsake itself, and you'll need to
   explicitly **Disable OTG mode** to get it back (there's no accidental
   way out of this while the cable's connected — that's intentional, so
   Keepsake doesn't touch the card while your computer has it mounted).
3. **Format SD** — erases the SD card completely. Keepsake asks you to
   confirm ("Formatting the SD card will erase everything on the card") —
   there's no undo once you confirm.

### Todos

**Archive todos after** — how many days a completed Todo stays in the
active list before auto-archiving (see [Archiving completed
Todos](#archiving-completed-todos)). Choices are 7, 14, 30, 60, or 90
days, or **Never** to turn off automatic archiving entirely (you can
still archive individual items manually). Default is 7 days.

### About

Shows the installed firmware version, how updates are going, and an
acknowledgment: Keepsake is built on Chroninkle by mach1na, which began as
Followup by ALXV Labs. It's free software under the GNU GPL v3.

**Updates** install by themselves. Once a day, when Wi-Fi is around,
Keepsake checks for a newer version and downloads it in the background. It
starts the next time Keepsake goes to sleep. You'll find it on Home when you
come back, with a card saying what's new. Nothing you've saved is touched.

- **Check for updates** looks right away. The line above it says what's
  happening: "Checking...", "Downloading 1.22.0... 40%", "You're up to date."
- When an update is ready, the button becomes **Restart to update** if you'd
  rather not wait.
- With a low battery (under 30%), the daily check waits until you plug in.
  Asking by hand works down to 15%.
- If an update doesn't start properly, Keepsake goes back to the version it
  had by itself, and won't try that update again.

### Manage topics

Create, rename, and delete [topics](#topics) — labels you can use to group
notes, ideas, and todos across a project, independent of their Note/Idea/Todo
type. Three are there from the start (**Home**, **Car**, **Shopping**) so the
list isn't empty on first boot; rename or delete them like any topic you add
yourself. You can also create a topic by voice without coming here at
all — see the **Topic** tag option in [Recording a
thought](#recording-a-thought).

- **New topic** — opens the keyboard to type a name.
- Select an existing topic to **Rename** (opens the keyboard) or
  **Delete** it. Deleting asks you to confirm; anything already
  tagged with that topic simply loses
  it, keeping any other topics it has.

## Locking, sleep, and battery

**Locking**: a short press of PWR locks the screen. Locking also puts the
display straight to sleep — it doesn't wait out the usual inactivity
timer — so a locked device you drop in a pocket or bag goes dark right
away rather than staying lit for a few more minutes. The frozen screen
you'll see while locked is a quick summary of your pending Todos (see
below), not a clock.

While locked, **jostling the device does not wake it** — only a real
button press does. This is deliberate: motion alone (walking, being carried
in a bag) is ignored so a locked device doesn't keep flashing itself awake.

**Unlocking**: press PWR again. Unlike locking, it lands you straight back
on the Home screen in one step rather than needing a second press.

**The lock screen itself** shows, at a glance:
- The current weekday and date.
- A **"TO-DO"** heading — or **"TO-DO - N PENDING"** if you have pending
  Todos — followed by up to five of them (anything flagged as a
  follow-up is shown first). If there are more than five, you'll see a
  "+N more pending" line instead of a longer list. If you're all caught
  up, it just says so.

**Auto-sleep** (when you're not locked, but the device has just been left
alone): after about 3 minutes of no motion, the screen goes to sleep —
it freezes on whatever was last shown rather than blanking, so it still
reads as "resting," not "off" or broken. After about 30 minutes of
continued inactivity, the device goes into a deeper low-power sleep and
disconnects WiFi; press the Record button or PWR to wake it back up.

## Shutting down and charging

**Shutting down**: hold PWR for about a second. A confirmation appears —
"Your device will power off. Do you want to continue?" — with **Cancel**
and **Shut down** options. Confirming powers the device off. If it's
plugged into USB power at the time, it'll actually stay on until you
unplug the cable — shutting down and unplugging together is what fully
powers it off on external power.

As a last resort, **holding PWR continuously for 6 seconds cuts power
immediately**, regardless of what the screen is doing — useful if the
device is ever unresponsive.

**Charging**: plug in via USB-C. A **Charging** card with Mochi having a
snack says how far along it is. It closes by itself after a few seconds, or
when you unplug, and **Close** puts it away sooner. Keepsake works as usual
while it charges. The status bar shows your battery percentage and a charging
mark while power is connected. The percentage is smoothed so it doesn't
jitter: on battery it only goes down, and while charging it only goes up.

**Low battery**: Keepsake reminds you gently, once each time:

- at **20%**, a short note at the bottom of the screen ("Time to charge
  soon");
- at **10%** and **5%**, a card with a sleepy Mochi.

Charging re-arms them. Updates wait for you to plug in when the battery is
below 30%.

## Tips and things that surprise people

- **Recordings are short on purpose** — 10 seconds max. Think "quick
  capture," not dictation.
- **Discard really discards** — nothing touches the SD card until you
  pick a tag other than Discard.
- **"Task" is what "Todos" means** — if you're looking for the tag that
  puts something on your Todos list, it's called Task in the tagging
  menu.
- **Archiving isn't deleting** — an archived Todo's transcript stays
  readable in the Archived view; only its audio goes away, and only after
  it's already been marked complete for a while (or you archive it
  yourself).
- **A locked, sleeping screen isn't broken** — e-paper stays on whatever
  it last drew when it sleeps; that's expected, not a bug.
