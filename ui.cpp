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

    _mcu_sleep_time = _set_sleep.getMcuTime();
    _display_sleep_time = _set_sleep.getDisplayTime();

    _display.brightness(BR_MID);
    _display_brightness = _display.brightness();

    // Set initial state
    if (_controls.rsPos() == RSP_NONE)
    {
        _errno = ERR_HW_RS;
        return;
    }

    _state = _next_states[UIS_CLOCK][PS_NONE][_controls.rsPos()];

    _alarm.begin();
    _states[_state]->uisBegin();
}

bool UI::valid(void)
{
    if (_controls.valid() &&
            _display.valid() &&
            _lighting.valid() &&
            _beeper.valid() &&
            _tsi.valid() &&
            !_player.disabled() &&
            (_errno == ERR_NONE))
    {
        return true;
    }

    if (_errno != ERR_NONE)
        return false;

    if (!_controls.rsValid())
        _errno = ERR_HW_RS;
    else if (!_controls.encValid())
        _errno = ERR_HW_ENC_SWI;
    else if (!_controls.brValid())
        _errno = ERR_HW_BR_ENC;
    else if (!_controls.swiValid(Controls::SWI_PLAY))
        _errno = ERR_HW_PLAY;
    else if (!_controls.swiValid(Controls::SWI_NEXT))
        _errno = ERR_HW_NEXT;
    else if (!_controls.swiValid(Controls::SWI_PREV))
        _errno = ERR_HW_PREV;
    else if (!_display.valid())
        _errno = ERR_HW_DISPLAY;
    else if (!_lighting.valid())
        _errno = ERR_HW_LIGHTING;
    else if (!_beeper.valid())
        _errno = ERR_HW_BEEPER;
    else if (!_tsi.valid())
        _errno = ERR_HW_TOUCH;
    else if (!_player._fs.valid())
        _errno = ERR_HW_DISK;
    else if (!_player._audio.valid())
        _errno = ERR_HW_AUDIO;
    else if (_player.disabled())
        _errno = ERR_PLAYER;

    return false;
}

void UI::error(void)
{
    _player.stop();
    _beeper.stop();

    Toggle err(1000);
    bool error_str = true;
    _display.showString("Err");

    while (true)
    {
        _controls.process();
        if (_controls.interaction())
            break;

        if (err.toggled())
        {
            if (err.on())
                _display.showString("Err");
            else if ((error_str = !error_str))
                _display.showString(_err_strs[_errno]);
            else
                _display.showInteger(_errno);
        }
    }

    _errno = ERR_NONE;
}

void UI::process(void)
{
    static bool wait_released = false;

    if (!valid()) error();

    ////////////////////////////////////////////////////////////////////////////
    // Controls
    _controls.process();

    ev_e bev = _controls.brTurn();
    ev_e sev = _controls.encTurn();
    uint32_t scs = _controls.swiCS(Controls::SWI_ENC);
    uint32_t scd = _controls.swiCD(Controls::SWI_ENC);
    rsp_e rsp = _controls.rsPos();

    uint32_t enc_pressed_time = 0;
    es_e es = ES_NONE;
    ps_e ps = PS_NONE;

    if (scs != 0)
    {
        es = ES_DEPRESSED;
        enc_pressed_time = msecs() - scs;
    }
    else if (scd != 0)
    {
        if (!wait_released)
        {
            es = ES_PRESSED;
            ps = pressState(scd);
        }

        wait_released = false;
    }
    else if (sev != EV_ZERO)
    {
        es = ES_TURNED;
    }
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Player
    _player.process();
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Brightness and Lighting
    updateBrightness(bev);
    _lighting.paletteNL();
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Alarm
    Alarm::as_e las = _alarm.state();
    Alarm::as_e as = _alarm.process(sev != EV_ZERO, es == ES_DEPRESSED);

    if ((las != Alarm::AS_OFF) && (as == Alarm::AS_OFF))
    {
        _display.showString("OFF");
        _refresh_start = msecs();

        if (es == ES_DEPRESSED)
            wait_released = true;
    }
    else if ((las != Alarm::AS_SNOOZE) && (as == Alarm::AS_SNOOZE))
    {
        _display.showString("Snoo");
        _refresh_start = msecs();
    }

    if ((las != as) && ((sev != EV_ZERO) || (es == ES_DEPRESSED)))
        return;
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Pressing - PS_RESET or longer
    if (pressing(es, enc_pressed_time))
    {
        wait_released = false;
        return;
    }
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // UI State
    uis_e slast = _state;
    _state = _next_states[slast][ps][rsp];
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Sleep
    bool wake = (_refresh_start != 0) || _alarm.alerting() || !_states[_state]->uisSleep()
        || _controls.interaction() || (_state != slast);

    sl_e sl = sleep(wake, _state == slast);
    if (sl == SL_RESTING)
        return;

    if (sl == SL_SLEPT)
    {
        if (_controls.swiClosed(Controls::SWI_ENC))
            wait_released = true;

        return;
    }

    if ((sl == SL_RESTED) && (_state == slast))
    {
        if (es == ES_DEPRESSED)
            wait_released = true;
        return;
    }
    ////////////////////////////////////////////////////////////////////////////

    if (_state != slast)
    {
        if (es == ES_PRESSED)
            es = ES_NONE;

        _states[slast]->uisEnd();
        _states[_state]->uisBegin();

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
        // Forcibly wake if only resting
        if (!_display.isAwake())
            (void)sleep(true, false);

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
            {
                _lighting.offNL();
            }
            else
            {
                t.reset();
                display_type = DT_2;
            }

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

UI::sl_e UI::sleep(bool wake, bool redisplay)
{
    static uint32_t rstart = msecs();  // Rest and sleep start
    static sl_e sl_state = SL_NO;

    auto display_wake = [&](bool rd) -> void
    {
        if (!_display.isAwake())
        {
            _display.wake(rd);
            if (rd) _states[_state]->uisRefresh();
        }
        else
        {
            _display.brightness(_display_brightness);
        }
    };

    auto display_sleep = [&](void) -> void
    {
        if (_state != UIS_CLOCK)
            _display.sleep(true);
        else if (_display.isOn())
            _display.brightness(BR_LOW);
    };

    auto clock_update = [&](void) -> void
    {
        if (sl_state == SL_SLEEPING)
            _rtc.wake();

        if ((_rtc.update() >= RU_MIN) && (_state == UIS_CLOCK))
            _display.showClock(_rtc.clockHour(), _rtc.clockMinute(), _rtc.clockIs12H() ? DF_12H : DF_24H);

        if (sl_state == SL_SLEEPING)
            _rtc.sleep();
    };

    uint32_t m = msecs();

    if ((sl_state == SL_RESTED) || (sl_state == SL_SLEPT))
        sl_state = SL_NO;

    if ((sl_state == SL_RESTING) && _controls.touched())
        wake = true; 

    if (wake)
    {
        if (sl_state == SL_RESTING)
        {
            display_wake(redisplay);

            if (!_lighting.isOnNL())
                _lighting.wake();

            sl_state = SL_RESTED;
        }

        rstart = m;
    }
    else if ((sl_state == SL_NO)
            && (_display_sleep_time != 0) && ((m - rstart) >= _display_sleep_time))
    {
        display_sleep();

        if (!_lighting.isOnNL())
            _lighting.sleep();

        sl_state = SL_RESTING;
    }

    bool sleep = (sl_state != SL_RESTED)
        && ((_mcu_sleep_time != 0) && ((m - rstart) >= _mcu_sleep_time));

    if (!sleep || _player.running())
    {
        if (sl_state == SL_RESTING)
            clock_update();

        return sl_state;
    }

    display_sleep();
    _lighting.sleep();

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

    _llwu.enable();

    if (!_llwu.pinsEnable(
                ui_swi, IRQC_INTR_RISING,
                ui_enc, IRQC_INTR_CHANGE,
                ui_rs, IRQC_INTR_CHANGE,
                br_enc, IRQC_INTR_CHANGE,
                play, IRQC_INTR_RISING
                ))
    {
        _llwu.disable();
        return sl_state;
    }

    sl_state = SL_SLEEPING;

    // Sleep for about a minute and wake up to check time
    (void)_rtc.update();
    (void)_llwu.timerEnable(60000 - (_rtc.clockSecond() * 1000));

    if (_tsi.threshold() != 0)
        (void)_llwu.tsiEnable < PIN_TOUCH > ();

    (void)_llwu.alarmEnable();

    // Tell the clock that it needs to read the TSR counter since NVIC is disabled
    _rtc.sleep();

    while (true)
    {
        if ((_llwu.sleep() != WUS_MODULE) || (_llwu.wakeupModule() != WUMS_LPTMR))
            break;

        clock_update();
        (void)_llwu.timerUpdate(60000 - (_rtc.clockSecond() * 1000));
    }

    _llwu.disable();
    _rtc.wake();

    sl_state = SL_SLEPT;

    // If the rotary switch is the wakeup source, state is going to change
    // so don't redisplay data from state prior to sleeping.
    display_wake(_llwu.wakeupPin() != PIN_RS_M);
    _lighting.wake();

    rstart = msecs();

    return sl_state;
}

void UI::updateBrightness(ev_e bev)
{
    uint8_t brightness = _brightness;
    evUpdate < uint8_t > (brightness, bev, 0, 255, false);

    if (brightness == _brightness)
        return;

    if (_display.brightness() != _display_brightness)
        _display.brightness(_display_brightness);

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
    _display_brightness = _display.brightness();
}

////////////////////////////////////////////////////////////////////////////////
// UI END //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Controls START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::Controls::Controls(UI & ui)
    : _ui{ui}
{
    setRsPos();
    _rs_pos_last = _rs_pos;
}

void UI::Controls::process(void)
{
    setRsPos();
    setTouch();

    __disable_irq();

    _br_turn = _br_adjust.turn();
    _enc_turn = _enc.turn();

    _enc_swi.process();
    _play.process();
    _next.process();
    _prev.process();

    __enable_irq();

    _enc_swi.postProcess();
    _play.postProcess();
    _next.postProcess();
    _prev.postProcess();

    _interaction = ((_rs_pos != RSP_NONE) && (_rs_pos != _rs_pos_last))
        || touched()
        || (_br_turn != EV_ZERO) || (_enc_turn != EV_ZERO)
        || _enc_swi.closed() || (_enc_swi.cd() != 0)
        || _play.closed() || (_play.cd() != 0)
        || _next.closed() || (_next.cd() != 0)
        || _prev.closed() || (_prev.cd() != 0);
}

bool UI::Controls::swiClosed(swi_e swi)
{
    switch (swi)
    {
        case SWI_ENC: return _enc_swi.closed();
        case SWI_PLAY: return _play.closed();
        case SWI_NEXT: return _next.closed();
        case SWI_PREV: return _prev.closed();
    }

    return false;
}

uint32_t UI::Controls::swiCS(swi_e swi)
{
    switch (swi)
    {
        case SWI_ENC: return _enc_swi.cs();
        case SWI_PLAY: return _play.cs();
        case SWI_NEXT: return _next.cs();
        case SWI_PREV: return _prev.cs();
    }

    return 0;
}

uint32_t UI::Controls::swiCD(swi_e swi)
{
    switch (swi)
    {
        case SWI_ENC: return _enc_swi.cd();
        case SWI_PLAY: return _play.cd();
        case SWI_NEXT: return _next.cd();
        case SWI_PREV: return _prev.cd();
    }

    return 0;
}

bool UI::Controls::swiValid(swi_e swi)
{
    switch (swi)
    {
        case SWI_ENC: return _enc_swi.valid();
        case SWI_PLAY: return _play.valid();
        case SWI_NEXT: return _next.valid();
        case SWI_PREV: return _prev.valid();
    }

    return false;
}

void UI::Controls::setRsPos(void)
{
    if (_rs_pos != RSP_NONE)
        _rs_pos_last = _rs_pos;

    int8_t pos = _rs.position();
    _rs_pos = RSP_NONE;
    if      (pos == 0) _rs_pos = RSP_RIGHT;
    else if (pos == 1) _rs_pos = RSP_LEFT;
    else if (pos == 2) _rs_pos = RSP_MIDDLE;
}

void UI::Controls::setTouch(void)
{
    uint16_t thr = _ui._tsi.threshold();
    if (thr == 0)
        return;

    // Return value of 0 means it's in the process of scanning
    uint16_t tval = _ui._tsi_channel.read();
    if (tval == 0)
        return;

    if ((_touch_mark == 0) && (tval >= thr))
        _touch_mark = msecs();
    else if ((_touch_mark != 0) && (tval < thr))
        _touch_mark = 0;
}

void UI::Controls::reset(void)
{
    _br_turn = _enc_turn = EV_ZERO;
    _rs_pos = _rs_pos_last = RSP_NONE;
    _enc_swi.reset();
    _play.reset();
    _next.reset();
    _prev.reset();
}

bool UI::Controls::valid(void) const
{
    return rsValid() && brValid() && encValid()
        && _play.valid() && _next.valid() && _prev.valid();
}

////////////////////////////////////////////////////////////////////////////////
// Controls END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Player START ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::Player::Player(UI & ui)
    : _ui{ui}
{
    if (!_audio.valid() || !_fs.valid())
    {
        _disabled = true;
        return;
    }

    _auto_stop_time = _ui._set_sleep.getPlayerTime();

    int num_tracks = _fs.getFiles(_tracks, _s_max_tracks, _track_exts);
    if (num_tracks <= 0)
    {
        _ui._errno = ERR_PLAYER_GET_FILES;
        _disabled = true;
        return;
    }

    // Max tracks is less than UINT16_MAX
    _num_tracks = (uint16_t)num_tracks;

    if (!_ui._eeprom.getTrack(_current_track) || (_current_track >= _num_tracks))
    {
        _current_track = 0;
        (void)_ui._eeprom.setTrack(_current_track);
    }

    _next_track = _current_track;

    _track = _fs.open(_tracks[_current_track]);
    if (_track == nullptr)
    {
        _ui._errno = ERR_PLAYER_OPEN_FILE;
        _disabled = true;
        return;
    }
}

void UI::Player::play(void)
{
    if (disabled() || playing())
        return;

    if (!running() && !_track->rewind())
    {
        _ui._errno = ERR_PLAYER_OPEN_FILE;
        _disabled = true;
        return;
    }

    start();
}

bool UI::Player::occupied(void) const
{
    return playing()
        || _ui._controls.swiClosed(Controls::SWI_PLAY)
        || _ui._controls.swiClosed(Controls::SWI_NEXT)
        || _ui._controls.swiClosed(Controls::SWI_PREV);
}

bool UI::Player::pressing(void)
{
    if (!_ui._controls.swiClosed(Controls::SWI_PLAY)
            && !_ui._controls.swiClosed(Controls::SWI_NEXT)
            && !_ui._controls.swiClosed(Controls::SWI_PREV))
        return false;

    uint32_t play_cs = _ui._controls.swiCS(Controls::SWI_PLAY);
    uint32_t next_cs = _ui._controls.swiCS(Controls::SWI_NEXT);
    uint32_t prev_cs = _ui._controls.swiCS(Controls::SWI_PREV);

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

void UI::Player::autoStop(bool inactive)
{
    static uint32_t sstart = msecs();

    uint32_t m = msecs();

    if (inactive)
    {
        if ((m - sstart) >= _auto_stop_time)
            _audio.stop();

        return;
    }

    sstart = m;
}

void UI::Player::process(void)
{
    if (disabled() || pressing())
        return;

    uint32_t play_cd = _ui._controls.swiCD(Controls::SWI_PLAY);
    uint32_t next_cd = _ui._controls.swiCD(Controls::SWI_NEXT);
    uint32_t prev_cd = _ui._controls.swiCD(Controls::SWI_PREV);

    // XXX If the MCU is asleep only the play/pause pushbutton will wake it.
    if (!running() && (play_cd != 0))
    {
        play();
    }
    else if (play_cd != 0)
    {
        if (play_cd >= _s_stop_time)
            stop();
        else
            _paused = !_paused;
    }
    else if ((prev_cd != 0) || (next_cd != 0))
    {
        if (!running() || paused())
            play();
    }

    if (!playing())
    {
        if ((_auto_stop_time != 0) && running())
            autoStop(!play_cd && !prev_cd && !next_cd);

        return;
    }

    if (_track->eof() || (prev_cd != 0) || (next_cd != 0))
    {
        if (_track->eof())
        {
            // Only save file index to eeprom if track was played to completion
            (void)_ui._eeprom.setTrack(_current_track);
            _next_track = skipTracks(1);
        }
        else
        {
            _audio.cancel(_track);
            if (!skipping())
            {
                int32_t s = skip(next_cd) - skip(prev_cd);
                if ((s < 0) && rewind()) s++;  // Increment so as not to count it
                _next_track = skipTracks(s);
            }
        }

        if (!newTrack())
            return;
    }

    if (_audio.ready() && !_audio.send(_track, 32) && !_track->valid())
    {
        _ui._errno = ERR_PLAYER_AUDIO;
        _disabled = true;
    }
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

    _ui._errno = ERR_PLAYER_OPEN_FILE;
    _disabled = true;

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

    if (!_ui._rtc.alarmIsSet())
        _ui._rtc.alarmStart();
}

UI::Alarm::as_e UI::Alarm::process(bool snooze, bool stop)
{
    if (!_ui._rtc.alarmInProgress())
    {
        if (_state != AS_OFF)
            this->stop();
    }
    else if (stop)
    {
        this->stop();
    }
    else if (_state == AS_OFF)
    {
        check();
    }
    else if (_state == AS_WAKE)
    {
        if (!_ui._rtc.alarmIsAwake())
            this->stop();
        else if (!this->snooze(snooze) && !_alarm_music)
            (void)_ui._beeper.toggled();
    }
    else // (_state == AS_SNOOZE)
    {
        wake();
    }

    return _state;
}

void UI::Alarm::check(void)
{
    if (!_ui._rtc.alarmIsAwake())
        return;

    if (_ui._rtc.alarmIsMusic()
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

    _state = AS_WAKE;
}

bool UI::Alarm::snooze(bool force)
{
    if (!force && !_ui._controls.touched())
        return false;

    if (_alarm_music)
        _ui._player.pause();
    else
        _ui._beeper.stop();

    if (!_ui._rtc.alarmSnooze())
        return false;

    _state = AS_SNOOZE;
    return true;
}

void UI::Alarm::wake(void)
{
    if (!_ui._rtc.alarmIsAwake())
        return;

    if (_alarm_music)
        _ui._player.play();
    else
        _ui._beeper.start();

    _state = AS_WAKE;
}

void UI::Alarm::stop(void)
{
    if (_alarm_music)
        _ui._player.stop();
    else
        _ui._beeper.stop();

    _ui._rtc.alarmStop();
    _ui._rtc.alarmStart();
    _state = AS_OFF;
}

////////////////////////////////////////////////////////////////////////////////
// Alarm END ///////////////////////////////////////////////////////////////////
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
// SetAlarm START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void UI::SetAlarm::uisBegin(void)
{
    _ui._rtc.getAlarm(_alarm);
    _state = _alarm.enabled ? SAS_HOUR : SAS_DISABLED;
    _blink.reset(_s_set_blink_time);
    _updated = _done = false;
    display();
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
        default: _ui._display.showString("...."); break;
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
void UI::SetClock::uisBegin(void)
{
    (void)_ui._rtc.update();
    _ui._rtc.getClock(_clock);
    _clock.second = 0;

    _state = SCS_TYPE;

    _blink.reset(_s_set_blink_time);
    _updated = _done = false;
    display();
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
        default: _ui._display.showString("...."); break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// SetClock END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SetSleep START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
constexpr char const * const UI::SetSleep::_s_opts[SOPT_CNT];

void UI::SetSleep::uisBegin(void)
{
    init();

    _state = SSS_MCU;
    changeMcu();

    _blink.reset(_s_set_blink_time);
    _updated = _done = false;
    display();
}

void UI::SetSleep::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::SetSleep::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    _updated = true;
    MFC(_update_actions[_state])(ev);
    display();
}

void UI::SetSleep::uisChange(void)
{
    _state = _next_states[_state];

    if (_change_actions[_state] != nullptr)
        MFC(_change_actions[_state])();

    if (_state == SSS_DONE)
        _blink.reset(_s_done_blink_time);
    else
        _blink.reset(_s_set_blink_time);

    display();
}

void UI::SetSleep::uisReset(ps_e ps)
{
    uisBegin();
}

void UI::SetSleep::uisEnd(void)
{
    if (_updated) set();
    _ui._display.blank();
}

bool UI::SetSleep::uisSleep(void)
{
    return true;
}

void UI::SetSleep::uisRefresh(void)
{
    _blink.reset();
    display();
}

uint32_t UI::SetSleep::getMcuTime(void)
{
    return _sleep.mcu_secs * 1000;
}

uint32_t UI::SetSleep::getDisplayTime(void)
{
    return _sleep.display_secs * 1000;
}

uint32_t UI::SetSleep::getPlayerTime(void)
{
    return _sleep.player_secs * 1000;
}

void UI::SetSleep::init(void)
{
    if (!_ui._eeprom.getSleep(_sleep))
    {
        _sleep.mcu_secs = 0;
        _sleep.display_secs = 0;
        _sleep.player_secs = 0;
    }

    initTime(_sleep.mcu_secs, _mcu_hm, _mcu_ms, _mcu_hm_flag);
    initTime(_sleep.display_secs, _display_hm, _display_ms, _display_hm_flag);
    initTime(_sleep.player_secs, _player_hm, _player_ms, _player_hm_flag);
}

void UI::SetSleep::initTime(uint32_t secs, uint8_t & hm, uint8_t & ms, df_t & hm_flag)
{
    uint32_t mins = secs / 60;

    if (mins < 60)
    {
        hm_flag = DF_NONE;
        hm = (uint8_t)mins;
        ms = (uint8_t)(secs % 60);

        if ((hm == 0) && (ms != 0) && (ms < _min_ms))
            ms = 0;
    }
    else
    {
        hm_flag = DF_HM;
        hm = (uint8_t)(mins / 60);
        ms = (uint8_t)(mins % 60);
    }
}

void UI::SetSleep::set(void)
{
    if (_mcu_hm_flag == DF_NONE)
        _sleep.mcu_secs = (_mcu_hm * 60) + _mcu_ms;
    else
        _sleep.mcu_secs = (_mcu_hm * 60 * 60) + (_mcu_ms * 60);

    if (_display_hm_flag == DF_NONE)
        _sleep.display_secs = (_display_hm * 60) + _display_ms;
    else
        _sleep.display_secs = (_display_hm * 60 * 60) + (_display_ms * 60);

    if (_player_hm_flag == DF_NONE)
        _sleep.player_secs = (_player_hm * 60) + _player_ms;
    else
        _sleep.player_secs = (_player_hm * 60 * 60) + (_player_ms * 60);

    (void)_ui._eeprom.setSleep(_sleep);

    _ui.setMcuSleep(_sleep.mcu_secs * 1000);
    _ui.setDisplaySleep(_sleep.display_secs * 1000);
    _ui._player.setAutoStop(_sleep.player_secs * 1000);
}

void UI::SetSleep::waitOpt(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetSleep::waitHM(bool on)
{
    display(on ? DF_NONE : DF_BL_BC);
}

void UI::SetSleep::waitMS(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetSleep::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetSleep::updateMcu(ev_e ev)
{
    _state = SSS_MCU_HM;
}

void UI::SetSleep::updateDisplay(ev_e ev)
{
    _state = SSS_DISPLAY_HM;
}

void UI::SetSleep::updatePlayer(ev_e ev)
{
    _state = SSS_PLAYER_HM;
}

void UI::SetSleep::updateHM(ev_e ev)
{
    if ((*_hm_flag != DF_NONE) && (*_hm == 1) && (ev == EV_NEG))
    {
        *_hm_flag = DF_NONE;
        *_hm = 59;
    }
    else if ((*_hm_flag == DF_NONE) && (*_hm == 59) && (ev == EV_POS))
    {
        *_hm_flag = DF_HM;
        *_hm = 1;
    }
    else
    {
        evUpdate < uint8_t > (*_hm, ev, 0, 99, false);
    }
}

void UI::SetSleep::updateMS(ev_e ev)
{
    evUpdate < uint8_t > (*_ms, ev, 0, 59);

    if ((*_ms > 0) && (*_ms < _min_ms))
    {
        if (ev == EV_NEG)
            *_ms = 0;
        else
            *_ms = _min_ms;
    }
}

void UI::SetSleep::changeMcu(void)
{
    _sopt = SOPT_MCU;
    _hm = &_mcu_hm;
    _ms = &_mcu_ms;
    _hm_flag = &_mcu_hm_flag;
}

void UI::SetSleep::changeDisplay(void)
{
    _sopt = SOPT_DISPLAY;
    _hm = &_display_hm;
    _ms = &_display_ms;
    _hm_flag = &_display_hm_flag;
}

void UI::SetSleep::changePlayer(void)
{
    _sopt = SOPT_PLAYER;
    _hm = &_player_hm;
    _ms = &_player_ms;
    _hm_flag = &_player_hm_flag;
}

void UI::SetSleep::changeMS(void)
{
    if ((*_hm == 0) && (*_hm_flag == DF_NONE))
        _min_ms = _s_min_secs;
    else
        _min_ms = 0;

    if ((*_ms != 0) && (*_ms < _min_ms))
        *_ms = _min_ms;
}

void UI::SetSleep::changeDone(void)
{
    set();
    _done = true;
    _updated = false;
}

void UI::SetSleep::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::SetSleep::displayOpt(df_t flags)
{
    _ui._display.showString(_s_opts[_sopt], flags);
}

void UI::SetSleep::displayTime(df_t flags)
{
    flags |= *_hm_flag;
    _ui._display.showTimer(*_hm, *_ms, (*_hm == 0) ? (flags | DF_BL0) : (flags | DF_NO_LZ));
}

void UI::SetSleep::displayDone(df_t flags)
{
    static constexpr uint8_t const nopts = 6;
    static uint32_t i = 0;

    if (_done) { i = 0; _done = false; }
    else if (flags == DF_NONE) i++;
    else return;

    uint8_t sw = i % (nopts + 1);

    if (sw == 0)      changeMcu();
    else if (sw == 2) changeDisplay();
    else if (sw == 4) changePlayer();

    switch (sw)
    {
        case 0: case 2: case 4: displayOpt(); break;
        case 1: case 3: case 5: ((*_hm == 0) && (*_ms == 0)) ? _ui._display.showOff() : displayTime(); break;
        default: _ui._display.showString("...."); break;
    }
}
////////////////////////////////////////////////////////////////////////////////
// SetSleep END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Clock START /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void UI::Clock::uisBegin(void)
{
    _state = CS_TIME;
    _track_updated = false;
    _alt_display.reset(_s_flash_time);
    _dflag = _ui._rtc.clockIs12H() ? DF_12H : DF_24H;

    clockUpdate(true);

    display();
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
        else if (_ui._controls.touchTime() >= _s_touch_time)
        {
            _state = CS_TRACK;
            changeTrack();
            display();
        }
    }
}

void UI::Clock::uisUpdate(ev_e ev)
{
    if (_state != CS_TRACK)
    {
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
    if ((_state != CS_TRACK) || !_track_updated)
        _state = _next_states[_state];

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
    _ui._display.blank();
}

bool UI::Clock::uisSleep(void)
{
    return true;
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
uint32_t volatile UI::Timer::_s_seconds = 0;
uint8_t volatile UI::Timer::_s_hue = UI::Timer::_s_hue_max;
uint32_t volatile UI::Timer::_s_hue_calls = 0;
uint8_t volatile UI::Timer::_s_hue_interval = 1;

void UI::Timer::timerDisplay(void)
{
    if (_s_seconds == 0)
        return;

    _s_seconds--;
}

void UI::Timer::timerLeds(void)
{
    if (_s_hue == _s_hue_min)
        return;

    if ((++_s_hue_calls % _s_hue_interval) == 0)
        _s_hue--;
}

void UI::Timer::uisBegin(void)
{
    _state = TS_SET_HM;
    _show_clock = false;

    _display_timer = Pit::acquire(timerDisplay, _s_seconds_interval);
    _led_timer = Pit::acquire(timerLeds, _s_seconds_interval);  // Will get updated

    if ((_display_timer == nullptr) || (_led_timer == nullptr))
    {
        _ui._errno = ERR_TIMER;
        return;
    }

    _blink.reset();
    displaySetTimer();
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
        if (_state == TS_WAIT)
            _state = TS_SET_HM;

        MFC(_update_actions[_state])(ev);

        _blink.reset();
        displaySetTimer();

        return;
    }

    if (_state == TS_ALERT)
        return;

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
            displayTimer();
    }

    uisWait();
}

void UI::Timer::uisChange(void)
{
    if ((_state != TS_SET_HM) && (_hm == 0) && (_ms == 0))
        return;

    // Action comes before changing state
    MFC(_change_actions[_state])();

    _state = _next_states[_state];
}

void UI::Timer::uisReset(ps_e ps)
{
    stop();
    reset();

    if (_state <= TS_WAIT)
        _state = TS_SET_HM;
    else
        _state = TS_WAIT;

    _show_clock = false;
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
        displaySetTimer();
    else if (_show_clock)
        clockUpdate(true);
    else
        timerUpdate(true);
}

void UI::Timer::waitHM(void)
{
    if (!_blink.toggled())
        return;

    displaySetTimer(_blink.on() ? DF_NONE : DF_BL_BC);
}

void UI::Timer::waitMS(void)
{
    if (!_blink.toggled())
        return;

    displaySetTimer(_blink.on() ? DF_NONE : DF_BL_AC);
}

void UI::Timer::updateHM(ev_e ev)
{
    if ((_hm_flag != DF_NONE) && (_hm == 1) && (ev == EV_NEG))
    {
        _hm_flag = DF_NONE;
        _hm = 59;
    }
    else if ((_hm_flag == DF_NONE) && (_hm == 59) && (ev == EV_POS))
    {
        _hm_flag = DF_HM;
        _hm = 1;
    }
    else
    {
        evUpdate < uint8_t > (_hm, ev, 0, 99, false);
    }
}

void UI::Timer::updateMS(ev_e ev)
{
    evUpdate < uint8_t > (_ms, ev, _min_ms, 59);
}

void UI::Timer::displaySetTimer(df_t flags)
{
    flags |= _hm_flag;
    _ui._display.showTimer(_hm, _ms, (_hm == 0) ? (flags | DF_BL0) : (flags | DF_NO_LZ));
}

void UI::Timer::displayTimer(void)
{
    _ui._display.showTimer(_hm, _ms, DF_NO_LZ | _hm_flag);
}

void UI::Timer::changeHM(void)
{
    if (_hm == 0)
        _min_ms = 1;
    else
        _min_ms = 0;

    if (_ms < _min_ms)
        _ms = _min_ms;

    _blink.reset();
    displaySetTimer();
}

void UI::Timer::start(void)
{
    displayTimer();

    if (_hm_flag == DF_NONE)
        _s_seconds = ((uint32_t)_hm * 60) + (uint32_t)_ms;
    else
        _s_seconds = ((uint32_t)_hm * 60 * 60) + ((uint32_t)_ms * 60);

    uint32_t minutes = _s_seconds / 60;
    if (minutes > (10 * 60))
        _s_hue_interval = 64;
    else if (minutes > 60)
        _s_hue_interval = 8;
    else
        _s_hue_interval = 1;

    _led_timer->updateInterval((_s_seconds * _s_us_per_hue) / _s_hue_interval, false);

    _last_second = _s_seconds;
    _hm_toggle = true;

    _s_hue_calls = 0;
    _s_hue = _s_hue_max;
    _last_hue = _s_hue;
    _ui._lighting.state(Lighting::LS_OTHER);
    _ui._lighting.setColor(CHSV(_last_hue, 255, 255));

    _display_timer->start();
    _led_timer->start();
}

void UI::Timer::reset(void)
{
    _ui._beeper.stop();
    _ui._lighting.state(Lighting::LS_NIGHT_LIGHT);
    _blink.reset();
    displayTimer();
}

void UI::Timer::run(void)
{
    if (!_show_clock || (_s_seconds == 0))
        timerUpdate();

    hueUpdate();

    if (_last_second == 0)
    {
        stop();
        _show_clock = false;
        _ui._beeper.start();
        _state = TS_ALERT;
    }
}

void UI::Timer::timerUpdate(bool force)
{
    // Not critical that interrupts be disabled
    uint32_t second = _s_seconds;

    if (!force && (second == _last_second))
        return;

    _last_second = second;

    df_t hm_flag;
    uint8_t hm, ms;
    uint32_t minute = second / 60;
    if (minute < 60)
    {
        hm_flag = DF_NONE;
        hm = (uint8_t)minute;
        ms = (uint8_t)(second % 60);
    }
    else
    {
        _hm_toggle = !_hm_toggle;
        hm_flag = _hm_toggle ? DF_HM : DF_NONE;
        hm = (uint8_t)(minute / 60);
        ms = (uint8_t)(minute % 60);
    }

    if (_ui._refresh_start == 0)
        _ui._display.showTimer(hm, ms, DF_NO_LZ | hm_flag);
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
    uint8_t hue = _s_hue;

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
    if (_ui._controls.touched())
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
// SetTouch START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
constexpr char const * const UI::SetTouch::_s_topts[TOPT_CNT];

void UI::SetTouch::uisBegin(void)
{
    _ui._tsi.getConfig(_touch);
    _state = (_touch.threshold == 0) ? TS_DISABLED : TS_OPT;
    _topt = TOPT_CAL;
    _ui._lighting.state(Lighting::LS_OTHER);
    reset();
}

void UI::SetTouch::uisWait(void)
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

void UI::SetTouch::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    MFC(_update_actions[_state])(ev);
    display();
}

void UI::SetTouch::uisChange(void)
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
            case TOPT_DEF: _ui._tsi.defaults(_touch); _state = TS_DONE; _topt = TOPT_CONF; break;
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

void UI::SetTouch::uisReset(ps_e ps)
{
    if (_state != TS_OPT)
    {
        if ((_state == TS_DONE) || (_state == TS_NSCN) || (_state == TS_READ)
                || ((_state == TS_CAL_START) && (_topt == TOPT_CAL)))
            _state = TS_OPT;
        else if (_state == TS_DISABLED)
            toggleEnabled();
        else if (_topt == TOPT_CAL)
            _state = TS_CAL_START;
        else if (_topt == TOPT_CONF)
            _state = TS_NSCN;
        else if (_topt == TOPT_READ)
            _state = TS_READ;
    }

    if (_state == TS_OPT)
        _topt = TOPT_CAL;

    if (_state == TS_CAL_START)
        _ui._lighting.setColor(CHSV(0, 255, 255));
    else
        _ui._lighting.setColor(CRGB::BLACK);

    reset();
}

void UI::SetTouch::uisEnd(void)
{
    _ui._lighting.state(Lighting::LS_NIGHT_LIGHT);
    _ui._display.blank();
}

bool UI::SetTouch::uisSleep(void)
{
    return ((_state != TS_CAL_UNTOUCHED) && (_state != TS_CAL_TOUCHED));
}

void UI::SetTouch::uisRefresh(void)
{
    _blink.reset();
    display();
}

void UI::SetTouch::reset(void)
{
    _readings = 0;
    _untouched.reset();
    _ui._beeper.stop();
    _ui._player.stop();

    _done = false;

    _blink.reset();
    display();
}

void UI::SetTouch::toggleEnabled(void)
{
    if (_touch.threshold == 0)
        _ui._tsi.defaults(_touch);
    else
        _touch.threshold = 0;
    _state = (_touch.threshold == 0) ? TS_DISABLED : TS_OPT;
    _topt = TOPT_CAL;
    (void)_ui._tsi.setConfig(_touch);
}

void UI::SetTouch::waitOpt(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetTouch::waitNscn(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetTouch::waitPs(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetTouch::waitRefChrg(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetTouch::waitExtChrg(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetTouch::waitCalStart(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetTouch::waitCalUntouched(bool on)
{
    if (on) display();

    uint16_t read = _ui._tsi_channel.read();
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

void UI::SetTouch::waitCalThreshold(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetTouch::waitCalTouched(bool on)
{
    if (on) display();

    uint16_t read = _ui._tsi_channel.read();
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

void UI::SetTouch::waitRead(bool on)
{
    uint16_t read = _ui._tsi_channel.read();
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

void UI::SetTouch::waitTest(bool on)
{
    static uint32_t beep_start = 0;

    uint16_t read = _ui._tsi_channel.read();
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

void UI::SetTouch::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetTouch::updateOpt(ev_e ev)
{
    evUpdate < topt_e > (_topt, ev, TOPT_CAL, (topt_e)(TOPT_CNT - 1));
}

void UI::SetTouch::updateNscn(ev_e ev)
{
    evUpdate < uint8_t > (_touch.nscn, ev, 0, Tsi::maxNscn());
}

void UI::SetTouch::updatePs(ev_e ev)
{
    evUpdate < uint8_t > (_touch.ps, ev, 0, Tsi::maxPs());
}

void UI::SetTouch::updateRefChrg(ev_e ev)
{
    evUpdate < uint8_t > (_touch.refchrg, ev, 0, Tsi::maxRefChrg());
}

void UI::SetTouch::updateExtChrg(ev_e ev)
{
    evUpdate < uint8_t > (_touch.extchrg, ev, 0, Tsi::maxExtChrg());
}

void UI::SetTouch::updateThreshold(ev_e ev)
{
    evUpdate < uint16_t > (_touch.threshold, ev, _untouched.hi, Tsi::maxThreshold(), false);
    _ui._lighting.setColor(CHSV(_touch.threshold % 256, 255, 255));
}

void UI::SetTouch::changeCalStart(void)
{
    if (_topt == TOPT_CONF)
        (void)_ui._tsi.configure(_touch.nscn, _touch.ps, _touch.refchrg, _touch.extchrg);

    _ui._lighting.setColor(CHSV(0, 255, 255));
    _readings = 0;
    _untouched.reset();
    _ui._player.stop();
}

void UI::SetTouch::changeCalThreshold(void)
{
    _ui._lighting.setColor(CHSV(0, 255, 255));
    _readings = 0;
    _touched_amp_off.reset();
    _touched_amp_on.reset();
    _touched = &_touched_amp_off;
}

void UI::SetTouch::changeRead(void)
{
    _read.reset();
    _untouched.reset();
}

void UI::SetTouch::changeDone(void)
{
    _ui._lighting.setColor(CRGB::BLACK);
    _ui._beeper.stop();
    (void)_ui._tsi.setConfig(_touch);
    _done = true;
}

void UI::SetTouch::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::SetTouch::displayOpt(df_t flags)
{
    _ui._display.showString(_s_topts[_topt], flags);
}

void UI::SetTouch::displayNscn(df_t flags)
{
    _ui._display.showOption("nS", Tsi::valNscn(_touch.nscn), flags);
}

void UI::SetTouch::displayPs(df_t flags)
{
    _ui._display.showOption("PS", Tsi::valPs(_touch.ps), flags | DF_HEX);
}

void UI::SetTouch::displayRefChrg(df_t flags)
{
    _ui._display.showOption("rC", Tsi::valRefChrg(_touch.refchrg), flags);
}

void UI::SetTouch::displayExtChrg(df_t flags)
{
    _ui._display.showOption("EC", Tsi::valExtChrg(_touch.extchrg), flags);
}

void UI::SetTouch::displayCalStart(df_t flags)
{
    _ui._display.showString("bASE", flags);
}

void UI::SetTouch::displayCalThreshold(df_t flags)
{
    _ui._display.showString("tUCH", flags);
}

void UI::SetTouch::displayCalWait(df_t flags)
{
    _ui._display.showInteger(_readings, flags | DF_HEX);
}

void UI::SetTouch::displayThreshold(df_t flags)
{
    _ui._display.showInteger(_touch.threshold, flags | DF_HEX);
}

void UI::SetTouch::displayDone(df_t flags)
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
            default: _ui._display.showString("...."); break;
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

void UI::SetTouch::displayDisabled(df_t flags)
{
    _ui._display.showOff(flags);
}

////////////////////////////////////////////////////////////////////////////////
// SetTouch END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SetLeds START ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
constexpr CRGB::Color const UI::SetLeds::_s_colors[];

void UI::SetLeds::uisBegin(void)
{
    _state = SLS_TYPE;
    _blink.reset();
    _updated = _done = false;
    _ui._lighting.state(Lighting::LS_SET);
    display();
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
