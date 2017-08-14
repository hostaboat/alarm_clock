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

    _leds.setBrightness(_brightness);
    _leds.updateColor(CRGB::BLACK);
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
        && _display.valid() && _leds.valid() && _beeper.valid();
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
                else if (_player.error() == PLAY_ERR_INVALID)
                    _display.showString("P.PLA");
                else if (_player.error() == PLAY_ERR_GET_FILES)
                    _display.showString("P.FIL");
                else if (_player.error() == PLAY_ERR_OPEN_FILE)
                    _display.showString("P.OPE");
                else if (_player.error() == PLAY_ERR_AUDIO)
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
                else if (!_leds.valid())
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

    if (scs != 0)
    {
        es = ES_DEPRESSED;
        sct = msecs() - scs;
    }
    else if (scd != 0)
    {
        if (scd < _s_medium_reset_time)
            es = ES_SPRESS;
        else if (scd < _s_long_reset_time)
            es = ES_MPRESS;
        else
            es = ES_LPRESS;
    }
    else if (sev != EV_ZERO)
    {
        es = ES_TURNED;
    }

    updateBrightness(bev);

    if (resetting(es, sct))
        return;

    int8_t rs_pos = _select.position();
    ss_e ss = SS_NONE;
    if      (rs_pos == 0) ss = SS_RIGHT;
    else if (rs_pos == 1) ss = SS_MIDDLE;
    else if (rs_pos == 2) ss = SS_LEFT;

    uis_e slast = _state;

    if ((es == ES_LPRESS) && (ss == SS_LEFT))
    {
        if (slast == UIS_SET_ALARM)
            _state = UIS_TOUCH;
        else if (slast == UIS_TOUCH)
            _state = UIS_SET_ALARM;

        es = ES_NONE;
    }
    else if ((es == ES_MPRESS) && (ss == SS_RIGHT))
    {
        if (slast == UIS_SET_CLOCK)
            _state = UIS_SET_TIMER;
        else
            _state = UIS_SET_CLOCK;

        es = ES_NONE;
    }
    else
    {
        _state = _next_states[slast][ss];
    }

    if (resting(es, _state != slast, bev != EV_ZERO))
        return;

    if (_state != slast)
    {
        _states[slast]->uisEnd();

        // This shouldn't fail.  If it does, there's a major problem.
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

        case ES_SPRESS:
            _states[_state]->uisChange();
            break;

        case ES_MPRESS:
        case ES_LPRESS:
            _states[_state]->uisReset();
            break;
    }
}

bool UI::resetting(es_e encoder_state, uint32_t depressed_time)
{
    // Reset states
    enum rs_e : uint8_t
    {
        RS_WAIT,
        RS_RESET,
        RS_WAIT_TOUCH,
        RS_TOUCH,
        RS_WAIT_RELEASED,
        RS_RELEASED
    };

    // Display types
    enum dt_e : uint8_t { DT_NONE, DT_DASHES, DT_TOUCH };

    static rs_e rs_state = RS_WAIT;
    static dt_e display_type = DT_NONE;
    static Toggle t(_s_reset_blink_time);

    auto blink = [&](dt_e dt) -> void
    {
        if (!t.toggled())
            return;

        if (t.on())
        {
            if (dt == DT_DASHES)
                _display.showDashes();
            else if (dt == DT_TOUCH)
                _display.showTouch();
        }
        else
        {
            _display.blank();
        }
    };

    if ((encoder_state != ES_DEPRESSED) && (rs_state != RS_WAIT))
        rs_state = RS_RELEASED;

    switch (rs_state)
    {
        case RS_WAIT:
            if (depressed_time < _s_medium_reset_time)
                break;

            rs_state = RS_RESET;
            // Fall through
        case RS_RESET:
            if (_state == UIS_CLOCK)
            {
                nightLight();
                t.disable();

                rs_state = RS_WAIT_RELEASED;
            }
            else
            {
                display_type = DT_DASHES;
                t.reset();

                if (_state == UIS_SET_ALARM)
                    rs_state = RS_WAIT_TOUCH;
                else
                    rs_state = RS_WAIT_RELEASED;
            }
            break;

        case RS_WAIT_TOUCH:
            if (depressed_time >= _s_long_reset_time)
                rs_state = RS_TOUCH;
            else
                blink(display_type);
            break;

        case RS_TOUCH:
            display_type = DT_TOUCH;
            rs_state = RS_WAIT_RELEASED;
            break;

        case RS_WAIT_RELEASED:
            blink(display_type);
            break;

        case RS_RELEASED:
            rs_state = RS_WAIT;
            display_type = DT_NONE;
            t.reset();
            if (_state != UIS_CLOCK)
                _display.blank();
            break;
    }

    return (rs_state != RS_WAIT);
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
            _leds.show();
            resting = false;
        }

        rstart = msecs();
    }
    else if (!resting && ((msecs() - rstart) >= _s_rest_time))
    {
        _display.sleep();
        _leds.blank();
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
    static PinInPU < PIN_EN_BR_A > & br_enc = PinInPU < PIN_EN_BR_A >::acquire();
    static PinIn < PIN_PLAY > & play = PinIn < PIN_PLAY >::acquire();

    _llwu.enable();

    if (!_llwu.pinsEnable(
                ui_swi, IRQC_INTR_FALLING,
                ui_enc, IRQC_INTR_CHANGE,
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
        _leds.show();

        resting = false;
        rstart = msecs();
    }

    return resting;
}

void UI::updateBrightness(ev_e bev)
{
    uint8_t brightness = _brightness;
    TBrEnc::evUpdate < uint8_t > (brightness, bev, 0, 255, false);

    if (brightness == _brightness)
        return;

    if (brightness > _brightness)
    {
        if (((brightness % 16) == 0) || (brightness == 255))
            _display.up();
    }
    else
    {
        if ((brightness % 16) == 0)
            _display.down();
    }

    _brightness = brightness;

    if (_leds.isClear())
        _leds.setBrightness(_brightness);
    else
        _leds.updateBrightness(_brightness);
}

void UI::nightLight(void)
{
    static bool on = false;
    on = !on;
    on ? _leds.updateColor(_night_light) : _leds.updateColor(CRGB::BLACK);
}

////////////////////////////////////////////////////////////////////////////////
// UI END //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Player START ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::Player::Player(void)
{
    if (!_prev.valid() || !_play.valid() || !_next.valid()
            || !_audio.valid() || !_fs.valid() || !_eeprom.valid())
    {
        _error = PLAY_ERR_INVALID;
        return;
    }

    int file_cnt = _fs.getFiles(_files, _s_max_files, _exts);
    if (file_cnt <= 0)
    {
        _error = PLAY_ERR_GET_FILES;
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
        _error = PLAY_ERR_OPEN_FILE;
        return;
    }
}

void UI::Player::play(void)
{
    if (disabled() || playing())
        return;

    if (!running() && !_file->rewind())
    {
        _error = PLAY_ERR_OPEN_FILE;
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
            _error = PLAY_ERR_OPEN_FILE;
            return;
        }
    }

    if (_audio.ready() && !_audio.send(_file, 32) && !_file->valid())
        _error = PLAY_ERR_AUDIO;
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
        if ((msecs() - _alarm_start) > _s_alarm_time)
            stop();
        else if (!snooze() && !_alarm_music)
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

    _alarm_start = msecs();
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
    if ((msecs() - _snooze_start) < _s_snooze_time)
        return;

    if (_alarm_music)
        _ui._player.play();
    else
        _ui._beeper.start();

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
    _blink.reset();
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
        (void)_ui._rtc.setAlarm(_alarm);

    _blink.reset();
    display();
}

void UI::SetAlarm::uisReset(void)
{
    toggleEnabled();
    display();
}

void UI::SetAlarm::uisEnd(void)
{
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

void UI::SetAlarm::updateHour(ev_e ev)
{
    TUiEncSw::evUpdate < uint8_t > (_alarm.hour, ev, 0, 23);
}

void UI::SetAlarm::updateMinute(ev_e ev)
{
    TUiEncSw::evUpdate < uint8_t > (_alarm.minute, ev, 0, 59);
}

void UI::SetAlarm::updateType(ev_e unused)
{
    _alarm.type = Rtc::alarmIsMusic(_alarm.type) ? Rtc::alarmBeep() : Rtc::alarmMusic();
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

    _state = SCS_TYPE;

    _blink.reset();
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
    if (_update_actions[_state] != nullptr)
    {
        MFC(_update_actions[_state])(ev);
        display();
    }
}

void UI::SetClock::uisChange(void)
{
    _state = _next_states[_state];

    if (_state == SCS_DONE)
        (void)_ui._rtc.setClock(_clock);

    _blink.reset();
    display();
}

void UI::SetClock::uisReset(void)
{
    // This won't be called since the state will be changed to timer mode
    _ui._display.blank();
}

void UI::SetClock::uisEnd(void)
{
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

void UI::SetClock::updateType(ev_e unused)
{
    _clock.type = Rtc::clockIs12H(_clock.type) ? Rtc::clock24H() : Rtc::clock12H();
}

void UI::SetClock::updateHour(ev_e ev)
{
    TUiEncSw::evUpdate < uint8_t > (_clock.hour, ev, 0, 23);
}

void UI::SetClock::updateMinute(ev_e ev)
{
    TUiEncSw::evUpdate < uint8_t > (_clock.minute, ev, 0, 59);
}

void UI::SetClock::updateYear(ev_e ev)
{
    TUiEncSw::evUpdate < uint16_t > (_clock.year, ev, _ui._rtc.clockMinYear(), _ui._rtc.clockMaxYear());
}

void UI::SetClock::updateMonth(ev_e ev)
{
    TUiEncSw::evUpdate < uint8_t > (_clock.month, ev, 1, 12);
}

void UI::SetClock::updateDay(ev_e ev)
{
    TUiEncSw::evUpdate < uint8_t > (_clock.day, ev, 1, Rtc::clockMaxDay(_clock.month, _clock.year));
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
    if (_update_actions[_state] != nullptr)
    {
        MFC(_update_actions[_state])(ev);
        display();
    }
}

void UI::SetTimer::uisChange(void)
{
    _state = _next_states[_state];

    if (_state == STS_DONE)
        (void)_ui._rtc.setTimer(_timer);

    _blink.reset();
    display();
}

void UI::SetTimer::uisReset(void)
{
    _ui._display.blank();
}

void UI::SetTimer::uisEnd(void)
{
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
    TUiEncSw::evUpdate < uint8_t > (_timer.minutes, ev, 0, 99);
}

void UI::SetTimer::updateSeconds(ev_e ev)
{
    TUiEncSw::evUpdate < uint8_t > (_timer.seconds, ev, (_timer.minutes == 0) ? 1 : 0, 59);
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

void UI::Clock::uisUpdate(ev_e unused)
{
    // Snooze on encoder turn???
    // Just in case touch isn't working with battery power
    if (_alarm.inProgress())
        (void)_alarm.snooze(true);
    else
        uisWait();
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
    // Maybe disable alarm?
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
}

void UI::Timer::uisUpdate(ev_e unused)
{
    // Encoder turn does nothing
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
    _ui._beeper.stop();
    _ui._leds.updateColor(CRGB::BLACK);
    _display_timer->release();
    _led_timer->release();
    _display_timer = _led_timer = nullptr;
}

bool UI::Timer::uisSleep(void)
{
    return (_state != TS_RUNNING) && (_state != TS_ALERT);
}

void UI::Timer::start(void)
{
    _s_timer_seconds = ((uint32_t)_timer.minutes * 60) + (uint32_t)_timer.seconds;
    _last_second = _s_timer_seconds;

    _s_timer_hue = _s_timer_hue_max;
    _last_hue = _s_timer_hue;
    _ui._leds.updateColor(CHSV(_last_hue, 255, 255));

    _display_timer->start();
    _led_timer->start();
}

void UI::Timer::reset(void)
{
    _ui._beeper.stop();
    _ui._leds.updateColor(CRGB::BLACK);
    _ui._display.showTimer(_timer.minutes, _timer.seconds, DF_NO_LZ);
}

void UI::Timer::run(void)
{
    // Not critical that interrupts be disabled
    uint32_t second = _s_timer_seconds;
    uint8_t hue = _s_timer_hue;

    if (second != _last_second)
    {
        _last_second = second;

        uint8_t minute = 0;
        if (second >= 60)
        {
            minute = (uint8_t)(second / 60);
            second %= 60;
        }

        _ui._display.showTimer(minute, (uint8_t)second, DF_NO_LZ);
    }

    if (hue != _last_hue)
    {
        _last_hue = hue;
        _ui._leds.updateColor(CHSV(hue, 255, 255));
    }

    if (_last_second == 0)
    {
        stop();
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
        _ui._leds.show();
    }
    else
    {
        _ui._display.blank();
        _ui._leds.blank();
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
    TUiEncSw::evUpdate < uint16_t > (_touch_read, ev, 0, 9999);
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

