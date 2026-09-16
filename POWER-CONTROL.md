# Console power control

`OrganPower` handles the supply keep-alive, the console power switch, and an
orderly shutdown of the whole instrument — including a host computer running the
sample engine.

It exists because a retrofit console often does not own its own power. The
Rodgers 760 that prompted it latches its supply through a relay the control
computer has to hold in, and the Raspberry Pi running GrandOrgue sits on an
outlet that same supply switches. Cut the supply while the Pi is writing and you
corrupt its card. So "switch the organ off" is a sequence, not a contact.

**Every part is optional.** A console with its own power switch, no host and no
supply relay sets the sentinels and this module does nothing at all.

---

## The sequence

1. **General cancel**, through the ordinary path, so nothing is left speaking
   while the amplifiers die and the lamps stay consistent with the engine.
2. **`POWER_SHUTDOWN_SYSEX`**, if the console has one — a message to whatever
   else needs telling on the way down.
3. **Lamps shifted out**, so the console goes visibly dark rather than freezing
   with stops apparently drawn.
4. **`POWER_HOST_SHUTDOWN_PIN` driven LOW**, asking the host to halt.
5. **Wait**: either `POWER_HOST_READY_PIN` falling, or `POWER_HOST_HALT_MS`
   elapsing, whichever comes first.
6. **`POWER_KEEPALIVE_PIN` released.** The supply dies and everything
   downstream goes with it.

---

## Keep-alive polarity

`POWER_KEEPALIVE_ACTIVE_HIGH` covers the two ways a console holds its own power
on, and it decides the drive style as well as the level — because that is not a
free choice.

**`true`** — a relay or transistor driven directly, as on a retrofit whose supply
latches through one. Asserted by driving HIGH, released by driving LOW. Push-pull
both ways, which is more positive.

**`false`** — an ATX supply's `PS_ON#`, or anything else with its own pull-up.
Asserted by driving LOW, released by going **hi-Z** and letting that pull-up take
the line.

> **Active-low is always released open-drain, and that matters.** ATX pulls
> `PS_ON#` up to **+5VSB**, and Teensy 4.x pins are **not 5V tolerant**. Releasing
> by driving HIGH would put a 3.3V push-pull output against a 5V rail. If you
> ever implement this polarity by hand elsewhere, release to input, never to
> HIGH.

Either way, an unpowered or resetting board leaves the pin hi-Z, which reads as
released in both polarities. So the supply drops on reset and through the
bootloader, and the console power-cycles on every firmware upload. Correct in
both cases, but worth knowing before you hear it.

### How an ATX console actually works

The momentary power switch is wired **directly across `PS_ON#` to ground**, in
parallel with the controller's open-drain output. Nothing about starting the
supply involves firmware:

1. The organist holds the switch. `PS_ON#` is pulled to ground, the supply comes
   alive, and the rest of the system starts booting.
2. The controller powers up and, in `powerInit()`, drives `PS_ON#` low itself.
3. The organist lets go. The controller is now holding the supply on.
4. On shutdown the controller releases to hi-Z, nothing is pulling `PS_ON#`
   down, the supply's pull-up takes the line, and everything dies.

The switch and the controller are two pull-downs on the same node. Neither ever
drives it high — which is the other reason active-low releases open-drain.

> **Do NOT power the controller from +5VSB.**
>
> It has to die with the supply. Powered from standby, the controller survives
> its own shutdown and sits in the spin at the end of the sequence. It will never
> re-assert `PS_ON#`. The next press then brings the supply up only for as long
> as the organist holds the button, and it collapses the moment they let go —
> one shutdown and the instrument is dead until someone pulls the mains cord.
>
> Power it from something that goes down with the main rails. USB from the host
> computer works, since the host is on those rails. The rule is the same one that
> governs the whole design: **everything dies together, so the next press is a
> cold boot.**

Note that the switch also needs a path to a scan input for `POWER_SWITCH_ADDR`,
since the firmware has to read it to run the shutdown. Starting the supply and
being read by the firmware are two different jobs for the same contact.

---

## Contract values

Add all eight to `ConfigData.cpp`. Omitting one is an undefined reference at
link naming the missing symbol, which is the failure you want.

| Symbol | Type | Disabled value | What it is |
|---|---|---|---|
| `POWER_KEEPALIVE_PIN` | `uint8_t` | `255` | Asserted while the console should stay powered |
| `POWER_KEEPALIVE_ACTIVE_HIGH` | `bool` | — | Which level on that pin means "stay powered" |
| `POWER_SWITCH_ADDR` | `uint16_t` | `ADDR_DISABLED` | The power switch, as an input bit address |
| `POWER_SWITCH_HOLD_MS` | `uint32_t` | — | Continuous press required before acting |
| `POWER_HOST_SHUTDOWN_PIN` | `uint8_t` | `255` | Driven LOW to ask the host to halt |
| `POWER_HOST_READY_PIN` | `uint8_t` | `255` | Read LOW (pulled down by the host) while it can honour a request |
| `POWER_HOST_HALT_MS` | `uint32_t` | — | Backstop for the halt wait |
| `POWER_SHUTDOWN_SYSEX[]` | `uint8_t[]` | — | Framed `F0 … F7` message |
| `POWER_SHUTDOWN_SYSEX_LEN` | `uint16_t` | `0` | Length, 0 for none |

A console with none of this:

```cpp
extern const uint8_t  POWER_KEEPALIVE_PIN      = 255;
extern const bool     POWER_KEEPALIVE_ACTIVE_HIGH = true;
extern const uint16_t POWER_SWITCH_ADDR        = ADDR_DISABLED;
extern const uint32_t POWER_SWITCH_HOLD_MS     = 250;
extern const uint8_t  POWER_HOST_SHUTDOWN_PIN  = 255;
extern const uint8_t  POWER_HOST_READY_PIN     = 255;
extern const uint32_t POWER_HOST_HALT_MS       = 15000;
extern const uint8_t  POWER_SHUTDOWN_SYSEX[]   = { 0 };
extern const uint16_t POWER_SHUTDOWN_SYSEX_LEN = 0;
```

`POWER_SUPPLY_PIN` is gone. It was declared in the contract from 1.0 and read by
no library code at any point. Delete it from your `ConfigData.cpp`.

---

## Sketch integration

Three calls, and one of them has to be first.

```cpp
void setup() {
    powerInit();              // FIRST. Before anything that can block.
    ...
    powerBootComplete();      // LAST.
}

void loop() {
    ...
    powerPoll();              // anywhere in the loop
}
```

`powerInit()` must precede everything because the storage mount, a first-boot
combination format and the startup wait can each block for a long time, and the
supply has to stay up through all of it.

`powerPoll()` is **also called by every blocking screen in the library** — the
startup wait, the config menu, expression calibration, piston assignment. That
is deliberate and it is the point: the organist can switch the console off while
it sits waiting for the sample engine to load, which is exactly when they are
most likely to want to. Any blocking loop added to the library in future must
pump it too.

---

## The arming latch

On the consoles this was built for, the power switch is **also the ON switch**:
holding it bypasses the supply relay, everything comes alive, and the firmware
then holds the relay itself so the organist can let go.

That means the switch is held down throughout `setup()` and still reads pressed
on the first scans. `OrganPower` therefore ignores it until it has been seen
**released** at least once. Without that latch the console shuts down the
instant it finishes booting, which presents as a dead board — because it is one.

Watch for `Power: switch released -- armed` on the debug serial. If that line
never appears, the contact is reading pressed permanently and the polarity is
not what the config says.

---

## The readiness line

**Run this wire.** It is one GPIO and it changes the behaviour twice over.

Asking a host to halt before it is listening is worse than not asking: the
request goes nowhere, the timeout expires anyway, and the console cuts mains
from a machine in the middle of booting — the exact failure the orderly shutdown
exists to prevent. `POWER_HOST_READY_PIN` is the host saying *a shutdown request
will be honoured now*, and while it is not asserted the power switch is ignored.

It also works at the other end. A host that releases the line as it halts is
telling the console the halt has begun, which is a real signal where
`POWER_HOST_HALT_MS` is only a guess. With the wire, the timeout is a backstop
for a host that dies without saying so.

### It is active LOW, and read with a pull-up

The host **pulls the line down** to claim ready, and **releases** it to say
halting. The console reads it with `INPUT_PULLUP`.

That is not a style choice. A bare input floats, and a floating pin reads
whatever is in the air. With an active-high convention and no pull, an unwired,
unpowered or crashed host would report *ready* at random — and a random ready is
the console cutting mains from a machine that was never listening. Active low
with a pull-up collapses every one of those cases to the same reading:

| Situation | Line | Console sees |
|---|---|---|
| Host up, `organ-ready` started | pulled LOW | ready |
| Host still booting | released | not ready |
| Host halting | released | not ready |
| Host crashed or unplugged | released | not ready |
| Wire not fitted | nothing connected | not ready |

Every failure lands on "not ready", which means the power switch is ignored and
`Power: switch ignored -- host not ready yet` appears on the debug serial. A
diagnosable refusal beats a random acceptance.

It also matches the rest of the design, where every signal — the host shutdown
request, an ATX `PS_ON#`, the power switch itself — asserts by pulling to ground.

**Without the wire** (`255`), no pin is read and "ready" means
`powerBootComplete()` has been called. The power switch then does nothing until
`setup()` has finished — which on a console with `STARTUP_WAIT_ENABLED` means
waiting for the sample engine to load — and the shutdown always runs the full
`POWER_HOST_HALT_MS`. Safe, but that is the whole reason to run the wire.

---

## Host setup: Raspberry Pi

Worked example for Raspberry Pi OS on a Pi 5, which is what this was developed
against. Two independent pieces: the shutdown input, and the readiness output.

### Wiring

| Console | Pi |
|---|---|
| `POWER_HOST_SHUTDOWN_PIN` | GPIO3, 40-pin header **pin 5** |
| `POWER_HOST_READY_PIN` (active low, pulled up console-side) | GPIO23, 40-pin header **pin 16** |
| Ground | header pin 6, or shared through USB |

Both boards are 3.3V logic; no level shifting.

The console holds its shutdown pin as an **input** (hi-Z) except when
requesting, when it drives LOW. It never drives it high, so no state of the
console — unpowered, resetting, in the bootloader — can halt the Pi by accident.

> **Do not connect GPIO3 to 3.3V or 5V.** It is a direct SoC pin. The only thing
> that should happen to it is being pulled to ground.

### Part A — the shutdown input

Add to `/boot/firmware/config.txt`, under `[all]` or `[pi5]`:

```
# Console power switch: pulls GPIO3 low to request shutdown
dtoverlay=gpio-key,gpio=3,active_low=1,gpio_pull=up,keycode=148,label=ORGAN_SHUTDOWN
```

Keycode 148 is `KEY_PROG1`, which nothing on a Pi desktop binds.

> **Not `gpio-shutdown`.** That overlay sends the *power* key, which Raspberry Pi
> OS Desktop's session power manager catches and turns into a
> shutdown/reboot/logout dialog that waits for a click — useless on a console
> with no keyboard. Setting `HandlePowerKey=poweroff` in `logind.conf` does not
> help, because the desktop holds a low-level inhibitor lock on that key and
> logind's `Handle*` settings are ignored while such a lock is held.

> **Three silent traps.** `gpio-key` takes `gpio=`; `gpio-shutdown` takes
> `gpio_pin=`; `gpio-poweroff` takes `gpiopin=`. If you are editing an existing
> line you must change **both** the overlay name and the pin parameter — leaving
> `gpio-shutdown` while adding `keycode=` does nothing at all, because that
> overlay has no `keycode` parameter and discards it without an error. Omitting
> `keycode=` makes `gpio-key` default back to the power key. And `config.txt`
> section filters like `[pi4]` apply until the next header, so a line appended
> to the end of the file can land under a filter for a board you do not have.

Install the handler. `triggerhappy` reads input events at system level, before
and independently of any desktop session:

```
sudo apt install -y triggerhappy
```

**Give it permission to power off.** This is the step that bites. Triggerhappy
drops privileges on purpose, so a correctly matched key produces nothing
visible, and the log says:

```
thd[711]: Executing trigger action: /sbin/poweroff
poweroff[1810]: Call to PowerOff failed: Interactive authentication required.
```

Find the user and the real path:

```
systemctl cat triggerhappy | grep ExecStart      # usually --user nobody
command -v poweroff                               # usually /sbin/poweroff
```

Grant exactly that one command:

```
sudo visudo -f /etc/sudoers.d/organ-shutdown
```

```
nobody ALL=(root) NOPASSWD: /sbin/poweroff
```

> Use `visudo`, not a plain editor. It syntax-checks before saving and sets the
> 0440 permissions sudo insists on. A malformed sudoers file can lock you out of
> `sudo` entirely.

Then the trigger:

```
sudo nano /etc/triggerhappy/triggers.d/organ-shutdown.conf
```

```
KEY_PROG1    1    /usr/bin/sudo -n /sbin/poweroff
```

Three whitespace-separated fields: key name, `1` for key-down only, command. The
`-n` means non-interactive, so a wrong sudoers rule fails immediately with a log
line instead of waiting forever on a prompt nobody will answer. The `.conf`
extension is required — without it the file is silently not read.

```
sudo systemctl enable --now triggerhappy
```

### Part B — the readiness output

A systemd oneshot drives GPIO23 high once triggerhappy is up, and low again as
the machine halts. `pinctrl` is the right tool on a Pi 5 — `raspi-gpio` does not
work there — and because it writes the hardware registers rather than holding a
line request, the pin stays put after the unit exits.

```
sudo nano /etc/systemd/system/organ-ready.service
```

```ini
[Unit]
Description=Signal the organ console that this host can accept a shutdown request
After=triggerhappy.service
Requires=triggerhappy.service
DefaultDependencies=no
Before=shutdown.target
Conflicts=shutdown.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/bin/pinctrl set 23 op dl
ExecStop=/usr/bin/pinctrl set 23 ip pn

[Install]
WantedBy=multi-user.target
```

```
sudo systemctl daemon-reload
sudo systemctl enable --now organ-ready.service
pinctrl get 23        # expect: 23: op -- -- | lo
```

`RemainAfterExit=yes` is what makes systemd treat the unit as active after the
one command finishes, which is what causes `ExecStop` to run at shutdown. The
`DefaultDependencies=no` / `Before=shutdown.target` / `Conflicts=shutdown.target`
group is what makes that `ExecStop` run *early* in the shutdown, rather than
after the filesystems have gone. It should signal that the halt has begun, not
that it has already finished — the console's own timeout covers the remainder.

`ExecStop` sets the pin back to an **input with no pull** rather than driving it
high. The console's pull-up then takes the line, which is the same state an
unpowered Pi presents — one behaviour for halting and for dying.

`Requires=triggerhappy.service` means the readiness line is never asserted if the
handler failed to start. That is the behaviour you want: no readiness, no
shutdown request, no mains cut.

### Testing, in order

Each step assumes the one before it passed.

```
cat /proc/bus/input/devices
```

Look for `Name="button@3"` with `Handlers=kbd eventN`. The node is named for the
GPIO, **not** for your `label=` — searching this output for `ORGAN_SHUTDOWN`
finds nothing even when everything is correct. The `B: KEY=` bitmap is printed in
64-bit chunks, least significant last; `100000 0 0` is bit 20 of the 128–191
chunk, key 148, which is what you asked for.

```
sudo apt install -y evtest
sudo evtest /dev/input/eventN
```

Ground header pin 5 to pin 6. Expect a `KEY_PROG1` down event, and the Pi should
halt. Time it: from grounding the pin to the green activity LED going dark and
staying dark. That number sets `POWER_HOST_HALT_MS` — leave generous margin, and
re-measure with the sample engine loaded, not idle.

Then with the console wired, watch its debug serial through a full cycle:

```
Power: switch released -- armed
Power: shutting down
Power: shutdown SysEx sent
Power: host halt requested
Power: host reported halt after NNNN ms
Power: releasing supply
```

`host reported halt after` only appears when the readiness wire is fitted and
working. Its absence means the wait ran the full timeout.

### Recommended: read-only SD card

Once the host is fully configured and the sample cache built, make the root
filesystem read-only: `sudo raspi-config` → **Performance Options** → **Overlay
File System**, answering yes to write-protecting the boot partition too. An
unclean power cut then cannot corrupt anything, because nothing is ever being
written.

Do it **last**. The overlay freezes the system as it stands, and everything
written afterwards — logs, engine settings, new caches, updates — is lost at the
next reboot. Take a full image backup first; the card becomes a known-good
snapshot and having a copy is what makes the arrangement safe.

To change anything after that: disable the overlay, reboot, make the change,
re-enable, reboot. Two reboots around every change, by design.

---

## Troubleshooting

**Console shuts down the instant it powers up.** The arming latch is not seeing a
release. Check `Power: switch released -- armed` on the serial; if it never
appears, the contact reads pressed permanently and the polarity is reversed from
what `POWER_SWITCH_ADDR` assumes.

**Power switch does nothing.** If the debug says `Power: switch ignored -- host
not ready yet`, the readiness line is not asserted: the host has not reached
`organ-ready.service`, or triggerhappy failed and `Requires=` blocked it. Check
`systemctl status organ-ready triggerhappy`. If nothing prints at all, the
switch address is wrong or `powerPoll()` is not being pumped where you are.

**A shutdown/reboot/logout dialog appears and waits.** The power key is still
being sent — `keycode=148` missing, or an old `gpio-shutdown` line still present.

**`evtest` shows `KEY_PROG1` but nothing happens.** The permission step.
`journalctl -u triggerhappy -n 50 --no-pager` will show `Call to PowerOff
failed: Interactive authentication required`.

**Console cuts power before the host finishes.** `POWER_HOST_HALT_MS` too short,
or the readiness line is dropping too early in the shutdown ordering.

**Host shuts down at random.** The console is driving its shutdown pin low when
it should be floating, or there is noise on a long run. A 0.1µF capacitor from
GPIO3 to ground at the Pi end settles it, and `debounce=100` can be added to the
overlay line.

**Blower or other SysEx target does not respond.** `midiOutSysEx()` writes to
usbMIDI and to the pipe mirror both. Confirm the message is complete and framed
`F0 … F7`, that `POWER_SHUTDOWN_SYSEX_LEN` matches the array, and that the
target's manufacturer ID and tag bytes are what it is watching for.
