#include "ui.h"
#include "rtc.h"
#include "pit.h"
#include "tsi.h"
#include "types.h"
#include "utility.h"

////////////////////////////////////////////////////////////////////////////////
// Controls START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
Controls::Controls(void)
{
    setPos();
    _pos_last = _pos;
    _pos_change = false;
}

void Controls::process(void)
{
    setPos();
    setTouch();

    __disable_irq();

    _bri_turn = _bri_enc.turn();
    _enc_turn = _swi_enc.turn();

    _enc_swi.process();
    _play.process();
    _next.process();
    _prev.process();

    __enable_irq();

    _enc_swi.postProcess();
    _play.postProcess();
    _next.postProcess();
    _prev.postProcess();

    _interaction = _pos_change
        || (_bri_turn != EV_ZERO) || (_enc_turn != EV_ZERO)
        || _enc_swi.closed() || _enc_swi.pressed()
        || _play.closed() || _play.pressed()
        || _next.closed() || _next.pressed()
        || _prev.closed() || _prev.pressed();
}

bool Controls::interaction(bool inc_touch) const
{
    if (inc_touch)
        return _interaction || touching();

    return _interaction;
}

ev_e Controls::turn(enc_e enc) const
{
    switch (enc)
    {
        case ENC_SWI: return _enc_turn;
        case ENC_BRI: return _bri_turn;
    }

    return EV_ZERO;
}

bool Controls::closed(swi_e swi) const
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

bool Controls::pressed(swi_e swi) const
{
    switch (swi)
    {
        case SWI_ENC: return _enc_swi.pressed();
        case SWI_PLAY: return _play.pressed();
        case SWI_NEXT: return _next.pressed();
        case SWI_PREV: return _prev.pressed();
    }

    return false;
}

bool Controls::doublePressed(swi_e swi) const
{
    switch (swi)
    {
        case SWI_ENC: return _enc_swi.doublePressed();
        case SWI_PLAY: return _play.doublePressed();
        case SWI_NEXT: return _next.doublePressed();
        case SWI_PREV: return _prev.doublePressed();
    }

    return false;
}

uint32_t Controls::pressTime(swi_e swi) const
{
    switch (swi)
    {
        case SWI_ENC: return _enc_swi.pressTime();
        case SWI_PLAY: return _play.pressTime();
        case SWI_NEXT: return _next.pressTime();
        case SWI_PREV: return _prev.pressTime();
    }

    return 0;
}

bool Controls::valid(con_e con) const
{
    switch (con)
    {
        case CON_RS: return _rs.valid();
        case CON_ENC_SWI: return _enc_swi.valid();
        case CON_ENC_BRI: return _bri_enc.valid();
        case CON_SWI_PLAY: return _play.valid();
        case CON_SWI_NEXT: return _next.valid();
        case CON_SWI_PREV: return _prev.valid();
    }

    return false;
}

bool Controls::valid(void) const
{
    return valid(CON_RS) && valid(CON_ENC_BRI) && valid(CON_ENC_SWI)
        && valid(CON_SWI_PLAY) && valid(CON_SWI_NEXT) && valid(CON_SWI_PREV);
}

void Controls::setPos(void)
{
    if (_pos != RSP_NONE)
        _pos_last = _pos;

    int8_t pos = _rs.position();
    _pos = RSP_NONE;
    if      (pos == 0) _pos = RSP_RIGHT;
    else if (pos == 1) _pos = RSP_LEFT;
    else if (pos == 2) _pos = RSP_MIDDLE;

    if (_pos != RSP_NONE)
        _pos_change = (_pos != _pos_last);
}

void Controls::touchEnable(void)
{
    _touch_enabled = true;
}

void Controls::touchDisable(void)
{
    _touch_enabled = _touched = false;
    _touch_mark = _touched_time = 0;
}

void Controls::setTouch(void)
{
    if (!_touch_enabled)
        return;

    _touched = false;
    _touched_time = 0;

    uint16_t thr = _tsi.threshold();
    if (thr == 0)
        return;

    // Return value of 0 means it's in the process of scanning
    uint16_t tval = _tsi_channel.read();
    if (tval == 0)
        return;

    if ((_touch_mark == 0) && (tval >= thr))
    {
        _touch_mark = msecs();
    }
    else if ((_touch_mark != 0) && (tval < thr))
    {
        _touched_time = msecs() - _touch_mark;

        if (_touched_time > _s_touch_time)
            _touched = true;

        _touch_mark = 0;
    }
}

bool Controls::touching(void) const
{
    return (_touch_mark != 0) && ((msecs() - _touch_mark) > _s_touch_time);
}

bool Controls::touched(void) const
{
    return _touched;
}

uint32_t Controls::touchingTime(void) const
{
    return (_touch_mark == 0) ? 0 : msecs() - _touch_mark;
}

uint32_t Controls::touchedTime(void) const
{
    return _touched_time;
}

void Controls::reset(void)
{
    _bri_turn = _enc_turn = EV_ZERO;
    _pos = _pos_last = RSP_NONE;
    _enc_swi.reset();
    _play.reset();
    _next.reset();
    _prev.reset();
}

////////////////////////////////////////////////////////////////////////////////
// Controls END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UI START ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::UI(void)
{
    if (!valid())
        return;

    _display.brightness(BR_MID);
    _display_brightness = _display.brightness();

    // Set initial state
    if (_controls.pos() == RSP_NONE)
    {
        _errno = ERR_HW_RS;
        return;
    }

    _state = _next_states[UIS_CLOCK][PS_NONE][_controls.pos()];
    _states[_state]->uisBegin();
}

bool UI::valid(void)
{
    static Tsi & _tsi = Tsi::acquire();

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

    if (!_controls.valid(CON_RS))
        _errno = ERR_HW_RS;
    else if (!_controls.valid(CON_ENC_SWI))
        _errno = ERR_HW_ENC_SWI;
    else if (!_controls.valid(CON_ENC_BRI))
        _errno = ERR_HW_BR_ENC;
    else if (!_controls.valid(CON_SWI_PLAY))
        _errno = ERR_HW_PLAY;
    else if (!_controls.valid(CON_SWI_NEXT))
        _errno = ERR_HW_NEXT;
    else if (!_controls.valid(CON_SWI_PREV))
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
    // Controls and UI State
    _controls.process();

    ev_e enc_turn = _controls.turn(ENC_SWI);

    es_e es = ES_NONE;
    ps_e ps = PS_NONE;

    if (_controls.closed(SWI_ENC))
    {
        es = ES_DEPRESSED;
    }
    else if (_controls.pressed(SWI_ENC))
    {
        if (!wait_released)
        {
            es = ES_PRESSED;
            ps = pressState(_controls.pressTime(SWI_ENC));
        }
        else
        {
            wait_released = false;
        }
    }
    else if (enc_turn != EV_ZERO)
    {
        es = ES_TURNED;
    }

    // Set UI state
    uis_e slast = _state;
    _state = _next_states[slast][ps][_controls.pos()];

    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Player
    _player.process();

    if (_player.stopping())
        display("STOP");
    else if (_player.skipping())
        display(_player.nextTrack() + 1);
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Lighting Animation
    _lighting.paletteNL();
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Power
    _power.process();

    if (_power.napping() && !_display.isAwake())
        return;

    if (_power.slept()
            || (_power.napped() && (_states[_state]->uisNap() == SC_OFF) && (slast == _state)))
    {
        // If slept, ES_DEPRESSED won't be set so have to check if closed.
        if (_controls.closed(SWI_ENC))
            wait_released = true;

        return;
    }
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Brightness
    updateBrightness(_controls.turn(ENC_BRI));
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Alarm
    _alarm.process(enc_turn != EV_ZERO, es == ES_DEPRESSED);

    if (_alarm.stopped())
    {
        display("OFF");
        if (es == ES_DEPRESSED)
            wait_released = true;
    }
    else if (_alarm.snoozed())
    {
        display("Snoo");
        if (enc_turn != EV_ZERO)
            enc_turn = EV_ZERO;
    }
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Pressing - PS_RESET or longer
    if (pressing(es))
    {
        wait_released = false;
        return;
    }
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // UI State processing
    if (_state != slast)
    {
        // Encoder switch press caused state change so don't process it
        if (es == ES_PRESSED)
            es = ES_NONE;

        _states[slast]->uisEnd();

        // Deferred wake to be in sync with state change after Power sleep
        if (_lighting.isSleeping())
        {
            _lighting.brightness(_brightness);
            _lighting.wake();
        }

        _states[_state]->uisBegin();

        _display_mark = 0;
    }
    else if (displaying())
    {
        // Still show timer hue change though it has to be aware to not alter display.
        if ((_state == UIS_TIMER) && _timer.running())
            _timer.uisWait();

        if (!refresh())
            return;
    }

    switch (es)
    {
        case ES_NONE:
        case ES_DEPRESSED:
            _states[_state]->uisWait();
            break;

        case ES_TURNED:
            _states[_state]->uisUpdate(enc_turn);
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

bool UI::pressing(es_e encoder_state)
{
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

    ps_e ps = pressState(_controls.pressTime(SWI_ENC));

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
                _lighting.toggleAni();
                _lighting.onNL();
                ds = DS_WAIT_RELEASED;
            }
            else
            {
                t.reset();
                display_type = DT_2;
                ds = DS_WAIT_ALT_STATE;
            }
            break;

        case DS_WAIT_ALT_STATE:
            if (ps <= PS_STATE)
                blink(display_type);
            else
                ds = DS_ALT_STATE;
            break;

        case DS_ALT_STATE:
            if (_state != UIS_CLOCK)
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

void UI::display(char const * str)
{
    _display_mark = msecs();
    _display.showString(str);
}

void UI::display(int32_t n)
{
    _display_mark = msecs();
    _display.showInteger(n);
}

bool UI::refresh(void)
{
    if (!displaying() || ((msecs() - _display_mark) < _s_display_wait))
        return false;

    _states[_state]->uisRefresh();
    _display_mark = 0;

    return true;
}

bool UI::canNap(void)
{
    return (_states[_state]->uisNap() != SC_ON) && !displaying()
        && !_rtc.alarmIsAwake() && !_controls.interaction();
}

bool UI::canStop(void)
{
    return _player.paused() && !displaying() && !_controls.interaction();
}

bool UI::canSleep(void)
{
    return canNap() && (_player.paused() || _player.stopped())
        && (_states[_state]->uisSleep() == SC_OFF);
}

void UI::napScreens(void)
{
    if (_states[_state]->uisNap() == SC_DIM)
    {
        if (_display.isAwake() && _display.isOn())
            _display.brightness(BR_LOW);

        if (_lighting.isOn() && (_brightness > _s_dim_br))
            _lighting.brightness(_s_dim_br);
    }
    else if (_states[_state]->uisNap() == SC_OFF)
    {
        _display.sleep(true);
        _lighting.sleep();
    }
}

void UI::sleepScreens(void)
{
    if (_states[_state]->uisSleep() != SC_OFF)
        return;

    _display.sleep(true);
    _lighting.sleep();
}

void UI::wakeScreens(int8_t wakeup_pin)
{
    // If brightness encoder was turned, treat it as a brightness update.
    bool br_turn = (_controls.turn(ENC_BRI) != EV_ZERO) || (wakeup_pin == PIN_EN_BR_A);

    // If rotary switch was repositioned, state is going to change so only
    // update screens if not repositioned.
    bool refresh = !_controls.posChange() && (wakeup_pin != PIN_RS_M);

    if (!_display.isAwake())
    {
        _display.wake(false);
        if (br_turn)
        {
            _display_brightness = 0;
            _display.brightness(_display_brightness);
            _display.off();
        }
        else
        {
            _display.brightness(_display_brightness);
        }

        if (refresh)
            _states[_state]->uisRefresh();
    }
    else if (br_turn)
    {
        _display_brightness = _display.brightness();
    }
    else
    {
        _display.brightness(_display_brightness);
    }

    if (_lighting.isSleeping())
    {
        if (br_turn)
        {
            _brightness = 0;
            _lighting.brightness(_brightness);
            _lighting.wake();
        }
        else if (refresh)
        {
            _lighting.brightness(_brightness);
            _lighting.wake();
        }
    }
    else if (br_turn)
    {
        _brightness = _lighting.brightness();
    }
    else
    {
        _lighting.brightness(_brightness);
    }
}

////////////////////////////////////////////////////////////////////////////////
// UI END //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Power START /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::Power::Power(UI & ui)
    : _ui{ui}
{
    _stop_mark = _rest_mark = msecs();

    if (!_ui._eeprom.getPower(_power))
        _power.nap_secs = _power.stop_secs = _power.sleep_secs = _power.touch_secs = 0;
}

void UI::Power::process(void)
{
    if ((_state == PS_SLEPT) || (_state == PS_NAPPED))
        _state = PS_AWAKE;

    if ((awake() || napping()) && canSleep())
    {
        sleep();

        if (_state == PS_SLEPT)
        {
            _rest_mark = _stop_mark = msecs();
            _touch_mark = 0;
            return;
        }
    }

    bool can_nap = canNap();

    if (napping() && !can_nap)
        wake();
    else if (awake() && can_nap)
        nap();
    else if (!_ui.canNap())
        _rest_mark = msecs();

    if (canStop())
        _ui._player.stop();
    else if (!_ui.canStop())
        _stop_mark = msecs();
}

bool UI::Power::canSleep(void)
{
    if (_power.touch_secs != 0)
    {
        bool touching = _ui._controls.touching();

        if (!touching && (_touch_mark != 0))
            _touch_mark = 0;
        else if (touching && ((_touch_mark == 0) || _ui._controls.interaction(false)))
            _touch_mark = msecs();
        else if ((_touch_mark != 0) && ((msecs() - _touch_mark) >= (_power.touch_secs * 1000)))
            return true;
    }

    return (_power.sleep_secs != 0) && _ui.canSleep()
        && ((msecs() - _rest_mark) >= (_power.sleep_secs * 1000));
}

bool UI::Power::canNap(void)
{
    return (_power.nap_secs != 0) && _ui.canNap()
        && ((msecs() - _rest_mark) >= (_power.nap_secs * 1000));
}

bool UI::Power::canStop(void)
{
    return _ui.canStop() && (_power.stop_secs != 0)
        && ((msecs() - _stop_mark) >= (_power.stop_secs * 1000));
}

void UI::Power::nap(void)
{
    _ui.napScreens();
    _state = PS_NAPPING;
}

void UI::Power::wake(void)
{
    _ui.wakeScreens(-1);
    _state = PS_NAPPED;
    _rest_mark = msecs();
}

void UI::Power::sleep(void)
{
    // Puts MCU in LLS (Low Leakage Stop) mode - a state retention power mode
    // where most peripherals are in state retention mode (with clocks stopped).
    // Current is reduced by about 44mA.  With the VS1053 off (~14mA) should get
    // a total of ~58mA savings putting things ~17mA which should be about what
    // the LED on the PowerBoost is consuming and the Neopixel strip which
    // despite being "off" still consumes ~7mA.

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
        return;
    }

    if (_ui._rtc.alarmEnabled() && !_llwu.alarmEnable())
    {
        _llwu.disable();
        return;
    }

    _ui.sleepScreens();
    _ui._player.stop();

    while (_ui._controls.touching()) _ui._controls.process();

    // Don't worry about touch not getting enabled since there are the encoders
    // and switches that can wake it up.
    (void)_llwu.tsiEnable < PIN_TOUCH > ();

    // Tell the clock that it needs to read the TSR counter since NVIC is disabled
    _ui._rtc.sleep();

    // Don't need to know the return value - whether module or pin.
    (void)_llwu.sleep();

    _llwu.disable();
    _ui._rtc.wake();

    // If the rotary switch is the wakeup source, state is going to change so
    // don't redisplay data from state prior to sleeping.
    // If a module was the wakeup source -1 will be returned as the wakeup pin.
    _ui.wakeScreens(_llwu.wakeupPin());

    // Don't restart player if it was stopped before sleeping.

    _state = PS_SLEPT;
}
////////////////////////////////////////////////////////////////////////////////
// Power END ///////////////////////////////////////////////////////////////////
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
        || _ui._controls.closed(SWI_PLAY)
        || _ui._controls.closed(SWI_NEXT)
        || _ui._controls.closed(SWI_PREV);
}

bool UI::Player::pressing(void)
{
    if (!_ui._controls.closed(SWI_PLAY)
            && !_ui._controls.closed(SWI_NEXT)
            && !_ui._controls.closed(SWI_PREV))
        return false;

    uint32_t play_ptime = _ui._controls.pressTime(SWI_PLAY);
    uint32_t next_ptime = _ui._controls.pressTime(SWI_NEXT);
    uint32_t prev_ptime = _ui._controls.pressTime(SWI_PREV);

    if ((play_ptime == 0) && (prev_ptime == 0) && (next_ptime == 0))
        return false;

    if (stopping())
        return true;

    if (running() && (play_ptime >= _s_stop_time))
    {
        _stopping = true;
        return true;
    }

    if ((next_ptime < _s_skip_msecs) && (prev_ptime < _s_skip_msecs))
        return true;

    if (next_ptime >= _s_skip_msecs)
        next_ptime -= _s_skip_msecs;

    if (prev_ptime >= _s_skip_msecs)
        prev_ptime -= _s_skip_msecs;

    int32_t s = skip(next_ptime) - skip(prev_ptime);
    if (s == 0) return true;

    _next_track = skipTracks(s);

    return true;
}

void UI::Player::process(void)
{
    if (disabled() || pressing())
        return;

    bool play_pressed = _ui._controls.pressed(SWI_PLAY);
    bool next_pressed = _ui._controls.pressed(SWI_NEXT);
    bool prev_pressed = _ui._controls.pressed(SWI_PREV);

    // XXX If the MCU is asleep only the play/pause pushbutton will wake it.
    if (!running() && play_pressed)
    {
        play();
    }
    else if (play_pressed)
    {
        if (_ui._controls.pressTime(SWI_PLAY) >= _s_stop_time)
            stop();
        else
            _paused = !_paused;
    }
    else if (next_pressed || prev_pressed)
    {
        if (!running() || paused())
            play();
    }

    if (!playing())
        return;

    if (_track->eof() || next_pressed || prev_pressed)
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
                uint32_t npt = _ui._controls.pressTime(SWI_NEXT);
                uint32_t ppt = _ui._controls.pressTime(SWI_PREV);

                int32_t s = skip(npt) - skip(ppt);
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
UI::Alarm::Alarm(UI & ui)
    : _ui{ui}
{
    _state = AS_OFF;

    if (!_ui._rtc.alarmIsSet())
        _ui._rtc.alarmStart();
}

void UI::Alarm::process(bool snooze, bool stop)
{
    if ((_state == AS_STARTED) || (_state == AS_WOKE))
        _state = AS_AWAKE;
    else if (_state == AS_SNOOZED)
        _state = AS_SNOOZE;
    else if (_state == AS_STOPPED)
        _state = AS_OFF;

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
    else if (_state == AS_AWAKE)
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

    _state = AS_STARTED;
}

bool UI::Alarm::snooze(bool force)
{
    if (!force && !_ui._controls.touching())
        return false;

    if (_alarm_music)
        _ui._player.pause();
    else
        _ui._beeper.stop();

    if (!_ui._rtc.alarmSnooze())
        return false;

    _state = AS_SNOOZED;
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

    _state = AS_WOKE;
}

void UI::Alarm::stop(void)
{
    if (_alarm_music)
        _ui._player.stop();
    else
        _ui._beeper.stop();

    _ui._rtc.alarmStop();
    _ui._rtc.alarmStart();

    _state = AS_STOPPED;
}

////////////////////////////////////////////////////////////////////////////////
// Alarm END ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Lighting START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::Lighting::Lighting(uint8_t brightness)
{
    for (uint8_t i = 0; i < EE_NLC_CNT; i++)
    {
        uint32_t color_code;
        if (_eeprom.getNLC(i, color_code))
            _nl_colors[i+1] = color_code;
    }

    removeDups();

    _leds.updateColor(CRGB::BLACK);
    this->brightness(brightness);
}

void UI::Lighting::onNL(void)
{
    if (_state != LS_NIGHT_LIGHT)
        return;

    _nl_on = true;

    if (_nl_ani)
    {
        _leds.startPalette(static_cast < Palette::pal_e > (_ani_index));
    }
    else
    {
        if (!isOn()) _active_index = 0;
        _leds.updateColor(_active_colors[_active_index]);
    }
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

void UI::Lighting::toggleAni(void)
{
    if (_state != LS_NIGHT_LIGHT)
        return;

    _nl_ani = !_nl_ani;
}

bool UI::Lighting::isOnNL(void) const
{
    return (_state == LS_NIGHT_LIGHT) && _nl_on;
}

void UI::Lighting::paletteNL(void)
{
    if ((_state != LS_NIGHT_LIGHT) || !_nl_on || !_nl_ani)
        return;

    _leds.updatePalette();
}

void UI::Lighting::cycleNL(ev_e ev)
{
    if ((_state != LS_NIGHT_LIGHT) || !_nl_on || (ev == EV_ZERO))
        return;

    if (_nl_ani)
    {
        evUpdate < uint8_t > (_ani_index, ev, 0, Palette::PAL_CNT - 1);
        _leds.startPalette(static_cast < Palette::pal_e > (_ani_index));
    }
    else
    {
        evUpdate < uint8_t > (_active_index, ev, 0, _active_cnt - 1);
        _leds.updateColor(_active_colors[_active_index]);
    }
}

void UI::Lighting::setNL(uint8_t index, CRGB const & c)
{
    if (index >= EE_NLC_CNT)
        return;

    _nl_colors[index+1] = c;
    removeDups(index + 1);
}

void UI::Lighting::removeDups(uint8_t index)
{
    for (uint8_t i = 0; i < EE_NLC_CNT + 1; i++)
        _active_colors[i] = _nl_colors[i];

    _active_cnt = EE_NLC_CNT + 1;

    for (uint8_t i = 0; i < _active_cnt; i++)
    {
        uint8_t ci = i + 1;
        for (uint8_t j = ci; j < _active_cnt; j++)
        {
            if (_active_colors[i] != _active_colors[j])
            {
                if (ci != j)
                    _active_colors[ci] = _active_colors[j];
                ci++;
            }
            else if (index == j)
            {
                index = i;
            }
        }

        _active_cnt = ci;
    }

    _active_index = (_active_colors[index] == CRGB::BLACK) ? 0 : index;
    if (_active_index != index) _nl_on = false;
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
    brightness(_brightness + 1);
}

void UI::Lighting::down(void)
{
    if (_brightness == 0) return;
    brightness(_brightness - 1);
}

void UI::Lighting::brightness(uint8_t b)
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
        if (_nl_on && isOn()) onNL();
        else if (!_nl_on) offNL();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Lighting END ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// HM START ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void UI::HM::init(uint32_t secs)
{
    uint32_t mins = secs / 60;

    if (mins < 60)
    {
        _hm = mins;
        _ms = (uint16_t)(secs % 60);
    }
    else
    {
        _ms = (uint16_t)(mins % 60);
        _hm = mins - _ms;

        if (_hm > (_s_max_hm)) _hm = _s_max_hm;
    }
}

void UI::HM::updateHM(ev_e ev)
{
    if (ev == EV_POS)
    {
        if (_hm == _s_max_hm)
            _hm = 0;
        else if (_hm >= 60)
            _hm += 60;
        else
            _hm += 1;
    }
    else // if (ev == EV_NEG)
    {
        if (_hm == 0)
            _hm = _s_max_hm;
        else if (_hm <= 60)
            _hm -= 1;
        else
            _hm -= 60;
    }
}

void UI::HM::updateMS(ev_e ev)
{
    evUpdate < uint8_t > (_ms, ev, 0, 59);

    if (_ms >= _min_ms)
        return;

    if (ev == EV_NEG)
    {
        if (_zero && (_ms != 0))
            _ms = 0;
        else
            _ms = 59;
    }
    else if (!_zero || (_ms != 0))
    {
        _ms = _min_ms;
    }
}

void UI::HM::changeMS(void)
{
    if (_hm == 0)
        _min_ms = _min_secs;
    else
        _min_ms = 0;

    if ((_ms < _min_ms) && ((_ms != 0) || !_zero))
        _ms = _min_ms;
}

uint32_t UI::HM::seconds(void) const
{
    uint32_t secs = (_hm * 60);
    if (_hm < 60)
        secs += _ms;
    else
        secs += (_ms * 60);

    return secs;
}

////////////////////////////////////////////////////////////////////////////////
// HM END //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SetAlarm START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UI::SetAlarm::SetAlarm(UI & ui)
    : UISetState(ui)
{
    _ui._rtc.getAlarm(_alarm);
}

void UI::SetAlarm::uisBegin(void)
{
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

UI::sc_e UI::SetAlarm::uisNap(void)
{
    return SC_OFF;
}

UI::sc_e UI::SetAlarm::uisSleep(void)
{
    return SC_OFF;
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

UI::sc_e UI::SetClock::uisNap(void)
{
    return SC_OFF;
}

UI::sc_e UI::SetClock::uisSleep(void)
{
    return SC_OFF;
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
// SetPower START //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
constexpr char const * const UI::SetPower::_s_opts[SOPT_CNT];

UI::SetPower::SetPower(UI & ui)
    : UISetState(ui)
{
    if (!_ui._eeprom.getPower(_power))
        _power.nap_secs = _power.stop_secs = _power.sleep_secs = _power.touch_secs = 0;

    _nap.init(_power.nap_secs);
    _stop.init(_power.stop_secs);
    _sleep.init(_power.sleep_secs);
    _touch_secs = _power.touch_secs;
}

void UI::SetPower::uisBegin(void)
{
    _state = SPS_OPT;
    _blink.reset(_s_set_blink_time);
    _updated = _done = false;
    display();
}

void UI::SetPower::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::SetPower::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    _updated = true;
    MFC(_update_actions[_state])(ev);
    display();
}

void UI::SetPower::uisChange(void)
{
    if (_state == SPS_OPT)
    {
        switch (_sopt)
        {
            case SOPT_NAP: _state = SPS_NAP_HM; break;
            case SOPT_STOP: _state = SPS_STOP_HM; break;
            case SOPT_SLEEP: _state = SPS_SLEEP_HM; break;
            case SOPT_TOUCH: _state = SPS_TOUCH; break;
            default: break;
        }
    }
    else
    {
        _state = _next_states[_state];
    }

    if (_change_actions[_state] != nullptr)
        MFC(_change_actions[_state])();

    if (_state == SPS_DONE)
        _blink.reset(_s_done_blink_time);
    else
        _blink.reset(_s_set_blink_time);

    display();
}

void UI::SetPower::uisReset(ps_e ps)
{
    uisBegin();
}

void UI::SetPower::uisEnd(void)
{
    if (_updated) commit();
    _ui._display.blank();
}

UI::sc_e UI::SetPower::uisNap(void)
{
    return SC_OFF;
}

UI::sc_e UI::SetPower::uisSleep(void)
{
    return SC_OFF;
}

void UI::SetPower::uisRefresh(void)
{
    _blink.reset();
    display();
}

void UI::SetPower::commit(void)
{
    _power.nap_secs = _nap.seconds();
    _power.stop_secs = _stop.seconds();
    _power.sleep_secs = _sleep.seconds();
    _power.touch_secs = _touch_secs;

    (void)_ui._eeprom.setPower(_power);

    _ui._power.update(_power);
}

void UI::SetPower::waitOpt(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetPower::waitHM(bool on)
{
    display(on ? DF_NONE : DF_BL_BC);
}

void UI::SetPower::waitMS(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetPower::waitTouch(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetPower::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetPower::updateOpt(ev_e ev)
{
    evUpdate < sopt_e > (_sopt, ev, SOPT_NAP, (sopt_e)(SOPT_CNT - 1));
}

void UI::SetPower::updateHM(ev_e ev)
{
    _hm->updateHM(ev);
}

void UI::SetPower::updateMS(ev_e ev)
{
    _hm->updateMS(ev);
}

void UI::SetPower::updateTouch(ev_e ev)
{
    evUpdate < uint8_t > (_touch_secs, ev, 0, _s_touch_max);

    if ((_touch_secs > 0) && (_touch_secs < _s_touch_min))
    {
        if (ev == EV_NEG)
            _touch_secs = 0;
        else
            _touch_secs = _s_touch_min;
    }
}

void UI::SetPower::changeNap(void)
{
    _hm = &_nap;
}

void UI::SetPower::changeStop(void)
{
    _hm = &_stop;
}

void UI::SetPower::changeSleep(void)
{
    _hm = &_sleep;
}

void UI::SetPower::changeMS(void)
{
    _hm->changeMS();
}

void UI::SetPower::changeDone(void)
{
    commit();
    _done = true;
    _updated = false;
}

void UI::SetPower::display(df_t flags)
{
    MFC(_display_actions[_state])(flags);
}

void UI::SetPower::displayOpt(df_t flags)
{
    _ui._display.showString(_s_opts[_sopt], flags);
}

void UI::SetPower::displayTimer(df_t flags)
{
    uint32_t seconds = _hm->seconds();
    if (_hm->hm() >= 60)
        flags |= DF_HM;

    _ui._display.showTimer(seconds, (_hm->hm() == 0) ? (flags | DF_BL0) : (flags | DF_NO_LZ));
}

void UI::SetPower::displayTouch(df_t flags)
{
    _ui._display.showInteger(_touch_secs, flags | DF_NO_LZ);
}

void UI::SetPower::displayDone(df_t flags)
{
    static constexpr uint8_t const nopts = 2;
    static uint32_t i = 0;

    if (_done) { i = 0; _done = false; }
    else if (flags == DF_NONE) i++;
    else return;

    if ((i % nopts) == 0)
        displayOpt();
    else if (_sopt == SOPT_TOUCH)
        displayTouch();
    else
        displayTimer();
}
////////////////////////////////////////////////////////////////////////////////
// SetPower END ////////////////////////////////////////////////////////////////
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
        else if (_ui._controls.touchingTime() >= _s_touch_time)
        {
            _state = CS_TRACK;
            changeTrack();
            display();
        }
    }
    else if (_ui._controls.touching() && !_track_updated)
    {
        _alt_display.reset(_s_flash_time);
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

UI::sc_e UI::Clock::uisNap(void)
{
    return SC_DIM;
}

UI::sc_e UI::Clock::uisSleep(void)
{
    return SC_OFF;
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
    if ((_hm.hm() == 0) && (_hm.ms() == 0))
        _state = TS_SET_HM;
    else
        _state = TS_WAIT;

    _show_clock = false;

    _display_timer = Pit::acquire(timerDisplay, _s_seconds_interval);
    _led_timer = Pit::acquire(timerLeds, _s_seconds_interval);  // Will get updated

    if ((_display_timer == nullptr) || (_led_timer == nullptr))
    {
        _ui._errno = ERR_TIMER;
        return;
    }

    _blink.reset();
    _show_timer.disable();
    displayTimer();
}

void UI::Timer::uisWait(void)
{
    if (_wait_actions[_state] != nullptr)
        MFC(_wait_actions[_state])();

    bool st = _show_timer.toggled();

    if (_show_clock && _show_timer.off() && !_ui.displaying())
        clockUpdate(st);

    if (st) _show_timer.disable();
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
        displayTimer();

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
        else
            timerUpdate(true);

        _show_timer.disable();
    }

    uisWait();
}

void UI::Timer::uisChange(void)
{
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

UI::sc_e UI::Timer::uisNap(void)
{
    return ((_state == TS_RUNNING) || (_state == TS_ALERT)) ? SC_DIM : SC_OFF;
}

UI::sc_e UI::Timer::uisSleep(void)
{
    return uisNap();
}

void UI::Timer::uisRefresh(void)
{
    if (_state <= TS_WAIT)
        displayTimer();
    else if (_show_clock)
        clockUpdate(true);
    else
        timerUpdate(true);
}

void UI::Timer::waitHM(void)
{
    if (!_blink.toggled())
        return;

    displayTimer(_blink.on() ? DF_NONE : DF_BL_BC);
}

void UI::Timer::waitMS(void)
{
    if (!_blink.toggled())
        return;

    displayTimer(_blink.on() ? DF_NONE : DF_BL_AC);
}

void UI::Timer::updateHM(ev_e ev)
{
    _hm.updateHM(ev);
}

void UI::Timer::updateMS(ev_e ev)
{
    _hm.updateMS(ev);
}

void UI::Timer::displayTimer(df_t flags)
{
    uint32_t seconds = _hm.seconds();
    if (_hm.hm() >= 60)
        flags |= DF_HM;

    _ui._display.showTimer(seconds, (_hm.hm() == 0) ? (flags | DF_BL0) : (flags | DF_NO_LZ));
}

void UI::Timer::changeHM(void)
{
    _hm.changeMS();
    _blink.reset();
    displayTimer();
}

void UI::Timer::start(void)
{
    df_t flags = DF_NO_LZ;

    _s_seconds = _hm.seconds();
    if (_hm.hm() >= 60)
        flags |= DF_HM;

    _ui._display.showTimer(_s_seconds, flags);

    uint32_t minutes = _s_seconds / 60;
    if (minutes > (10 * 60))
        _s_hue_interval = 64;
    else if (minutes > 60)
        _s_hue_interval = 8;
    else
        _s_hue_interval = 1;

    _led_timer->updateInterval((_s_seconds * _s_us_per_hue) / _s_hue_interval, false);

    _last_second = _s_seconds;
    _hm_on = true;

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
    _show_timer.disable();
    displayTimer();
}

void UI::Timer::run(void)
{
    if (!_show_clock || _show_timer.on() || (_s_seconds == 0))
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
    uint32_t seconds = _s_seconds;

    if (!force && (seconds == _last_second))
        return;

    _last_second = seconds;

    // If an hour or more left, blink the decimal to indicate progress
    if ((seconds / 60) >= 60)
        _hm_on = !_hm_on;
    else
        _hm_on = false;

    if (!_ui.displaying())
        _ui._display.showTimer(seconds, _hm_on ? (DF_HM | DF_NO_LZ) : DF_NO_LZ);
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

    _show_timer.reset();
    _ui._display.showTimer(_s_seconds, (_s_seconds >= 3600) ? (DF_HM | DF_NO_LZ) : DF_NO_LZ);
}

void UI::Timer::resume(void)
{
    _show_timer.reset();
    _ui._display.showTimer(_s_seconds, _hm_on ? (DF_HM | DF_NO_LZ) : DF_NO_LZ);

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
    if (_ui._controls.touching())
    {
        uisChange();
        return;
    }

    if (!_ui._beeper.toggled())
        return;

    if (_ui._beeper.on())
    {
        if (!_ui.displaying())
            _ui._display.showDone();

        _ui._lighting.on();
    }
    else
    {
        if (!_ui.displaying())
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
    _ui._controls.touchDisable();

    _tsi.getConfig(_touch);
    _state = (_touch.threshold == 0) ? TS_DISABLED : TS_OPT;
    _topt = TOPT_CAL;
    _ui._lighting.state(Lighting::LS_OTHER);
    reset();
}

void UI::SetTouch::uisWait(void)
{
    if (_wait_actions[_state] == nullptr)
        return;

    if ((_state == TS_CAL_UNTOUCHED) || (_state == TS_CAL_TOUCHED))
        MFC(_wait_actions[_state])(_blink.toggled());
    else if (_state == TS_READ)
        MFC(_wait_actions[_state])(_read_toggle.toggled());
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
    _ui._controls.touchEnable();
}

UI::sc_e UI::SetTouch::uisNap(void)
{
    return ((_state == TS_CAL_UNTOUCHED) || (_state == TS_CAL_TOUCHED)
            || (_state == TS_READ) || (_state == TS_TEST)) ? SC_DIM : SC_OFF;
}

UI::sc_e UI::SetTouch::uisSleep(void)
{
    return uisNap();
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
        _tsi.defaults(_touch);
    else
        _touch.threshold = 0;
    _state = (_touch.threshold == 0) ? TS_DISABLED : TS_OPT;
    _topt = TOPT_CAL;
    (void)_tsi.setConfig(_touch);
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

void UI::SetTouch::waitCalThreshold(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetTouch::waitCalTouched(bool on)
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

void UI::SetTouch::waitRead(bool on)
{
    if (!on) return;

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

void UI::SetTouch::waitTest(bool on)
{
    static uint32_t beep_mark = 0;

    uint16_t read = _tsi_channel.read();
    if (read == 0)
        return;

    if ((msecs() - beep_mark) < _s_beeper_wait)
        return;

    beep_mark = msecs();

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
        (void)_tsi.configure(_touch.nscn, _touch.ps, _touch.refchrg, _touch.extchrg);

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
    (void)_tsi.setConfig(_touch);
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
// SetNLC START ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
constexpr CRGB::Color const UI::SetNLC::_s_colors[];

UI::SetNLC::SetNLC(UI & ui)
    : UISetState(ui)
{
    for (uint8_t i = 0; i < EE_NLC_CNT; i++)
    {
        uint32_t color_code;
        if (_ui._eeprom.getNLC(i, color_code))
            _nl_colors[i] = color_code;
    }
}

void UI::SetNLC::uisBegin(void)
{
    _state = SLS_INDEX;
    _blink.reset();
    _updated = _done = false;
    _ui._lighting.state(Lighting::LS_SET);
    _color = _nl_colors[_index];
    _ui._lighting.setColor(_color);
    display();
}

void UI::SetNLC::uisWait(void)
{
    if ((_wait_actions[_state] != nullptr) && _blink.toggled())
        MFC(_wait_actions[_state])(_blink.on());
}

void UI::SetNLC::uisUpdate(ev_e ev)
{
    if (_update_actions[_state] == nullptr)
        return;

    MFC(_update_actions[_state])(ev);

    if (_state != SLS_TYPE)
    {
        if (_state >= SLS_COLOR)
            _updated = true;
        _ui._lighting.setColor(_color);
    }

    display();
}

void UI::SetNLC::uisChange(void)
{
    MFC(_change_actions[_state])();

    if (_state == SLS_DONE)
    {
        commit();
        _done = true;
    }

    _blink.reset();
    display();
}

void UI::SetNLC::uisReset(ps_e ps)
{
    _state = SLS_INDEX;
    _type = SLT_COLOR;
    _sub_state = 0;
    _color = _nl_colors[_index];
    _ui._lighting.setColor(_color);
    display();
}

void UI::SetNLC::uisEnd(void)
{
    if (_updated) commit();

    _ui._lighting.state(Lighting::LS_NIGHT_LIGHT);
    if (_ui._lighting.isOn())
        _ui._lighting.onNL();
    _ui._display.blank();
}

void UI::SetNLC::commit(void)
{
    if (_type == SLT_COLOR)
        _nl_colors[_index] = ((uint32_t)_cindex << 24) | _color.colorCode();
    else
        _nl_colors[_index] = (_nl_colors[_index] & 0xFF000000) | _color.colorCode();

    (void)_ui._eeprom.setNLC(_index, _nl_colors[_index]);
    _ui._lighting.setNL(_index, _color);
}

UI::sc_e UI::SetNLC::uisNap(void)
{
    return SC_OFF;
}

UI::sc_e UI::SetNLC::uisSleep(void)
{
    return SC_OFF;
}

void UI::SetNLC::uisRefresh(void)
{
    _blink.reset();
    display();
}

void UI::SetNLC::waitIndex(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetNLC::waitType(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetNLC::waitColor(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetNLC::waitRGB(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetNLC::waitHSV(bool on)
{
    display(on ? DF_NONE : DF_BL_AC);
}

void UI::SetNLC::waitDone(bool on)
{
    display(on ? DF_NONE : DF_BL);
}

void UI::SetNLC::updateIndex(ev_e ev)
{
    evUpdate < uint8_t > (_index, ev, 0, EE_NLC_CNT - 1);
    _color = _nl_colors[_index];
}

void UI::SetNLC::updateType(ev_e ev)
{
    evUpdate < slt_e > (_type, ev, SLT_COLOR, SLT_HSV);
}

void UI::SetNLC::updateColor(ev_e ev)
{
    evUpdate < uint8_t > (_cindex, ev, 0, (sizeof(_s_colors) / sizeof(CRGB::Color)) - 1);
    _color = _s_colors[_cindex];
}

void UI::SetNLC::updateRGB(ev_e ev)
{
    evUpdate < uint8_t > (*_val, ev, 0, 255);
    _color = _rgb;
}

void UI::SetNLC::updateHSV(ev_e ev)
{
    evUpdate < uint8_t > (*_val, ev, 0, 255);
    _color = _hsv;
}

void UI::SetNLC::changeIndex(void)
{
    _state = SLS_TYPE;
}

void UI::SetNLC::changeType(void)
{
    _state = _next_type_states[_type];
    _sub_state = 0;

    if (_state == SLS_COLOR)
    {
        _cindex = _nl_colors[_index] >> 24;
        if (_cindex >= (sizeof(_s_colors) / sizeof(CRGB::Color)))
            _cindex = 0;
        _color = _s_colors[_cindex];
    }
    else if (_state == SLS_RGB)
    {
        _opt = _rgb_opts[_sub_state];
        _rgb = _color;
        _val = &_rgb[_sub_state];
    }
    else  // _state == SLS_HSV
    {
        _opt = _hsv_opts[_sub_state];
        _hsv = _color;
        _val = &_hsv[_sub_state];
    }

    _ui._lighting.setColor(_color);
}

void UI::SetNLC::changeColor(void)
{
    _state = SLS_DONE;
}

void UI::SetNLC::changeRGB(void)
{
    if (_ui._controls.doublePressed(SWI_ENC))
    {
        _state = SLS_DONE;
        return;
    }

    _sub_state = (_sub_state == 2) ? 0 : _sub_state + 1;

    _opt = _rgb_opts[_sub_state];
    _val = &_rgb[_sub_state];
}

void UI::SetNLC::changeHSV(void)
{
    if (_ui._controls.doublePressed(SWI_ENC))
    {
        _state = SLS_DONE;
        return;
    }

    _sub_state = (_sub_state == 2) ? 0 : _sub_state + 1;

    _opt = _hsv_opts[_sub_state];
    _val = &_hsv[_sub_state];
}

void UI::SetNLC::changeDone(void)
{
    _state = SLS_INDEX;
}

void UI::SetNLC::display(df_t flags)
{
    if (_display_actions[_state] != nullptr)
        MFC(_display_actions[_state])(flags);
}

void UI::SetNLC::displayIndex(df_t flags)
{
    _ui._display.showOption("nL", (uint8_t)(_index + 1), flags);
}

void UI::SetNLC::displayType(df_t flags)
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

void UI::SetNLC::displayColor(df_t flags)
{
    _ui._display.showInteger(_cindex + 1, flags);
}

void UI::SetNLC::displayRGB(df_t flags)
{
    _ui._display.showOption(_opt, *_val, flags | DF_HEX | DF_LZ);
}

void UI::SetNLC::displayHSV(df_t flags)
{
    _ui._display.showOption(_opt, *_val, flags | DF_HEX | DF_LZ);
}

void UI::SetNLC::displayDone(df_t flags)
{
    static uint8_t nopts = 0;
    static uint32_t i = 0;

    if (_done) { i = 0; _done = false; if (_type == SLT_COLOR) nopts = 3; else nopts = 5; }
    else if (flags == DF_NONE) i++;
    else return;

    if ((i % nopts) == 0)
    {
        displayIndex();
    }
    else if ((i % nopts) == 1)
    {
        displayType();
    }
    else if (_type == SLT_COLOR)
    {
        displayColor();
    }
    else if ((_type == SLT_RGB) || (_type == SLT_HSV))
    {
        uint8_t oi = (i % nopts) - 2;

        if (_type == SLT_RGB)
        {
            _opt = _rgb_opts[oi];
            _val = &_rgb[oi];
            displayRGB();
        }
        else
        {
            _opt = _hsv_opts[oi];
            _val = &_hsv[oi];
            displayHSV();
        }
    }
}
////////////////////////////////////////////////////////////////////////////////
// SetNLC END /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
