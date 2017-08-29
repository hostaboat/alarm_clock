#include "ui.h"
#include "rtc.h"
#include "pit.h"
#include "tsi.h"
#include "types.h"
#include "utility.h"

////////////////////////////////////////////////////////////////////////////////
// UI START ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::UI(void)
{
    if (!valid())
        return;

    _display.brightness(BR_MID);

    // Set initial state
    int8_t rs_pos = _select.position();
    if (rs_pos == 0)
        _state = UIS_SET_CLOCK;
    else if (rs_pos == 1)
        _state = UIS_CLOCK;
    else if (rs_pos == 2)
        _state = UIS_SET_ALARM;
    else
        error();  // This shouldn't happen on startup.  It means the rotary switch is inbetween positions

    if (!_states[_state]->uisBegin())
        error(_state);
}

bool UI::valid(void)
{
    return !_player.disabled() && _player._prev.valid() && _player._next.valid() && _player._play.valid()
        && _player._fs.valid() && _player._audio.valid()
        && _select.valid() && _br_adjust.valid() && _enc_swi.valid()
        && _display.valid() && _lighting.valid() && _beeper.valid();
}

void UI::error(uis_e state)
{
    if (_display.valid())
    {
        Toggle err(500);
        _display.showString("Err");

        while (true)
        {
            if (err.toggled())
            {
                if (err.on())
                    _display.showString("Err");
                else if (_player.error() == Player::ERR_INVALID)
                    _display.showString("P.PLA");
                else if (_player.error() == Player::ERR_GET_FILES)
                    _display.showString("P.FIL");
                else if (_player.error() == Player::ERR_OPEN_FILE)
                    _display.showString("P.OPE");
                else if (_player.error() == Player::ERR_AUDIO)
                    _display.showString("P.AUd");
                else if (!_player._play.valid())
                    _display.showString("PlA");
                else if (!_player._prev.valid())
                    _display.showString("PrE");
                else if (!_player._next.valid())
                    _display.showString("nE");
                else if (!_player._audio.valid())
                    _display.showString("Au");
                else if (!_player._fs.valid())
                    _display.showString("FS");
                else if (!_select.valid())
                    _display.showString("SELE.");
                else if (!_br_adjust.valid())
                    _display.showString("br.Ad.");
                else if (!_enc_swi.valid())
                    _display.showString("EncS.");
                else if (!_beeper.valid())
                    _display.showString("BEEP");
                else if (!_lighting.valid())
                    _display.showString("LEdS");
                else if (state == UIS_SET_ALARM)
                    _display.showString("SE.AL.");
                else if (state == UIS_SET_CLOCK)
                    _display.showString("SE.CL.");
                else if (state == UIS_SET_TIMER)
                    _display.showString("SE.tI");
                else if (state == UIS_CLOCK)
                    _display.showString("CL.");
                else if (state == UIS_TIMER)
                    _display.showString("tI.");
                else if (state == UIS_TOUCH)
                    _display.showString("tUCH");
                else if (state == UIS_SET_LEDS)
                    _display.showString("SE.LE.");
                else if (state == UIS_CNT)
                    _display.showString("Init");
            }
        }
    }

    if (_beeper.valid())
    {
        _beeper.start();
        while (true)
            (void)_beeper.toggled();
    }

    while (true);
}

void UI::process(void)
{
    if (!valid())
        error();

    _player.process();

    __disable_irq();

    ev_e bev = _br_adjust.turn();
    ev_e sev = _enc_swi.turn();
    uint32_t scs = _enc_swi.closeStart(); 
    uint32_t scd = _enc_swi.closeDuration(); 

    __enable_irq();

    uint32_t sct = 0;
    es_e es = ES_NONE;
    ps_e ps = PS_NONE;

    if (scs != 0)
    {
        es = ES_DEPRESSED;
        sct = msecs() - scs;
    }
    else if (scd != 0)
    {
        es = ES_PRESSED;
        ps = pressState(scd);
    }
    else if (sev != EV_ZERO)
    {
        es = ES_TURNED;
    }

    updateBrightness(bev);
    _lighting.paletteNL();

    if (pressing(es, sct))
        return;

    int8_t rs_pos = _select.position();
    ss_e ss = SS_NONE;
    if      (rs_pos == 0) ss = SS_RIGHT;
    else if (rs_pos == 1) ss = SS_MIDDLE;
    else if (rs_pos == 2) ss = SS_LEFT;

    uis_e slast = _state;
    _state = _next_states[slast][ps][ss];

    if (resting(es, _state != slast, bev != EV_ZERO))
        return;

    if (_state != slast)
    {
        if (es == ES_PRESSED)
            es = ES_NONE;

        _states[slast]->uisEnd();

        if (!_states[_state]->uisBegin())
            error(_state);
    }

    switch (es)
    {
        case ES_NONE:
        case ES_DEPRESSED:
            _states[_state]->uisWait();
            break;

        case ES_TURNED:
            _states[_state]->uisUpdate(sev);
            break;

        case ES_PRESSED:
            if (ps == PS_SHORT)
                _states[_state]->uisChange();
            else
                _states[_state]->uisReset();
            break;
    }
}

UI::ps_e UI::pressState(uint32_t msecs)
{
    if (msecs == 0)
        return PS_NONE;
    else if (msecs <= _s_short_press)
        return PS_SHORT;
    else if (msecs <= _s_medium_press)
        return PS_MEDIUM;
    else
        return PS_LONG;
}

bool UI::pressing(es_e encoder_state, uint32_t depressed_time)
{
    // Depressed states
    enum ds_e : uint8_t
    {
        DS_WAIT_MEDIUM,
        DS_MEDIUM,
        DS_WAIT_LONG,
        DS_LONG,
        DS_WAIT_RELEASED,
        DS_RELEASED
    };

    // Display types
    enum dt_e : uint8_t { DT_NONE, DT_1, DT_2, DT_3 };

    static ds_e ds = DS_WAIT_MEDIUM;
    static dt_e display_type = DT_NONE;
    static Toggle t(_s_reset_blink_time);

    auto blink = [&](dt_e dt) -> void
    {
        if (!t.toggled()) return;
        _display.showBars(dt, false, t.on() ? DF_NONE : DF_BL);
    };

    if ((encoder_state != ES_DEPRESSED) && (ds != DS_WAIT_MEDIUM))
        ds = DS_RELEASED;

    ps_e ps = pressState(depressed_time);

    switch (ds)
    {
        case DS_WAIT_MEDIUM:
            if (ps <= PS_SHORT)
                break;

            ds = DS_MEDIUM;
            // Fall through
        case DS_MEDIUM:
            if (_state == UIS_CLOCK)
            {
                _lighting.toggleNL();
                t.disable();

                ds = DS_WAIT_RELEASED;
            }
            else
            {
                display_type = DT_1;
                t.reset();

                switch (_state)
                {
                    case UIS_SET_ALARM:
                    case UIS_SET_CLOCK:
                    case UIS_SET_TIMER:
                        ds = DS_WAIT_LONG;
                        break;
                    default:
                        ds = DS_WAIT_RELEASED;
                        break;
                }
            }
            break;

        case DS_WAIT_LONG:
            if (ps == PS_MEDIUM)
                blink(display_type);
            else
                ds = DS_LONG;
            break;

        case DS_LONG:
            display_type = DT_2;
            ds = DS_WAIT_RELEASED;
            break;

        case DS_WAIT_RELEASED:
            blink(display_type);
            break;

        case DS_RELEASED:
            ds = DS_WAIT_MEDIUM;
            display_type = DT_NONE;
            t.reset();
            if (_state != UIS_CLOCK)
                _display.blank();
            break;
    }

    return (ds != DS_WAIT_MEDIUM);
}

bool UI::resting(es_e encoder_state, bool ui_state_changed, bool brightness_changed)
{
    static uint32_t rstart = msecs();
    static bool resting = false;

    bool wake = (encoder_state != ES_NONE) || ui_state_changed || brightness_changed;

    if (wake || !_states[_state]->uisSleep())
    {
        if (resting)
        {
            _display.wake();
            _lighting.on();
            resting = false;
        }

        rstart = msecs();
    }
    else if (!resting && ((msecs() - rstart) >= _s_rest_time))
    {
        _display.sleep();
        _lighting.off();
        resting = true;
    }

    if (!resting || _player.running())
        return resting;

    // Resting is true and the player isn't running - can go to sleep

    // Puts MCU in LLS (Low Leakage Stop) mode - a state retention power mode
    // where most peripherals are in state retention mode (with clocks stopped).
    // Current is reduced by about 44mA.  With the VS1053 off (~14mA) should get
    // a total of ~58mA savings putting things ~17mA which should be about what
    // the LED on the PowerBoost is consuming and the Neopixel strip which
    // despite being "off" still consumes ~7mA.

    // Don't like this.  Should have a way to just pass in the device.
    static PinIn < PIN_EN_SW > & ui_swi = PinIn < PIN_EN_SW >::acquire();
    static PinInPU < PIN_EN_A > & ui_enc = PinInPU < PIN_EN_A >::acquire();
    static PinInPU < PIN_RS_L > & ui_rs = PinInPU < PIN_RS_L >::acquire();
    static PinInPU < PIN_EN_BR_A > & br_enc = PinInPU < PIN_EN_BR_A >::acquire();
    static PinIn < PIN_PLAY > & play = PinIn < PIN_PLAY >::acquire();

    _llwu.enable();

    if (!_llwu.pinsEnable(
                ui_swi, IRQC_INTR_FALLING,
                ui_enc, IRQC_INTR_CHANGE,
                ui_rs, IRQC_INTR_CHANGE,
                br_enc, IRQC_INTR_CHANGE,
                play, IRQC_INTR_FALLING))
    {
        _llwu.disable();
        return resting;
    }

    // Tell the clock that it needs to read the TSR counter since NVIC is disabled
    _rtc.sleep();

    (void)_llwu.sleep();
    int8_t wakeup_pin = _llwu.wakeupPin();

    _llwu.disable();

    _rtc.wake();

    if (wakeup_pin == PIN_PLAY)
    {
        _player.play();
    }
    else
    {
        _display.wake();
        _lighting.on();

        resting = false;
        rstart = msecs();
    }

    return resting;
}

void UI::updateBrightness(ev_e bev)
{
    uint8_t brightness = _brightness;
    evUpdate < uint8_t > (brightness, bev, 0, 255, false);

    if (brightness == _brightness)
        return;

    if (brightness > _brightness)
    {
        if ((_brightness == 0) || ((brightness % 16) == 0) || (brightness == 255))
            _display.up();

        _lighting.up();
    }
    else
    {
        if ((brightness == 0) || ((brightness & 0x0F) == 0x0F))
            _display.down();

        _lighting.down();
    }

    _brightness = brightness;
}

////////////////////////////////////////////////////////////////////////////////
// UI END //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Lighting START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::Lighting::Lighting(uint8_t brightness)
{
    uint32_t color_code;
    if (_eeprom.getLedsColor(color_code))
        _nl_color = color_code;

    _leds.updateColor(CRGB::BLACK);
    _leds.setBrightness(brightness);
}

void UI::Lighting::onNL(void)
{
    if (_state != LS_NIGHT_LIGHT)
        return;

    _nl_on = true;

    if (_nl_index == 0)
        _leds.updateColor(_nl_color);
    else
        _leds.startPalette(static_cast < Palette::pal_e > (_nl_index - 1));
}

void UI::Lighting::offNL(void)
{
    if (_state != LS_NIGHT_LIGHT)
        return;

    _nl_on = false;
    _leds.updateColor(CRGB::BLACK);
}

void UI::Lighting::toggleNL(void)
{
    if (_state != LS_NIGHT_LIGHT)
        return;

    if (isOnNL()) offNL();
    else onNL();
}

bool UI::Lighting::isOnNL(void) const
{
    return (_state == LS_NIGHT_LIGHT) && _nl_on;
}

void UI::Lighting::paletteNL(void)
{
    if ((_state != LS_NIGHT_LIGHT) || !_nl_on || (_nl_index == 0))
        return;

    _leds.updatePalette();
}

void UI::Lighting::cycleNL(ev_e ev)
{
    if ((_state != LS_NIGHT_LIGHT) || !_nl_on || (ev == EV_ZERO))
        return;

    evUpdate < uint8_t > (_nl_index, ev, 0, Palette::PAL_CNT);

    if (_nl_index == 0)
        _leds.updateColor(_nl_color);
    else
        _leds.startPalette(static_cast < Palette::pal_e > (_nl_index - 1));
}

void UI::Lighting::setNL(CRGB const & c)
{
    _nl_color = c; _nl_index = 0;
    (void)_eeprom.setLedsColor(c.colorCode());
}

void UI::Lighting::setColor(CRGB const & c)
{
    if ((_state == LS_NIGHT_LIGHT) && !_nl_on)
        return;

    _leds.updateColor(c);
}

void UI::Lighting::up(void)
{
    if (_brightness == 255) return;
    setBrightness(_brightness + 1);
}

void UI::Lighting::down(void)
{
    if (_brightness == 0) return;
    setBrightness(_brightness - 1);
}

void UI::Lighting::setBrightness(uint8_t b)
{
    if (_leds.isClear())
        _leds.setBrightness(b);
    else
        _leds.updateBrightness(b);

    _brightness = b;
}

void UI::Lighting::state(ls_e state)
{
    if (_state == state)
        return;

    ls_e lstate = _state;
    _state = state;

    if ((lstate == LS_TIMER) && (state == LS_NIGHT_LIGHT))
    {
        if (_nl_on) onNL();
        else offNL();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Lighting END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Player START ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::Player::Player(void)
{
    if (!_prev.valid() || !_play.valid() || !_next.valid()
            || !_audio.valid() || !_fs.valid() || !_eeprom.valid())
    {
        _error = ERR_INVALID;
        return;
    }

    int file_cnt = _fs.getFiles(_files, _s_max_files, _exts);
    if (file_cnt <= 0)
    {
        _error = ERR_GET_FILES;
        return;
    }

    _num_files = (uint16_t)file_cnt;

    if (!_eeprom.getFileIndex(_file_index) || (_file_index >= _num_files))
    {
        _file_index = 0;
        _eeprom.setFileIndex(_file_index);
    }

    _file = _fs.open(_files[_file_index]);
    if (_file == nullptr)
    {
        _error = ERR_OPEN_FILE;
        return;
    }
}

void UI::Player::play(void)
{
    if (disabled() || playing())
        return;

    if (!running() && !_file->rewind())
    {
        _error = ERR_OPEN_FILE;
        return;
    }

    start();
}

bool UI::Player::occupied(void) const
{
    return playing() || _play.closed() || _prev.closed() || _next.closed();
}

void UI::Player::process(void)
{
    static uint32_t sstart = msecs();

    if (disabled() || _play.closed() || _prev.closed() || _next.closed())
        return;

    __disable_irq();

    uint32_t play_ctime = _play.closeDuration();
    uint32_t prev_ctime = _prev.closeDuration();
    uint32_t next_ctime = _next.closeDuration();

    __enable_irq();

    uint32_t m = msecs();

    // If the audio isn't on and a button was pressed or soft play is enabled
    // start the audio.
    if (!running() && (play_ctime || prev_ctime || next_ctime))
    {
        play();
        play_ctime = prev_ctime = next_ctime = 0;
    }
    else if (play_ctime != 0)
    {
        if (play_ctime >= _s_stop_time)
            stop();
        else
            _paused = !_paused;

        sstart = m;
    }

    if (playing())
    {
        sstart = m;
    }
    else
    {
        if (paused() && ((m - sstart) >= _s_sleep_time))
            _audio.stop();

        return;
    }

    auto skip = [](uint32_t t) -> int16_t
    {
        if (t == 0) return 0;
        else if ((t >> 10) == 0) return 1;

        uint32_t val = 0, i = 10;
        uint32_t const end = 8;

        for (; i > end; i--)
        {
            t >>= i;
            uint32_t tmp = (t > 10) ? 10 : t;
            val += tmp;
            t -= tmp;
            if (t == 0) break;
            t <<= i;
        }

        val += (t >> i) + 1;
        if (val > INT16_MAX) val = INT16_MAX;
        return val;
    };

    if (_file->eof() || (prev_ctime != 0) || (next_ctime != 0))
    {
        int16_t inc = 1;

        if (!_file->eof())
        {
            _audio.cancel(_file);
            inc = skip(next_ctime) - skip(prev_ctime);
        }

        if (!newTrack(inc))
        {
            _error = ERR_OPEN_FILE;
            return;
        }
    }

    if (_audio.ready() && !_audio.send(_file, 32) && !_file->valid())
        _error = ERR_AUDIO;
}

bool UI::Player::newTrack(int16_t inc)
{
    if (inc == 0)
        return true;

    uint32_t file_size = _files[_file_index].size;

    // If at least 1/4 of the song played save index to EEPROM
    // and just rewind it, i.e. decrement number of tracks.
    if ((file_size - _file->remaining()) > (file_size >> 2))
    {
        (void)_eeprom.setFileIndex(_file_index);  // Eeprom will only write if value is different
        if (inc < 0)
            inc++;  // Do rewind of current
    }

    int16_t new_index = (_file_index + inc) % _num_files;
    if (new_index == _file_index)
        return _file->rewind();
    else if (new_index < 0)
        _file_index = (uint16_t)(_num_files + new_index); // Actually a subtraction
    else
        _file_index = (uint16_t)new_index;

    _file->close();

    _file = _fs.open(_files[_file_index]);
    if (_file == nullptr)
        return false;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Player END //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Alarm START /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void UI::Alarm::begin(void)
{
    _state = AS_OFF;
    _ui._rtc.getAlarm(_alarm);
    _ui._eeprom.getTouchThreshold(_touch_threshold);
    _alarm_time = _alarm.time * 60 * 60 * 1000;
    _snooze_time = _alarm.snooze * 60 * 1000;
    _wake_time = _alarm.wake * 60 * 1000;
}

void UI::Alarm::update(uint8_t hour, uint8_t minute)
{
    if (!_alarm.enabled)
        return;

    if (_state == AS_OFF)
    {
        check(hour, minute);
    }
    else if (_state == AS_ON)
    {
        // Stop alarm after it's been in progress for some time or if it's been
        // actively on for more than some time. If user's been snoozing it for
        // a while they're not getting up.  And if it's beeping or playing for
        // more than a reasonable amount of time, it's likely that one forgot
        // to turn it off.
        if (((_alarm_time != 0) && ((msecs() - _alarm_start) > _alarm_time))
                || ((_wake_time != 0) && ((msecs() - _wake_start) > _wake_time)))
        {
            stop();
            return;
        }

        if (!snooze() && !_alarm_music)
            (void)_ui._beeper.toggled();
    }
    else if (_state == AS_SNOOZE)
    {
        wake();
    }
    else if (_state == AS_DONE)
    {
        done(hour, minute);
    }
}

bool UI::Alarm::enabled(void)
{
    return _alarm.enabled;
}

bool UI::Alarm::inProgress(void)
{
    return (_alarm.enabled && ((_state == AS_ON) || (_state == AS_SNOOZE)));
}

void UI::Alarm::check(uint8_t hour, uint8_t minute)
{
    if ((hour != _alarm.hour) || (minute != _alarm.minute))
        return;

    if (Rtc::alarmIsMusic(_alarm.type)
            && !_ui._player.disabled() && !_ui._player.occupied())
    {
        _ui._player.play();
        _alarm_music = true;
    }
    else
    {
        _ui._beeper.start();
        _alarm_music = false;
    }

    _alarm_start = _wake_start = msecs();
    _state = AS_ON;
}

bool UI::Alarm::snooze(bool force)
{
    if (!inProgress())
        return false;

    if (!force && (_ui._tsi_channel.read() < _touch_threshold))
        return false;

    if (_alarm_music)
        _ui._player.pause();
    else
        _ui._beeper.stop();

    _snooze_start = msecs();
    _state = AS_SNOOZE;

    return true;
}

void UI::Alarm::wake(void)
{
    if ((msecs() - _snooze_start) < _snooze_time)
        return;

    if (_alarm_music)
        _ui._player.play();
    else
        _ui._beeper.start();

    _wake_start = msecs();
    _state = AS_ON;
}

void UI::Alarm::stop(void)
{
    if (_alarm_music)
        _ui._player.stop();
    else
        _ui._beeper.stop();

    _state = AS_DONE;
}

void UI::Alarm::done(uint8_t hour, uint8_t minute)
{
    if ((hour != _alarm.hour) || (minute != _alarm.minute))
        _state = AS_OFF;
}
////////////////////////////////////////////////////////////////////////////////
// Alarm END ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SetAlarm START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool UI::SetAlarm::uisBegin(void)
{
    _ui._rtc.getAlarm(_alarm);
    _state = _alarm.enabled ? SAS_HOUR : SAS_DISABLED;
    _blink.reset(500);
    _updated = _done = false;
    display();

    return true;
}

void UI::SetAlarm::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::SetAlarm::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    _updated = true;
    MFC(_update_actions[_state])(ev);
    display();
}

void UI::SetAlarm::uisChange(void)
{
    // Enable if disabled
    if (_state == SAS_DISABLED)
    {
        _alarm.enabled = true;
        (void)_ui._rtc.setAlarm(_alarm);
    }

    _state = _next_states[_state];

    if (_state == SAS_DONE)
    {
        (void)_ui._rtc.setAlarm(_alarm);
        _done = true;
        _updated = false;
        _blink.reset(1000);
    }
    else
    {
        _blink.reset(500);
    }

    display();
}

void UI::SetAlarm::uisReset(void)
{
    toggleEnabled();
    display();
}

void UI::SetAlarm::uisEnd(void)
{
    if (_updated)
        (void)_ui._rtc.setAlarm(_alarm);

    _ui._display.blank();
}

bool UI::SetAlarm::uisSleep(void)
{
    return true;
}

void UI::SetAlarm::waitHour(bool on)
{
    display(on ? DF_NONE : DF_BL_BC);
}

void UI::SetAlarm::waitMinute(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetAlarm::waitType(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetAlarm::waitSnooze(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetAlarm::waitWake(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetAlarm::waitTime(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetAlarm::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetAlarm::updateHour(ev_e ev)
{
    evUpdate < uint8_t > (_alarm.hour, ev, 0, 23);
}

void UI::SetAlarm::updateMinute(ev_e ev)
{
    evUpdate < uint8_t > (_alarm.minute, ev, 0, 59);
}

void UI::SetAlarm::updateType(ev_e unused)
{
    _alarm.type = Rtc::alarmIsMusic(_alarm.type) ? Rtc::alarmBeep() : Rtc::alarmMusic();
}

void UI::SetAlarm::updateSnooze(ev_e ev)
{
    evUpdate < uint8_t > (_alarm.snooze, ev, 1, 59);
}

void UI::SetAlarm::updateWake(ev_e ev)
{
    evUpdate < uint8_t > (_alarm.wake, ev, 0, 59);
}

void UI::SetAlarm::updateTime(ev_e ev)
{
    evUpdate < uint8_t > (_alarm.time, ev, 0, 23);
}

void UI::SetAlarm::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::SetAlarm::displayClock(df_t flags)
{
    _ui._display.showClock(_alarm.hour, _alarm.minute,
            flags | (_ui._rtc.clockIs12H() ? DF_12H : DF_24H));
}

void UI::SetAlarm::displayType(df_t flags)
{
    Rtc::alarmIsMusic(_alarm.type) ? _ui._display.showPlay(flags) : _ui._display.showBeep(flags);
}

void UI::SetAlarm::displaySnooze(df_t flags)
{
    _ui._display.showOption("SN", _alarm.snooze, flags);
}

void UI::SetAlarm::displayWake(df_t flags)
{
    _ui._display.showOption("ON", _alarm.wake, flags);
}

void UI::SetAlarm::displayTime(df_t flags)
{
    _ui._display.showOption("A.T.", _alarm.time, flags);
}

void UI::SetAlarm::displayDone(df_t flags)
{
    static constexpr uint8_t const nopts = 5;
    static uint32_t i = 0;

    if (_done) { i = 0; _done = false; }
    else if (flags == DF_NONE) i++;
    else return;

    switch (i % (nopts + 1))
    {
        case 0: displayClock(); break;
        case 1: displayType(); break;
        case 2: displaySnooze(); break;
        case 3: displayWake(); break;
        case 4: displayTime(); break;
        default: _ui._display.showString("... ");
    }
}

void UI::SetAlarm::displayDisabled(df_t flags)
{
    _ui._display.showOff(flags);
}

void UI::SetAlarm::toggleEnabled(void)
{
    _alarm.enabled = !_alarm.enabled;
    _state = _alarm.enabled ? SAS_HOUR : SAS_DISABLED;
    (void)_ui._rtc.setAlarm(_alarm);
}

////////////////////////////////////////////////////////////////////////////////
// SetAlarm END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SetClock START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool UI::SetClock::uisBegin(void)
{
    (void)_ui._rtc.update();
    _ui._rtc.getClock(_clock);
    _clock.second = 0;

    _state = SCS_TYPE;

    _blink.reset(500);
    _updated = _done = false;
    display();

    return true;
}

void UI::SetClock::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::SetClock::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    _updated = true;
    MFC(_update_actions[_state])(ev);
    display();
}

void UI::SetClock::uisChange(void)
{
    _state = _next_states[_state];

    if (_state == SCS_DONE)
    {
        (void)_ui._rtc.setClock(_clock);
        _done = true;
        _updated = false;
        _blink.reset(1000);
    }
    else
    {
        _blink.reset(500);
    }

    display();
}

void UI::SetClock::uisReset(void)
{
    // This won't be called since the state will be changed to timer mode
    _ui._display.blank();
}

void UI::SetClock::uisEnd(void)
{
    if (_updated)
        (void)_ui._rtc.setClock(_clock);

    _ui._display.blank();
}

bool UI::SetClock::uisSleep(void)
{
    return true;
}

void UI::SetClock::waitType(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetClock::waitHour(bool on)
{
    display(on ? DF_NONE : DF_BL_BC);
}

void UI::SetClock::waitMinute(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetClock::waitYear(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetClock::waitMonth(bool on)
{
    display(on ? DF_NONE : DF_BL_BC);
}

void UI::SetClock::waitDay(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetClock::waitDST(bool on)
{
    display(on ? DF_NONE : DF_BL3);
}

void UI::SetClock::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetClock::updateType(ev_e unused)
{
    _clock.type = Rtc::clockIs12H(_clock.type) ? Rtc::clock24H() : Rtc::clock12H();
}

void UI::SetClock::updateHour(ev_e ev)
{
    evUpdate < uint8_t > (_clock.hour, ev, 0, 23);
}

void UI::SetClock::updateMinute(ev_e ev)
{
    evUpdate < uint8_t > (_clock.minute, ev, 0, 59);
}

void UI::SetClock::updateYear(ev_e ev)
{
    evUpdate < uint16_t > (_clock.year, ev, _ui._rtc.clockMinYear(), _ui._rtc.clockMaxYear());
}

void UI::SetClock::updateMonth(ev_e ev)
{
    evUpdate < uint8_t > (_clock.month, ev, 1, 12);
}

void UI::SetClock::updateDay(ev_e ev)
{
    evUpdate < uint8_t > (_clock.day, ev, 1, Rtc::clockMaxDay(_clock.month, _clock.year));
}

void UI::SetClock::updateDST(ev_e unused)
{
    _clock.dst = !_clock.dst;
}

void UI::SetClock::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::SetClock::displayType(df_t flags)
{
    Rtc::clockIs12H(_clock.type) ? _ui._display.show12Hour(flags) : _ui._display.show24Hour(flags);
}

void UI::SetClock::displayTime(df_t flags)
{
    _ui._display.showClock(_clock.hour, _clock.minute,
            flags | (Rtc::clockIs12H(_clock.type) ? DF_12H : DF_24H));
}

void UI::SetClock::displayDate(df_t flags)
{
    _ui._display.showDate(_clock.month, _clock.day, flags);
}

void UI::SetClock::displayYear(df_t flags)
{
    _ui._display.showInteger(_clock.year, flags);
}

void UI::SetClock::displayDST(df_t flags)
{
    _ui._display.showOption("DS", _clock.dst ? 'Y' : 'N', flags);
}

void UI::SetClock::displayDone(df_t flags)
{
    static constexpr uint8_t const nopts = 5;
    static uint32_t i = 0;

    if (_done) { i = 0; _done = false; }
    else if (flags == DF_NONE) i++;
    else return;

    switch (i % (nopts + 1))
    {
        case 0: displayTime(); break;
        case 1: displayType(); break;
        case 2: displayDate(); break;
        case 3: displayYear(); break;
        case 4: displayDST(); break;
        default: _ui._display.showString("... ");
    }
}

////////////////////////////////////////////////////////////////////////////////
// SetClock END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SetTimer START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool UI::SetTimer::uisBegin(void)
{
    _ui._rtc.getTimer(_timer);

    _state = STS_MINUTES;

    _blink.reset();
    _updated = _done = false;
    display();

    return true;
}

void UI::SetTimer::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::SetTimer::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    _updated = true;
    MFC(_update_actions[_state])(ev);
    display();
}

void UI::SetTimer::uisChange(void)
{
    _state = _next_states[_state];

    if ((_state == STS_SECONDS) && (_timer.minutes == 0) && (_timer.seconds == 0))
        _timer.seconds = 1;

    if (_state == STS_DONE)
        (void)_ui._rtc.setTimer(_timer);

    _blink.reset();
    display();
}

void UI::SetTimer::uisReset(void)
{
    // This won't be called since the state will be changed back to clock mode
    _ui._display.blank();
}

void UI::SetTimer::uisEnd(void)
{
    if (_updated)
        (void)_ui._rtc.setTimer(_timer);

    _ui._display.blank();
}

bool UI::SetTimer::uisSleep(void)
{
    return true;
}

void UI::SetTimer::waitMinutes(bool on)
{
    display(on ? DF_NONE : DF_BL_BC);
}

void UI::SetTimer::waitSeconds(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetTimer::updateMinutes(ev_e ev)
{
    evUpdate < uint8_t > (_timer.minutes, ev, 0, 99);
}

void UI::SetTimer::updateSeconds(ev_e ev)
{
    evUpdate < uint8_t > (_timer.seconds, ev, (_timer.minutes == 0) ? 1 : 0, 59);
}

void UI::SetTimer::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::SetTimer::displayTimer(df_t flags)
{
    _ui._display.showTimer(_timer.minutes, _timer.seconds,
            (_timer.minutes == 0) ? (flags | DF_BL0) : (flags | DF_NO_LZ));
}

////////////////////////////////////////////////////////////////////////////////
// SetTimer END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Clock START /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool UI::Clock::uisBegin(void)
{
    _state = CS_TIME;

    clockUpdate(true);

    _dflag = _ui._rtc.clockIs12H() ? DF_12H : DF_24H;
    display();

    _alarm.begin();

    return true;
}

void UI::Clock::uisWait(void)
{
    bool force = false;

    if ((_state != CS_TIME) && _date_display.toggled())
    {
        _state = CS_TIME;
        force = true;
    }

    if (clockUpdate(force))
        display();

    if (_alarm.enabled())
        _alarm.update(_clock.hour, _clock.minute);
}

void UI::Clock::uisUpdate(ev_e ev)
{
    static uint32_t turn_wait = 0;

    // Just in case touch isn't working when battery powered,
    // snooze on encoder turn.
    if (_alarm.inProgress())
    {
        (void)_alarm.snooze(true);
        turn_wait = msecs();
    }
    else
    {
        // Don't process turns for a second after turn to snooze
        if ((msecs() - turn_wait) > 1000)
            _ui._lighting.cycleNL(ev);

        uisWait();
    }
}

void UI::Clock::uisChange(void)
{
    if (_alarm.inProgress())
    {
        _alarm.stop();
        return;
    }

    _state = _next_states[_state];

    if (_state != CS_TIME)
        _date_display.reset();

    display();
}

void UI::Clock::uisReset(void)
{
    // Night light gets turned on
    return;
}

void UI::Clock::uisEnd(void)
{
    if (_alarm.inProgress())
        _alarm.stop();

    _ui._display.blank();
}

bool UI::Clock::uisSleep(void)
{
    return false;
}

bool UI::Clock::clockUpdate(bool force)
{
    if ((_ui._rtc.update() < RU_MIN) && !force)
        return false;

    tClock c;
    bool updated = force;

    _ui._rtc.getClock(c);

    if ((_state == CS_TIME)
            && ((c.minute != _clock.minute) || (c.hour != _clock.hour)))
    {
        updated = true;
    }
    else if ((_state == CS_DATE)
            && ((c.day != _clock.day) || (c.month != _clock.month)))
    {
        updated = true;
    }
    else if ((_state == CS_YEAR) && (c.year != _clock.year))
    {
        updated = true;
    }

    _clock = c;

    return updated;
}

void UI::Clock::display(void)
{
    MFC(_display_actions[_state])();
}

void UI::Clock::displayTime(void)
{
    _ui._display.showClock(_clock.hour, _clock.minute, _dflag);
}

void UI::Clock::displayDate(void)
{
    _ui._display.showDate(_clock.month, _clock.day);
}

void UI::Clock::displayYear(void)
{
    _ui._display.showInteger(_clock.year);
}
////////////////////////////////////////////////////////////////////////////////
// Clock END ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Timer START /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
uint32_t volatile UI::Timer::_s_timer_seconds = 0;
uint8_t volatile UI::Timer::_s_timer_hue = UI::Timer::_s_timer_hue_max;

void UI::Timer::timerDisplay(void)
{
    if (_s_timer_seconds == 0)
        return;

    _s_timer_seconds--;
}

void UI::Timer::timerLeds(void)
{
    if (_s_timer_hue == _s_timer_hue_min)
        return;

    _s_timer_hue--;
}

bool UI::Timer::uisBegin(void)
{
    _state = TS_WAIT;
    _show_clock = false;
    _ui._rtc.getTimer(_timer);
    _ui._display.showTimer(_timer.minutes, _timer.seconds, DF_NO_LZ);

    _display_timer = Pit::acquire(timerDisplay, _s_seconds_interval);
    _led_timer = Pit::acquire(timerLeds,
            (((uint32_t)_timer.minutes * 60) + (uint32_t)_timer.seconds) * _s_hue_interval);

    if ((_display_timer == nullptr) || (_led_timer == nullptr))
        return false;

    return true;
}

void UI::Timer::uisWait(void)
{
    if (_wait_actions[_state] != nullptr)
        MFC(_wait_actions[_state])();

    if (_show_clock)
        clock();
}

void UI::Timer::uisUpdate(ev_e ev)
{
    static uint32_t turn_msecs = 0;
    static int8_t turns = 0;

    if (((msecs() - turn_msecs) < _s_turn_wait) || (_state == TS_ALERT))
        return;

    turns += (int8_t)ev;

    if ((turns == _s_turns) || (turns == -_s_turns))
    {
        turns = 0;
        turn_msecs = msecs();

        _show_clock = !_show_clock;
        if (_show_clock)
            clock(true);
        else if (_state != TS_WAIT)
            timer(true);
        else
            _ui._display.showTimer(_timer.minutes, _timer.seconds, DF_NO_LZ);
    }

    uisWait();
}

void UI::Timer::uisChange(void)
{
    if ((_timer.minutes == 0) && (_timer.seconds == 0))
        return;

    // Action comes before changing state here
    MFC(_change_actions[_state])();

    _state = _next_states[_state];
}

void UI::Timer::uisReset(void)
{
    stop();
    reset();
    _state = TS_WAIT;
}

void UI::Timer::uisEnd(void)
{
    stop();
    _ui._lighting.state(Lighting::LS_NIGHT_LIGHT);
    _ui._beeper.stop();
    _ui._display.blank();
    _display_timer->release();
    _led_timer->release();
    _display_timer = _led_timer = nullptr;
}

bool UI::Timer::uisSleep(void)
{
    return (_state != TS_RUNNING) && (_state != TS_ALERT);
}

void UI::Timer::timer(bool force)
{
    // Not critical that interrupts be disabled
    _second = _s_timer_seconds;

    if ((_second == _last_second) && !force)
        return;

    _last_second = _second;

    _minute = 0;
    if (_second >= 60)
    {
        _minute = (uint8_t)(_second / 60);
        _second %= 60;
    }

    _ui._display.showTimer(_minute, (uint8_t)_second, DF_NO_LZ);
}

void UI::Timer::clock(bool force)
{
    if ((_ui._rtc.update() >= RU_MIN) || force)
        _ui._display.showClock(_ui._rtc.clockHour(), _ui._rtc.clockMinute(), _ui._rtc.clockIs12H() ? DF_12H : DF_24H);
}

void UI::Timer::start(void)
{
    _s_timer_seconds = ((uint32_t)_timer.minutes * 60) + (uint32_t)_timer.seconds;
    _last_second = _s_timer_seconds;

    _s_timer_hue = _s_timer_hue_max;
    _last_hue = _s_timer_hue;
    _ui._lighting.state(Lighting::LS_TIMER);
    _ui._lighting.setColor(CHSV(_last_hue, 255, 255));

    _display_timer->start();
    _led_timer->start();
}

void UI::Timer::reset(void)
{
    _ui._beeper.stop();
    _ui._lighting.state(Lighting::LS_NIGHT_LIGHT);
    _ui._display.showTimer(_timer.minutes, _timer.seconds, DF_NO_LZ);
}

void UI::Timer::run(void)
{
    // Again, not critical that interrupts be disabled
    uint8_t hue = _s_timer_hue;

    if (!_show_clock || (_s_timer_seconds == 0))
        timer();

    if (hue != _last_hue)
    {
        _last_hue = hue;
        _ui._lighting.setColor(CHSV(hue, 255, 255));
    }

    if (_last_second == 0)
    {
        stop();
        _show_clock = false;
        _ui._beeper.start();
        _state = TS_ALERT;
    }
}

void UI::Timer::pause(void)
{
    _display_timer->pause();
    _led_timer->pause();
}

void UI::Timer::resume(void)
{
    _display_timer->resume();
    _led_timer->resume();
}

void UI::Timer::stop(void)
{
    _display_timer->stop();
    _led_timer->stop();
}

void UI::Timer::alert(void)
{
    if (!_ui._beeper.toggled())
        return;

    if (_ui._beeper.on())
    {
        _ui._display.showDone();
        _ui._lighting.on();
    }
    else
    {
        _ui._display.blank();
        _ui._lighting.off();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Timer END ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Touch START /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool UI::Touch::uisBegin(void)
{
    _state = TS_READ;
    _touch_read = 0;

    // Using this as a display update to show touch value when reading
    // as well as a display blink for setting.
    _blink.reset(_s_touch_read_interval);

    return true;
}

void UI::Touch::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::Touch::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    MFC(_update_actions[_state])(ev);
    display();
}

void UI::Touch::uisChange(void)
{
    _state = _next_states[_state];
    MFC(_change_actions[_state])();
    display();
}

void UI::Touch::uisReset(void)
{
    _state = TS_READ;
    _ui._display.blank();
    _blink.reset(_s_touch_read_interval);
}

void UI::Touch::uisEnd(void)
{
    _ui._display.blank();
}

bool UI::Touch::uisSleep(void)
{
    return false;
}

void UI::Touch::waitRead(bool unused)
{
    _touch_read = _ui._tsi_channel.read();
    display();
}

void UI::Touch::waitThreshold(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::Touch::waitTest(bool unused)
{
    if (_ui._tsi_channel.read() >= _touch_read)
        _ui._beeper.start();
    else
        _ui._beeper.stop();
}

void UI::Touch::updateThreshold(ev_e ev)
{
    evUpdate < uint16_t > (_touch_read, ev, 0, 9999);
}

void UI::Touch::changeRead(void)
{
    _blink.reset(_s_touch_read_interval);
}

void UI::Touch::changeThreshold(void)
{
    _blink.reset(_s_set_blink_time);
}

void UI::Touch::changeTest(void)
{
    _blink.reset(0);
}

void UI::Touch::changeDone(void)
{
    (void)_ui._eeprom.setTouchThreshold(_touch_read);
}

void UI::Touch::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::Touch::displayRead(df_t flags)
{
    _ui._display.showInteger(_touch_read, flags);
}

void UI::Touch::displayThreshold(df_t flags)
{
    _ui._display.showInteger(_touch_read, flags);
}
////////////////////////////////////////////////////////////////////////////////
// Touch END ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SetLeds START ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
constexpr CRGB::Color const UI::SetLeds::_s_colors[];

bool UI::SetLeds::uisBegin(void)
{
    _state = SLS_TYPE;
    _blink.reset();
    _updated = _done = false;
    _ui._lighting.state(Lighting::LS_SET);
    display();

    return true;
}

void UI::SetLeds::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::SetLeds::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    MFC(_update_actions[_state])(ev);

    if (_state != SLS_TYPE)
    {
        _updated = true;
        _ui._lighting.setColor(_color);
    }

    display();
}

void UI::SetLeds::uisChange(void)
{
    static uint32_t click = 0;
    bool double_click = (msecs() - click) < 300;

    click = msecs();

    if (_state == SLS_TYPE)
    {
        _state = _next_type_states[_type];

        if (_state == SLS_COLOR)
        {
            _color = _s_colors[_cindex];
        }
        else
        {
            _sub_state = 0;

            if (_state == SLS_RGB)
            {
                _opt = _rgb_opts[_sub_state];
                _val = &_rgb[_sub_state];
                _color = _rgb;
            }
            else  // _state == SLS_HSV
            {
                _opt = _hsv_opts[_sub_state];
                _val = &_hsv[_sub_state];
                _color = _hsv;
            }
        }

        _ui._lighting.setColor(_color);
    }
    else if ((_state == SLS_COLOR) || double_click)
    {
        _state = SLS_DONE;
    }
    else if (_state == SLS_DONE)
    {
        _state = SLS_TYPE;
    }
    else
    {
        _sub_state = (_sub_state == 2) ? 0 : _sub_state + 1;

        if (_state == SLS_RGB)
        {
            _opt = _rgb_opts[_sub_state];
            _val = &_rgb[_sub_state];
        }
        else  // _state == SLS_HSV
        {
            _opt = _hsv_opts[_sub_state];
            _val = &_hsv[_sub_state];
        }
    }

    if (_state == SLS_DONE)
    {
        _ui._lighting.setNL(_color);
        _done = true;
    }

    _blink.reset();
    display();
}

void UI::SetLeds::uisReset(void)
{
    _state = SLS_TYPE;
    _type = SLT_COLOR;
    _sub_state = 0;
    _rgb = _default_rgb;
    _hsv = _default_hsv;
    _cindex = 0;
    _ui._lighting.setColor(CRGB::BLACK);
    display();
}

void UI::SetLeds::uisEnd(void)
{
    if (_updated)
        _ui._lighting.setNL(_color);

    _ui._lighting.state(Lighting::LS_NIGHT_LIGHT);
    _ui._display.blank();
}

bool UI::SetLeds::uisSleep(void)
{
    return true;
}

void UI::SetLeds::waitType(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetLeds::waitColor(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetLeds::waitRGB(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetLeds::waitHSV(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetLeds::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetLeds::updateType(ev_e ev)
{
    uint8_t type = _type;
    evUpdate < uint8_t > (type, ev, 0, SLT_HSV);
    _type = (slt_e)type;
}

void UI::SetLeds::updateColor(ev_e ev)
{
    evUpdate < uint8_t > (_cindex, ev, 0, (sizeof(_s_colors) / sizeof(CRGB::Color)) - 1);
    _color = _s_colors[_cindex];
}

void UI::SetLeds::updateRGB(ev_e ev)
{
    evUpdate < uint8_t > (*_val, ev, 0, 255);
    _color = _rgb;
}

void UI::SetLeds::updateHSV(ev_e ev)
{
    evUpdate < uint8_t > (*_val, ev, 0, 255);
    _color = _hsv;
}

void UI::SetLeds::display(df_t flags)
{
    if (_display_actions[_state] != nullptr)
        MFC(_display_actions[_state])(flags);
}

void UI::SetLeds::displayType(df_t flags)
{
    switch (_type)
    {
        case SLT_COLOR:
            _ui._display.showString("CoLr", flags);
            break;
        case SLT_RGB:
            _ui._display.showString("RGB", flags);
            break;
        case SLT_HSV:  // Can't display 'V' so use 'B' for value/brightness
            _ui._display.showString("HSB", flags);
            break;
    }
}

void UI::SetLeds::displayColor(df_t flags)
{
    _ui._display.showInteger(_cindex, flags);
}

void UI::SetLeds::displayRGB(df_t flags)
{
    _ui._display.showOption(_opt, *_val, flags | DF_HEX | DF_LZ);
}

void UI::SetLeds::displayHSV(df_t flags)
{
    _ui._display.showOption(_opt, *_val, flags | DF_HEX | DF_LZ);
}

void UI::SetLeds::displayDone(df_t flags)
{
    static uint32_t i = 0;

    if (_done) { i = 0; _done = false; }
    else if (flags == DF_NONE) i++;
    else return;

    switch (_type)
    {
        case SLT_COLOR:
            if ((i % 2) == 0)
                displayType();
            else
                displayColor();
            break;

        case SLT_RGB:
            if ((i % 4) == 0)
            {
                displayType();
            }
            else
            {
                _opt = _rgb_opts[(i % 4) - 1];
                _val = &_rgb[(i % 4) - 1];
                displayRGB();
            }
            break;

        case SLT_HSV:
            if ((i % 4) == 0)
            {
                displayType();
            }
            else
            {
                _opt = _hsv_opts[(i % 4) - 1];
                _val = &_hsv[(i % 4) - 1];
                displayHSV();
            }
            break;
    }
}
////////////////////////////////////////////////////////////////////////////////
// SetLeds END /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

