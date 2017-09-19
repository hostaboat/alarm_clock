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
    return _select.valid() && _br_adjust.valid() && _enc_swi.valid() &&
        _display.valid() && _lighting.valid() && _beeper.valid() &&
        _player._prev.valid() && _player._next.valid() && _player._play.valid() &&
        _player._fs.valid() && _player._audio.valid() && !_player.disabled() &&
        _touch.valid();
}

void UI::error(uis_e state)
{
    _player.stop();
    _beeper.stop();

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
                else if (!_touch.valid())
                    _display.showString("tSI");
            }
        }
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
    else if (rs_pos == 1) ss = SS_LEFT;
    else if (rs_pos == 2) ss = SS_MIDDLE;

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

        _refresh_start = 0;
    }
    else if (_refresh_start)
    {
        if ((_state == UIS_TIMER) && _timer.running())
            _timer.uisWait();

        if ((msecs() - _refresh_start) < _s_refresh_wait)
            return;

        _refresh_start = 0;
        _states[_state]->uisRefresh();
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
            if (ps == PS_CHANGE)
                _states[_state]->uisChange();
            else
                _states[_state]->uisReset(ps);
            break;
    }
}

UI::ps_e UI::pressState(uint32_t msecs)
{
    if (msecs == 0)
        return PS_NONE;
    else if (msecs <= _s_press_change)
        return PS_CHANGE;
    else if (msecs <= _s_press_reset)
        return PS_RESET;
    else if (msecs <= _s_press_state)
        return PS_STATE;
    else
        return PS_ALT_STATE;
}

bool UI::pressing(es_e encoder_state, uint32_t depressed_time)
{
    if (_player.stopping() || _player.skipping())
    {
        if (_player.stopping())
            _display.showString("STOP");
        else
            _display.showInteger(_player.nextTrack() + 1);

        _refresh_start = msecs();

        if ((_state == UIS_TIMER) && _timer.running())
            _timer.uisWait();

        return true;
    }

    // Depressed states
    enum ds_e : uint8_t
    {
        DS_WAIT_RESET,
        DS_RESET,
        DS_WAIT_STATE,
        DS_STATE,
        DS_WAIT_ALT_STATE,
        DS_ALT_STATE,
        DS_WAIT_RELEASED,
        DS_RELEASED
    };

    // Display types
    enum dt_e : uint8_t { DT_NONE, DT_1, DT_2, DT_3 };

    static ds_e ds = DS_WAIT_RESET;
    static dt_e display_type = DT_NONE;
    static Toggle t(_s_reset_blink_time);

    auto blink = [&](dt_e dt) -> void
    {
        if (!t.toggled()) return;
        _display.showBars(dt, false, t.on() ? DF_NONE : DF_BL);
    };

    if ((encoder_state != ES_DEPRESSED) && (ds != DS_WAIT_RESET))
        ds = DS_RELEASED;

    ps_e ps = pressState(depressed_time);

    switch (ds)
    {
        case DS_WAIT_RESET:
            if (ps <= PS_CHANGE)
                break;

            ds = DS_RESET;
            // Fall through
        case DS_RESET:
            if (_state == UIS_CLOCK)
            {
                _lighting.toggleNL();
                t.disable();
            }
            else
            {
                display_type = DT_1;
                t.reset();
            }

            ds = DS_WAIT_STATE;
            break;

        case DS_WAIT_STATE:
            if (ps <= PS_RESET)
                blink(display_type);
            else
                ds = DS_STATE;
            break;

        case DS_STATE:
            if (_state == UIS_CLOCK)
                _lighting.offNL();

            t.reset();
            display_type = DT_2;
            ds = DS_WAIT_ALT_STATE;
            break;

        case DS_WAIT_ALT_STATE:
            if (ps <= PS_STATE)
                blink(display_type);
            else
                ds = DS_ALT_STATE;
            break;

        case DS_ALT_STATE:
            if (_state == UIS_CLOCK)
            {
                _lighting.setNL(_s_default_color);
                _lighting.setColor(_s_default_color);
                _lighting.onNL();
                _display.blank();
                t.disable();
            }
            else
            {
                t.reset();
                display_type = DT_3;
            }

            ds = DS_WAIT_RELEASED;
            break;

        case DS_WAIT_RELEASED:
            blink(display_type);
            break;

        case DS_RELEASED:
            ds = DS_WAIT_RESET;
            display_type = DT_NONE;
            t.reset();
            if (_state != UIS_CLOCK)
                _display.blank();
            break;
    }

    return (ds != DS_WAIT_RESET);
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
            _display.wake(!ui_state_changed);
            if (!_lighting.isOnNL())
                _lighting.wake();
            resting = false;
        }

        rstart = msecs();
    }
    else if (!resting && ((msecs() - rstart) >= _s_rest_time))
    {
        _display.sleep(true);
        if (!_lighting.isOnNL())
            _lighting.sleep();
        resting = true;
    }

    if (!resting || _player.running())
        return resting;

    // Resting is true and the player isn't running - can go to sleep

    // Puts MCU in LLS (Low Leakage Stop) mode - a state retention power mode
    // where most peripherals are in state retention mode (with clocks stopped).
    // Current is reduced by about 44mA if the night light is off.  With the
    // VS1053 off (~14mA) should get a total of ~58mA savings putting things
    // ~17mA (if night light is off) which should be about what the LED on the
    // PowerBoost is consuming and the Neopixel strip which despite being "off"
    // (again if the night light is off) still consumes ~7mA.

    // Don't like this.  Should have a way to just pass in the device.
    static PinIn < PIN_EN_SW > & ui_swi = PinIn < PIN_EN_SW >::acquire();
    static PinInPU < PIN_EN_A > & ui_enc = PinInPU < PIN_EN_A >::acquire();
    static PinInPU < PIN_RS_M > & ui_rs = PinInPU < PIN_RS_M >::acquire();
    static PinInPU < PIN_EN_BR_A > & br_enc = PinInPU < PIN_EN_BR_A >::acquire();
    static PinIn < PIN_PLAY > & play = PinIn < PIN_PLAY >::acquire();

    if (!_lighting.isOnNL())
        _lighting.sleep();

    _llwu.enable();

    if (!_llwu.pinsEnable(
                ui_swi, IRQC_INTR_FALLING,
                ui_enc, IRQC_INTR_CHANGE,
                ui_rs, IRQC_INTR_CHANGE,
                br_enc, IRQC_INTR_CHANGE,
                play, IRQC_INTR_FALLING
                ))
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
        // If the rotary switch is the wakeup source, state is going to change
        // so don't redisplay data from state prior to sleeping.
        _display.wake(wakeup_pin != PIN_RS_M);

        if (!_lighting.isOnNL())
            _lighting.wake();

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
    setBrightness(brightness);
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
        _leds.setColor(c);

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

    if ((lstate != LS_SET) && (state == LS_NIGHT_LIGHT))
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
    if (!_prev.valid() || !_play.valid() || !_next.valid() || !_audio.valid() || !_fs.valid())
    {
        _error = ERR_INVALID;
        return;
    }

    int num_tracks = _fs.getFiles(_tracks, _s_max_tracks, _track_exts);
    if (num_tracks <= 0)
    {
        _error = ERR_GET_FILES;
        return;
    }

    // Max tracks is less than UINT16_MAX
    _num_tracks = (uint16_t)num_tracks;

    if (!_eeprom.getTrack(_current_track) || (_current_track >= _num_tracks))
    {
        _current_track = 0;
        (void)_eeprom.setTrack(_current_track);
    }

    _next_track = _current_track;

    _track = _fs.open(_tracks[_current_track]);
    if (_track == nullptr)
    {
        _error = ERR_OPEN_FILE;
        return;
    }
}

void UI::Player::play(void)
{
    if (disabled() || playing())
        return;

    if (!running() && !_track->rewind())
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

bool UI::Player::pressing(void)
{
    if (!_play.closed() && !_prev.closed() && !_next.closed())
        return false;

    __disable_irq();

    uint32_t play_cs = _play.closeStart();
    uint32_t prev_cs = _prev.closeStart();
    uint32_t next_cs = _next.closeStart();

    __enable_irq();

    if ((play_cs == 0) && (prev_cs == 0) && (next_cs == 0))
        return false;

    if (stopping())
        return true;

    uint32_t m = msecs();

    if (running() && (play_cs != 0) && ((m - play_cs) >= _s_stop_time))
    {
        _stopping = true;
        return true;
    }

    uint32_t tn = (next_cs != 0) ? m - next_cs : 0;
    uint32_t tp = (prev_cs != 0) ? m - prev_cs : 0;

    if ((tn < _s_skip_msecs) && (tp < _s_skip_msecs))
        return true;

    if (tn >= _s_skip_msecs)
        tn -= _s_skip_msecs;
    if (tp >= _s_skip_msecs)
        tp -= _s_skip_msecs;

    int32_t s = skip(tn) - skip(tp);
    if (s == 0) return true;

    _next_track = skipTracks(s);

    return true;
}

void UI::Player::process(void)
{
    static uint32_t sstart = msecs();

    if (disabled() || pressing())
        return;

    __disable_irq();

    uint32_t play_ctime = _play.closeDuration();
    uint32_t prev_ctime = _prev.closeDuration();
    uint32_t next_ctime = _next.closeDuration();

    __enable_irq();

    uint32_t m = msecs();

    // XXX If the MCU is asleep only the play/pause pushbutton will wake it.
    if (!running() && (play_ctime != 0))
    {
        play();
    }
    else if (play_ctime != 0)
    {
        if (play_ctime >= _s_stop_time)
            stop();
        else
            _paused = !_paused;

        sstart = m;
    }
    else if ((prev_ctime != 0) || (next_ctime != 0))
    {
        if (!running() || paused())
            play();
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

    if (_track->eof() || (prev_ctime != 0) || (next_ctime != 0))
    {
        if (_track->eof())
        {
            // Only save file index to eeprom if track was played to completion
            (void)_eeprom.setTrack(_current_track);
            _next_track = skipTracks(1);
        }
        else
        {
            _audio.cancel(_track);
            if (!skipping())
            {
                int32_t s = skip(next_ctime) - skip(prev_ctime);
                if ((s < 0) && rewind()) s++;  // Increment so as not to count it
                _next_track = skipTracks(s);
            }
        }

        if (!newTrack())
            return;
    }

    if (_audio.ready() && !_audio.send(_track, 32) && !_track->valid())
        _error = ERR_AUDIO;
}

bool UI::Player::rewind(void)
{
    if ((_track == nullptr)
            || ((_track->size() - _track->remaining()) < (_track->size() / 4)))
        return false;

    return true;
}

int32_t UI::Player::skip(uint32_t t)
{
    static constexpr uint16_t const ss = 5;
    static constexpr uint32_t const sm = ss * _s_skip_msecs;

    if (t == 0) return 0;

    uint16_t speed = t / sm;
    if (speed > 5) speed = 5;

    uint32_t s = (ss * ((1 << speed) - 1)) + ((t - (sm * speed)) / (_s_skip_msecs / (1 << speed)));

    if (s > INT32_MAX)
        return INT32_MAX;

    return s + 1;
}

uint16_t UI::Player::skipTracks(int32_t skip)
{
    int32_t track = ((int32_t)_current_track + skip) % _num_tracks;
    if (track < 0)
        track = (int32_t)_num_tracks + track;

    return (uint16_t)track;
}

bool UI::Player::newTrack(void)
{
    if (_next_track == _current_track)
    {
        if (_track->rewind())
            return true;
    }
    else
    {
        _current_track = _next_track;
        _track->close();

        _track = _fs.open(_tracks[_current_track]);
        if (_track != nullptr)
            return true;
    }

    _error = ERR_OPEN_FILE;
    return false;
}

bool UI::Player::setTrack(uint16_t track)
{
    if (track >= _num_tracks)
        return false;

    if (running() && !_track->eof())
        _audio.cancel(_track);

    _next_track = skipTracks((int32_t)track - (int32_t)_current_track);
    if (!newTrack())
        return false;

    play();

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
    _alarm_time = _alarm.time * 60 * 60 * 1000;
    _snooze_time = _alarm.snooze * 60 * 1000;
    _wake_time = _alarm.wake * 60 * 1000;
    (void)_ui._touch.enable();
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

    if (!force && !_ui._touch.touched(_wake_start))
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
    _blink.reset(_s_set_blink_time);
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
        _blink.reset(_s_done_blink_time);
    }
    else
    {
        _blink.reset(_s_set_blink_time);
    }

    display();
}

void UI::SetAlarm::uisReset(ps_e ps)
{
    if ((ps == PS_RESET) && ((_state == SAS_HOUR) || (_state == SAS_DISABLED)))
    {
        toggleEnabled();
        display();
    }
    else
    {
        uisBegin();
    }
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

void UI::SetAlarm::uisRefresh(void)
{
    _blink.reset();
    display();
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

void UI::SetAlarm::updateType(ev_e ev)
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
        default: _ui._display.showString("... "); break;
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

    _blink.reset(_s_set_blink_time);
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
        _blink.reset(_s_done_blink_time);
    }
    else
    {
        _blink.reset(_s_set_blink_time);
    }

    display();
}

void UI::SetClock::uisReset(ps_e ps)
{
    uisBegin();
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

void UI::SetClock::uisRefresh(void)
{
    _blink.reset();
    display();
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

void UI::SetClock::updateType(ev_e ev)
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

void UI::SetClock::updateDST(ev_e ev)
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
        default: _ui._display.showString("... "); break;
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

void UI::SetTimer::uisReset(ps_e ps)
{
    uisBegin();
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

void UI::SetTimer::uisRefresh(void)
{
    _blink.reset();
    display();
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

    _alt_display.reset(_s_flash_time);

    clockUpdate(true);
    _alarm.begin();

    _track_updated = false;

    _dflag = _ui._rtc.clockIs12H() ? DF_12H : DF_24H;
    display();

    return true;
}

void UI::Clock::uisWait(void)
{
    bool force = false;

    if ((_state != CS_TIME) && _alt_display.toggled())
    {
        _alt_display.reset(_s_flash_time);
        _state = CS_TIME;
        force = true;
    }

    if (_state != CS_TRACK)
    {
        if (clockUpdate(force))
        {
            display();
        }
        else if (_ui._touch.timedTouch(_s_touch_time))
        {
            _state = CS_TRACK;
            changeTrack();
            display();
        }
    }

    if (_alarm.enabled())
        _alarm.update(_clock.hour, _clock.minute);
}

void UI::Clock::uisUpdate(ev_e ev)
{
    static uint32_t turn_wait = 0;

    // Snooze on encoder turn just in case touch isn't working when
    // battery powered or touch threshold is set too high.
    if (_alarm.inProgress() && !_alarm.snoozing())
    {
        (void)_alarm.snooze(true);
        turn_wait = msecs();
    }
    else if (_state != CS_TRACK)
    {
        // Don't process turns for a second after turn to snooze
        if ((msecs() - turn_wait) > _s_snooze_wait)
            _ui._lighting.cycleNL(ev);

        uisWait();
    }
    else
    {
        updateTrack(ev);
        _track_updated = true;
    }
}

void UI::Clock::uisChange(void)
{
    if (_alarm.inProgress())
    {
        _alarm.stop();
        _state = CS_ALARM;
    }
    else if ((_state != CS_TRACK) || !_track_updated)
    {
        _state = _next_states[_state];
    }

    if (_state == CS_TRACK)
        changeTrack();
    else if (_state != CS_TIME)
        _alt_display.reset();

    display();
}

void UI::Clock::uisReset(ps_e ps)
{
    if (ps >= PS_STATE)
        uisBegin();

    // For PS_RESET, night light gets turned on
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

void UI::Clock::uisRefresh(void)
{
    _alt_display.reset();
    clockUpdate(true);
    display();
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

void UI::Clock::changeTrack(void)
{
    if (_alt_display.onTime() == _s_flash_time)
    {
        _track = _ui._player.currentTrack();
    }
    else
    {
        _ui._player.setTrack(_track);
        _track_updated = false;
    }

    _alt_display.reset(_s_flash_time);
}

void UI::Clock::updateTrack(ev_e ev)
{
    evUpdate < uint16_t > (_track, ev, 0, _ui._player.numTracks() - 1);
    _alt_display.reset(_s_track_idle_time);
    display();
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

void UI::Clock::displayAlarm(void)
{
    _ui._display.showOff();
}

void UI::Clock::displayTrack(void)
{
    _ui._display.showInteger(_track + 1);
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
    (void)_ui._touch.enable();

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

    if (_show_clock && (_ui._refresh_start == 0))
        clockUpdate();
}

void UI::Timer::uisUpdate(ev_e ev)
{
    static uint32_t turn_msecs = 0;
    static int8_t turns = 0;

    if (_state <= TS_WAIT)
    {
        // Allow user to update timer without going back to SetTimer.
        if (_state == TS_WAIT)
            _state = TS_MINUTES;

        MFC(_update_actions[_state])(ev);
        displayTimer();

        return;
    }

    if (_state == TS_ALERT)
    {
        uisChange();
        return;
    }

    if ((msecs() - turn_msecs) < _s_turn_wait)
        return;

    turns += (int8_t)ev;

    if ((turns == _s_turns) || (turns == -_s_turns))
    {
        turns = 0;
        turn_msecs = msecs();

        _show_clock = !_show_clock;
        if (_show_clock)
            clockUpdate(true);
        else if (_state != TS_WAIT)
            timerUpdate(true);
        else
            _ui._display.showTimer(_timer.minutes, _timer.seconds, DF_NO_LZ);
    }

    uisWait();
}

void UI::Timer::uisChange(void)
{
    if (_state < TS_WAIT)
    {
        _state = _next_states[_state];

        if ((_state == TS_SECONDS) && (_timer.minutes == 0) && (_timer.seconds == 0))
            _timer.seconds = 1;

        if (_state != TS_WAIT)
        {
            _blink.reset();
            displayTimer();
        }
        else
        {
            _ui._display.showTimer(_timer.minutes, _timer.seconds, DF_NO_LZ);
            _led_timer->updateInterval(
                    (((uint32_t)_timer.minutes * 60) + (uint32_t)_timer.seconds) * _s_hue_interval, false);
        }

        return;
    }

    if ((_timer.minutes == 0) && (_timer.seconds == 0))
        return;

    // Action comes before changing state here
    MFC(_change_actions[_state])();

    _state = _next_states[_state];
}

void UI::Timer::uisReset(ps_e ps)
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

void UI::Timer::uisRefresh(void)
{
    if (_state < TS_WAIT)
        displayTimer(DF_NONE);
    else if (_show_clock)
        clockUpdate(true);
    else
        timerUpdate(true);
}

void UI::Timer::waitMinutes(void)
{
    if (!_blink.toggled())
        return;

    displayTimer(_blink.on() ? DF_NONE : DF_BL_BC);
}

void UI::Timer::waitSeconds(void)
{
    if (!_blink.toggled())
        return;

    displayTimer(_blink.on() ? DF_NONE : DF_BL_AC);
}

void UI::Timer::updateMinutes(ev_e ev)
{
    evUpdate < uint8_t > (_timer.minutes, ev, 0, 99);
}

void UI::Timer::updateSeconds(ev_e ev)
{
    evUpdate < uint8_t > (_timer.seconds, ev, (_timer.minutes == 0) ? 1 : 0, 59);
}

void UI::Timer::displayTimer(df_t flags)
{
    _ui._display.showTimer(_timer.minutes, _timer.seconds,
            (_timer.minutes == 0) ? (flags | DF_BL0) : (flags | DF_NO_LZ));
}

void UI::Timer::start(void)
{
    _s_timer_seconds = ((uint32_t)_timer.minutes * 60) + (uint32_t)_timer.seconds;
    _last_second = _s_timer_seconds;

    _s_timer_hue = _s_timer_hue_max;
    _last_hue = _s_timer_hue;
    _ui._lighting.state(Lighting::LS_OTHER);
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
    if (!_show_clock || (_s_timer_seconds == 0))
        timerUpdate();

    hueUpdate();

    if (_last_second == 0)
    {
        stop();
        _show_clock = false;
        _ui._beeper.start();
        _state = TS_ALERT;
        _alarm_start = msecs();
    }
}

void UI::Timer::timerUpdate(bool force)
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

    if (_ui._refresh_start == 0)
        _ui._display.showTimer(_minute, (uint8_t)_second, DF_NO_LZ);
}

void UI::Timer::clockUpdate(bool force)
{
    if ((_ui._rtc.update() >= RU_MIN) || force)
    {
        _ui._display.showClock(
                _ui._rtc.clockHour(),
                _ui._rtc.clockMinute(),
                _ui._rtc.clockIs12H() ? DF_12H : DF_24H);
    }
}

void UI::Timer::hueUpdate(void)
{
    // Again, not critical that interrupts be disabled
    uint8_t hue = _s_timer_hue;

    if (hue != _last_hue)
    {
        _last_hue = hue;
        _ui._lighting.setColor(CHSV(hue, 255, 255));
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
    if (_ui._touch.touched(_alarm_start))
    {
        uisChange();
        return;
    }

    if (!_ui._beeper.toggled())
        return;

    if (_ui._beeper.on())
    {
        if (_ui._refresh_start == 0)
            _ui._display.showDone();

        _ui._lighting.on();
    }
    else
    {
        if (_ui._refresh_start == 0)
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
constexpr char const * const UI::Touch::_s_topts[TOPT_CNT];

bool UI::Touch::valid(void)
{
    return _tsi.valid();
}

bool UI::Touch::enabled(void) const
{
    return _touch_enabled;
}

bool UI::Touch::enable(void)
{
    if (_touch.threshold != 0)
        _touch_enabled = true;

    return _touch_enabled;
}

// Argument passed in is to guard against a touch threshold that is too low.
// It should be the start time of some event for which a touch is expected.
// If a touch event occurs within this amount of time, touch will be disabled
// until re-enabled or re-configured.
bool UI::Touch::touched(uint32_t stime)
{
    if (!_touch_enabled)
        return false;

    bool touched = _tsi_channel.touched();

    if (touched && (stime != 0) && ((msecs() - stime) < _s_touch_disable_time))
        _touch_enabled = false;

    return _touch_enabled && touched;
}

bool UI::Touch::timedTouch(uint32_t ttime)
{
    static uint32_t touched_time = msecs();

    if (!_touch_enabled)
        return false;

    uint16_t tval = _tsi_channel.read();
    if (tval < _tsi.threshold())
    {
        if (tval != 0)
            touched_time = msecs();

        return false;
    }

    return (msecs() - touched_time) >= ttime;
}

bool UI::Touch::uisBegin(void)
{
    _tsi.get(_touch);
    _state = (_touch.threshold == 0) ? TS_DISABLED : TS_OPT;
    _ui._player.stop();
    reset();
    _ui._lighting.state(Lighting::LS_OTHER);
    return true;
}

void UI::Touch::uisWait(void)
{
    if (_wait_actions[_state] == nullptr)
        return;

    if ((_state == TS_CAL_UNTOUCHED) || (_state == TS_CAL_TOUCHED) || (_state == TS_READ))
        MFC(_wait_actions[_state])(_blink.toggled());
    else if (_state == TS_TEST)
        MFC(_wait_actions[_state])(_ui._beeper.on());
    else if (_blink.toggled())
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
    if (_state == TS_READ)
    {
        _touch.threshold = _read.hi;
        _untouched.hi = _read.lo;
        _ui._lighting.setColor(CHSV(_touch.threshold % 256, 255, 255));
    }

    if (_state == TS_DISABLED)
    {
        toggleEnabled();
    }
    else if (_state == TS_OPT)
    {
        switch (_topt)
        {
            case TOPT_CAL: _state = TS_CAL_START; break;
            case TOPT_CONF: _state = TS_NSCN; break;
            case TOPT_DEF: _tsi.defaults(_touch); _state = TS_DONE; _topt = TOPT_CONF; break;
            case TOPT_DIS: toggleEnabled(); break;
            case TOPT_READ: _state = TS_READ; break;
            default: break;
        }
    }
    else
    {
        _state = _next_states[_state];
    }

    if (_change_actions[_state] != nullptr)
        MFC(_change_actions[_state])();

    _blink.reset();
    display();
}

void UI::Touch::uisReset(ps_e ps)
{
    if ((_state == TS_CAL_START) || (_state == TS_NSCN))
        _state = TS_OPT;
    else if (_state == TS_DISABLED)
        toggleEnabled();
    else if (_topt == TOPT_CAL)
        _state = TS_CAL_START;
    else if (_topt == TOPT_CONF)
        _state = TS_NSCN;

    _ui._beeper.stop();
    _amp.stop();

    reset();
}

void UI::Touch::uisEnd(void)
{
    _ui._lighting.state(Lighting::LS_NIGHT_LIGHT);
    _ui._display.blank();
    _touch_enabled = (_touch.threshold != 0);
}

bool UI::Touch::uisSleep(void)
{
    return ((_state != TS_CAL_UNTOUCHED) && (_state != TS_CAL_TOUCHED));
}

void UI::Touch::uisRefresh(void)
{
    _blink.reset();
    display();
}

void UI::Touch::reset(void)
{
    _readings = 0;
    _untouched.reset();
    _topt = TOPT_CAL;

    _done = false;

    _blink.reset();
    display();
}

void UI::Touch::toggleEnabled(void)
{
    if (_touch.threshold == 0)
        _tsi.defaults(_touch);
    else
        _touch.threshold = 0;
    _state = (_touch.threshold == 0) ? TS_DISABLED : TS_OPT;
    _topt = TOPT_CAL;
    (void)_tsi.set(_touch);
}

void UI::Touch::waitOpt(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::Touch::waitNscn(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::Touch::waitPs(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::Touch::waitRefChrg(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::Touch::waitExtChrg(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::Touch::waitCalStart(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::Touch::waitCalUntouched(bool on)
{
    if (on) display();

    uint16_t read = _tsi_channel.read();
    if (read == 0)
        return;

    if (read > _untouched.hi)
        _untouched.hi = read;

    _readings++;

    if (_readings == (_s_readings / 2))
    {
        _amp.start();
    }
    else if (_readings == _s_readings)
    {
        _amp.stop();
        uisChange();
    }
    else if ((_readings % 256) == 0)
    {
        _ui._lighting.setColor(CHSV(_readings / 256, 255, 255));
    }
}

void UI::Touch::waitCalThreshold(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::Touch::waitCalTouched(bool on)
{
    if (on) display();

    uint16_t read = _tsi_channel.read();
    if (read == 0)
        return;

    if (read < _touched->lo)
        _touched->lo = read;

    if (read > _touched->hi)
        _touched->hi = read;

    _readings++;

    if (_readings == (_s_readings / 2))
    {
        _amp.start();
        _touched = &_touched_amp_on;
    }
    else if (_readings == _s_readings)
    {
        if (_touched_amp_off.lo < _untouched.hi)
            _touched_amp_off.lo = _untouched.hi;

        if (_touched_amp_on.lo < _untouched.hi)
            _touched_amp_on.lo = _untouched.hi;

        uint16_t amp_off_avg = _touched_amp_off.avg();
        uint16_t amp_on_avg = _touched_amp_on.avg();

        _touch.threshold = (amp_off_avg + amp_on_avg) / 2;

        _ui._lighting.setColor(CHSV(_touch.threshold % 256, 255, 255));
        _amp.stop();
        uisChange();
    }
    else if ((_readings % 256) == 0)
    {
        _ui._lighting.setColor(CHSV(_readings / 256, 255, 255));
    }
}

void UI::Touch::waitRead(bool on)
{
    uint16_t read = _tsi_channel.read();
    if ((read != 0) && (read != _touch.threshold))
    {
        if (read > _read.hi)
            _read.hi = read;

        if (read < _read.lo)
            _read.lo = read;

        _touch.threshold = read;
        _ui._lighting.setColor(CHSV(read % 256, 255, 255));
        display();
    }
}

void UI::Touch::waitTest(bool on)
{
    static uint32_t beep_start = 0;

    uint16_t read = _tsi_channel.read();
    if (read == 0)
        return;

    if ((msecs() - beep_start) < _s_beeper_wait)
        return;

    beep_start = msecs();

    if (read >= _touch.threshold)
    {
        if (!on) _ui._beeper.start();
    }
    else
    {
        if (on) _ui._beeper.stop();
    }
}

void UI::Touch::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::Touch::updateOpt(ev_e ev)
{
    evUpdate < topt_e > (_topt, ev, TOPT_CAL, (topt_e)(TOPT_CNT - 1));
}

void UI::Touch::updateNscn(ev_e ev)
{
    evUpdate < uint8_t > (_touch.nscn, ev, 0, Tsi::maxNscn());
}

void UI::Touch::updatePs(ev_e ev)
{
    evUpdate < uint8_t > (_touch.ps, ev, 0, Tsi::maxPs());
}

void UI::Touch::updateRefChrg(ev_e ev)
{
    evUpdate < uint8_t > (_touch.refchrg, ev, 0, Tsi::maxRefChrg());
}

void UI::Touch::updateExtChrg(ev_e ev)
{
    evUpdate < uint8_t > (_touch.extchrg, ev, 0, Tsi::maxExtChrg());
}

void UI::Touch::updateThreshold(ev_e ev)
{
    evUpdate < uint16_t > (_touch.threshold, ev, _untouched.hi, Tsi::maxThreshold(), false);
    _ui._lighting.setColor(CHSV(_touch.threshold % 256, 255, 255));
}

void UI::Touch::changeCalStart(void)
{
    if (_topt == TOPT_CONF)
        (void)_tsi.configure(_touch.nscn, _touch.ps, _touch.refchrg, _touch.extchrg);

    _ui._lighting.setColor(CHSV(0, 255, 255));
    _readings = 0;
    _untouched.reset();
    _ui._player.stop();
}

void UI::Touch::changeCalThreshold(void)
{
    _ui._lighting.setColor(CHSV(0, 255, 255));
    _readings = 0;
    _touched_amp_off.reset();
    _touched_amp_on.reset();
    _touched = &_touched_amp_off;
}

void UI::Touch::changeRead(void)
{
    _read.reset();
    _untouched.reset();
}

void UI::Touch::changeDone(void)
{
    _ui._lighting.setColor(CRGB::BLACK);
    _ui._beeper.stop();
    (void)_tsi.set(_touch);
    _done = true;
}

void UI::Touch::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::Touch::displayOpt(df_t flags)
{
    _ui._display.showString(_s_topts[_topt], flags);
}

void UI::Touch::displayNscn(df_t flags)
{
    _ui._display.showOption("nS", Tsi::valNscn(_touch.nscn), flags);
}

void UI::Touch::displayPs(df_t flags)
{
    _ui._display.showOption("PS", Tsi::valPs(_touch.ps), flags | DF_HEX);
}

void UI::Touch::displayRefChrg(df_t flags)
{
    _ui._display.showOption("rC", Tsi::valRefChrg(_touch.refchrg), flags);
}

void UI::Touch::displayExtChrg(df_t flags)
{
    _ui._display.showOption("EC", Tsi::valExtChrg(_touch.extchrg), flags);
}

void UI::Touch::displayCalStart(df_t flags)
{
    _ui._display.showString("bASE", flags);
}

void UI::Touch::displayCalThreshold(df_t flags)
{
    _ui._display.showString("tUCH", flags);
}

void UI::Touch::displayCalWait(df_t flags)
{
    _ui._display.showInteger(_readings, flags | DF_HEX);
}

void UI::Touch::displayThreshold(df_t flags)
{
    _ui._display.showInteger(_touch.threshold, flags | DF_HEX);
}

void UI::Touch::displayDone(df_t flags)
{
    static uint32_t i = 0;

    if (_done) { i = 0; _done = false; }
    else if (flags == DF_NONE) i++;
    else return;

    if (_topt == TOPT_CONF)
    {
        static constexpr uint8_t const nopts = 6;

        switch (i % (nopts + 1))
        {
            case 0: displayNscn(); break;
            case 1: displayPs(); break;
            case 2: displayRefChrg(); break;
            case 3: displayExtChrg(); break;
            case 4: _ui._display.showString("thr."); break;
            case 5: displayThreshold(); break;
            default: _ui._display.showString("... "); break;
        }
    }
    else
    {
        if ((i % 2) == 0)
            _ui._display.showString("thr.");
        else
            displayThreshold();
    }
}

void UI::Touch::displayDisabled(df_t flags)
{
    _ui._display.showOff(flags);
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

void UI::SetLeds::uisReset(ps_e ps)
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
    if (_ui._lighting.isOn())
        _ui._lighting.onNL();
    _ui._display.blank();
}

bool UI::SetLeds::uisSleep(void)
{
    return true;
}

void UI::SetLeds::uisRefresh(void)
{
    _blink.reset();
    display();
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
    evUpdate < slt_e > (_type, ev, SLT_COLOR, SLT_HSV);
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

