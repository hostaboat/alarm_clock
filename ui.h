#ifndef _UI_H_
#define _UI_H_

#include "audio.h"
#include "display.h"
#include "eeprom.h"
#include "elecmech.h"
#include "file.h"
#include "llwu.h"
#include "opti.h"
#include "pit.h"
#include "rtc.h"
#include "tsi.h"
#include "types.h"
#include "utility.h"

template < class DSW >
class MomSwitch  // Momentary Switch
{
    public:
        MomSwitch(DSW & swi) : _swi(swi) {}

        void process(void) { _cs = _swi.closeStart(); _cd = _swi.closeDuration(); }
        void postProcess(void);

        bool closed(void) const { return _swi.closed() || (cs() != 0); }
        bool pressed(void) const { return cd() != 0; }
        bool doublePressed(void) const { return _double_pressed; }
        uint32_t pressTime(void) const { return _press_time; }

        bool valid(void) const { return _swi.valid(); }
        void reset(void) { _cs = _cd = _cs_tmp = 0; }

    private:
        uint32_t cs(void) const { if (_cs_tmp != 0) return _cs_tmp; return _cs; }
        uint32_t cd(void) const { return _cd; }
        static constexpr uint32_t const _s_double_press = 300;

        DSW & _swi;
        uint32_t _cs = 0;
        uint32_t _cd = 0;
        uint32_t _cs_tmp = 0;
        uint32_t _last_press = 0;
        uint32_t _press_time = 0;
        bool _double_pressed = false;
};

template < class DSW >
void MomSwitch < DSW >::postProcess(void)
{
    if (_swi.closed())
    {
        if ((_cs == 0) && (_cs_tmp == 0))
            _cs_tmp = msecs();
    }
    else if (_cs_tmp != 0)
    {
        _cd = msecs() - _cs_tmp;
        _cs_tmp = 0;
    }

    _double_pressed = false;

    if (cs() != 0)
    {
        _press_time = msecs() - cs();
    }
    else if (cd() != 0)
    {
        _press_time = cd();

        if ((msecs() - _last_press) < _s_double_press)
            _double_pressed = true;

        _last_press = msecs();
    }
    else
    {
        _press_time = 0;
    }
}

// Rotary Switch Positions
enum rsp_e : uint8_t
{
    RSP_NONE,
    RSP_LEFT,
    RSP_MIDDLE,
    RSP_RIGHT,
    RSP_CNT
};

// Controls
enum con_e : uint8_t
{
    CON_RS,
    CON_ENC_SWI,
    CON_ENC_BRI,
    CON_SWI_PLAY,
    CON_SWI_NEXT,
    CON_SWI_PREV,
};

// Encoders
enum enc_e : uint8_t
{
    ENC_SWI,
    ENC_BRI,
};

// Switches
enum swi_e : uint8_t
{
    SWI_ENC,
    SWI_PLAY,
    SWI_NEXT,
    SWI_PREV,
};

class Controls
{
    public:
        Controls(void);

        void process(void);
        bool interaction(bool inc_touch = true) const;

        // Rotary Switch
        rsp_e pos(void) const { return _pos; }
        bool posChange(void) const { return _pos_change; }

        // Rotary Encoders
        ev_e turn(enc_e enc) const;

        // Momentary Switches
        bool closed(swi_e swi) const;
        bool pressed(swi_e swi) const;
        bool doublePressed(swi_e swi) const;
        uint32_t pressTime(swi_e swi) const;

        // Touch
        void touchEnable(void);
        void touchDisable(void);
        bool touching(void) const;
        bool touched(void) const;
        uint32_t touchingTime(void) const;
        uint32_t touchedTime(void) const;

        bool valid(con_e con) const;
        bool valid(void) const;

        void reset(void);

    private:
        void setPos(void);
        void setTouch(void);

        TRsw & _rs = TRsw::acquire();
        TBrEnc & _bri_enc = TBrEnc::acquire(IRQC_INTR_CHANGE);
        TUiEncSw & _swi_enc = TUiEncSw::acquire(IRQC_INTR_CHANGE, IRQC_INTR_CHANGE);

        MomSwitch < TUiEncSw > _enc_swi{_swi_enc};
        MomSwitch < TSwPlay > _play{TSwPlay::acquire(IRQC_INTR_CHANGE)};
        MomSwitch < TSwNext > _next{TSwNext::acquire(IRQC_INTR_CHANGE)};
        MomSwitch < TSwPrev > _prev{TSwPrev::acquire(IRQC_INTR_CHANGE)};

        Tsi & _tsi = Tsi::acquire();
        Tsi::Channel & _tsi_channel = _tsi.acquire < PIN_TOUCH > ();

        ev_e _bri_turn = EV_ZERO;
        ev_e _enc_turn = EV_ZERO;

        rsp_e _pos = RSP_NONE;
        rsp_e _pos_last = RSP_NONE;
        bool _pos_change = false;

        bool _touch_enabled = true;
        uint32_t _touch_mark = 0;
        uint32_t _touched_time = 0;
        bool _touched = false;
        static constexpr uint32_t const _s_touch_time = 50;

        bool _interaction = false;
};

class UI
{
    public:
        static UI & acquire(void) { static UI ui; return ui; }

        void process(void);
        bool valid(void);

        UI(UI const &) = delete;
        UI & operator=(UI const &) = delete;

    private:
        UI(void);

        enum err_e
        {
            ERR_NONE,
            ERR_HW_RS,
            ERR_HW_ENC_SWI,
            ERR_HW_BR_ENC,
            ERR_HW_PLAY,
            ERR_HW_NEXT,
            ERR_HW_PREV,
            ERR_HW_BEEPER,
            ERR_HW_DISPLAY,
            ERR_HW_LIGHTING,
            ERR_HW_TOUCH,
            ERR_HW_DISK,
            ERR_HW_AUDIO,
            ERR_PLAYER,
            ERR_PLAYER_GET_FILES,
            ERR_PLAYER_OPEN_FILE,
            ERR_PLAYER_AUDIO,
            ERR_TIMER,
            ERR_CNT  // 18
        };

        char const * const _err_strs[ERR_CNT] =
        {
            "nonE"     // ERR_NONE
            "C.SEL.",  // ERR_HW_RS
            "C.SEt.",  // ERR_HW_ENC_SWI
            "C.br.",   // ERR_HW_BR_ENC
            "C.PLA.",  // ERR_HW_PLAY
            "C.nE.",   // ERR_HW_NEXT
            "C.PrE.",  // ERR_HW_PREV
            "BEEP",    // ERR_HW_BEEPER
            "dISP.",   // ERR_HW_DISPLAY
            "LEdS",    // ERR_HW_LIGHTING
            "tUCH",    // ERR_HW_TOUCH
            "FILE",    // ERR_HW_DISK
            "Aud.",    // ERR_HW_AUDIO
            "PLAY",    // ERR_PLAYER
            "P.FIL.",  // ERR_PLAYER_GET_FILES
            "P.OPE.",  // ERR_PLAYER_OPEN_FILE
            "P.Aud.",  // ERR_PLAYER_AUDIO
            "T.PIt.",  // ERR_TIMER
        };

        // Encoder-Switch states
        enum es_e : uint8_t
        {
            ES_NONE,
            ES_TURNED,
            ES_DEPRESSED,
            ES_PRESSED,
        };

        // Switch press states
        enum ps_e : uint8_t
        {
            PS_NONE,
            PS_CHANGE,
            PS_RESET,
            PS_STATE,
            PS_ALT_STATE,
            PS_CNT
        };

        // UI states
        enum uis_e : uint8_t
        {
            UIS_SET_ALARM,
            UIS_SET_CLOCK,
            UIS_SET_TOUCH,
            UIS_CLOCK,
            UIS_TIMER,
            UIS_SET_POWER,
            UIS_SET_LEDS,
            UIS_CNT
        };

        uis_e const _next_states[UIS_CNT][PS_CNT][RSP_CNT] =
        {
            //      RSP_NONE       RSP_LEFT     RSP_MIDDLE    RSP_RIGHT
            //
            //                          UIS_SET_ALARM
            {
                { UIS_SET_ALARM, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER },  // PS_NONE
                { UIS_SET_ALARM, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER },  // PS_CHANGE
                { UIS_SET_ALARM, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER },  // PS_RESET
                { UIS_SET_ALARM, UIS_SET_CLOCK, UIS_CLOCK,     UIS_TIMER },  // PS_STATE
                { UIS_SET_ALARM, UIS_SET_TOUCH, UIS_CLOCK,     UIS_TIMER },  // PS_ALT_STATE
            },
            //                          UIS_SET_CLOCK
            {
                { UIS_SET_CLOCK, UIS_SET_CLOCK, UIS_CLOCK,     UIS_TIMER },  // PS_NONE
                { UIS_SET_CLOCK, UIS_SET_CLOCK, UIS_CLOCK,     UIS_TIMER },  // PS_CHANGE
                { UIS_SET_CLOCK, UIS_SET_CLOCK, UIS_CLOCK,     UIS_TIMER },  // PS_RESET
                { UIS_SET_CLOCK, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER },  // PS_STATE
                { UIS_SET_CLOCK, UIS_SET_TOUCH, UIS_CLOCK,     UIS_TIMER },  // PS_ALT_STATE
            },
            //                           UIS_SET_TOUCH
            {
                { UIS_SET_TOUCH, UIS_SET_TOUCH, UIS_CLOCK,     UIS_TIMER }, // PS_NONE
                { UIS_SET_TOUCH, UIS_SET_TOUCH, UIS_CLOCK,     UIS_TIMER }, // PS_CHANGE
                { UIS_SET_TOUCH, UIS_SET_TOUCH, UIS_CLOCK,     UIS_TIMER }, // PS_RESET
                { UIS_SET_TOUCH, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_STATE
                { UIS_SET_TOUCH, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_ALT_STATE
            },
            //                            UIS_CLOCK
            {
                {     UIS_CLOCK, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_NONE
                {     UIS_CLOCK, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_CHANGE
                {     UIS_CLOCK, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_RESET
                {     UIS_CLOCK, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_STATE
                {     UIS_CLOCK, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_ALT_STATE
            },
            //                            UIS_TIMER
            {
                {     UIS_TIMER, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_NONE
                {     UIS_TIMER, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_CHANGE
                {     UIS_TIMER, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_RESET
                {     UIS_TIMER, UIS_SET_ALARM, UIS_CLOCK, UIS_SET_POWER }, // PS_STATE
                {     UIS_TIMER, UIS_SET_ALARM, UIS_CLOCK,  UIS_SET_LEDS }, // PS_ALT_STATE
            },
            //                           UIS_SET_POWER
            {
                { UIS_SET_POWER, UIS_SET_ALARM, UIS_CLOCK, UIS_SET_POWER }, // PS_NONE
                { UIS_SET_POWER, UIS_SET_ALARM, UIS_CLOCK, UIS_SET_POWER }, // PS_CHANGE
                { UIS_SET_POWER, UIS_SET_ALARM, UIS_CLOCK, UIS_SET_POWER }, // PS_RESET
                { UIS_SET_POWER, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_STATE
                { UIS_SET_POWER, UIS_SET_ALARM, UIS_CLOCK,  UIS_SET_LEDS }, // PS_ALT_STATE
            },
            //                           UIS_SET_LEDS
            {
                {  UIS_SET_LEDS, UIS_SET_ALARM, UIS_CLOCK,  UIS_SET_LEDS }, // PS_NONE
                {  UIS_SET_LEDS, UIS_SET_ALARM, UIS_CLOCK,  UIS_SET_LEDS }, // PS_CHANGE
                {  UIS_SET_LEDS, UIS_SET_ALARM, UIS_CLOCK,  UIS_SET_LEDS }, // PS_RESET
                {  UIS_SET_LEDS, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_STATE
                {  UIS_SET_LEDS, UIS_SET_ALARM, UIS_CLOCK,     UIS_TIMER }, // PS_ALT_STATE
            },
        };

        uis_e _state;
        err_e _errno = ERR_NONE;

        ps_e pressState(uint32_t msecs);
        bool pressing(es_e encoder_state);
        void updateBrightness(ev_e ev);
        void error(void);

        static constexpr uint32_t const _s_reset_blink_time = 300;

        static constexpr uint32_t const _s_press_change = 1500;
        static constexpr uint32_t const _s_press_reset  = 4000;
        static constexpr uint32_t const _s_press_state = 6500;
        static constexpr uint32_t const _s_press_alt_state = 8000;

        TDisplay & _display = TDisplay::acquire();
        TBeep & _beeper = TBeep::acquire();
        Rtc & _rtc = Rtc::acquire();
        Eeprom & _eeprom = Eeprom::acquire();

        // Want to have display and night light brightness synced.
        // The display levels are about 1/16 of the night light levels
        // off by one - 17 display levels and 256 night light levels.
        // The value 120 is midway between 112 and 128. See updateBrightness().
        static constexpr uint8_t const _s_brightness = 120;
        uint8_t _brightness = _s_brightness;
        uint8_t _display_brightness = BR_MID;
        static constexpr uint8_t const _s_dim_leds = 14;
        static constexpr br_e const _s_dim_display = BR_LOW;

        uint32_t _display_mark = 0;
        static constexpr uint32_t const _s_display_wait = 1000;
        bool displaying(void) const { return _display_mark != 0; }
        void display(char const * str);
        void display(int32_t n);
        bool refresh(void);

        enum pwr_e : uint8_t { PWR_NAP, PWR_SLEEP, PWR_TOUCH_SLEEP };

        void dimScreens(void);
        void sleepScreens(void);
        void wakeScreens(int8_t wakeup_pin);
        bool _wait_released = false;
        bool _skip_processing = false;

        // Make touch activation times consistent and make sure touch induced
        // sleep is greater than the touch activation time.
        // Times are in seconds.
        static constexpr uint32_t const _s_touch_activate = 1;
        static constexpr uint32_t const _s_touch_sleep = _s_touch_activate + 1;

        class UIState
        {
            public:
                UIState(UI & ui) : _ui{ui} {}

                virtual void uisBegin(void) = 0;
                virtual void uisWait(void) = 0;
                virtual void uisUpdate(ev_e ev) = 0;
                virtual void uisChange(void) = 0;
                virtual void uisReset(ps_e ps) = 0;
                virtual void uisEnd(void) = 0;
                virtual void uisRefresh(void) = 0;
                virtual bool uisPower(pwr_e pwr) = 0;

            protected:
                UI & _ui;
        };

        class UISetState : public UIState
        {
            public:
                UISetState(UI & ui) : UIState(ui) {}

                virtual void uisBegin(void) = 0;
                virtual void uisWait(void) = 0;
                virtual void uisUpdate(ev_e ev) = 0;
                virtual void uisChange(void) = 0;
                virtual void uisReset(ps_e ps) = 0;
                virtual void uisEnd(void) = 0;
                virtual void uisRefresh(void) = 0;
                virtual bool uisPower(pwr_e pwr) = 0;

            protected:
                static constexpr uint32_t const _s_set_blink_time = 500;   // milliseconds
                static constexpr uint32_t const _s_done_blink_time = 800;
                Toggle _blink{_s_set_blink_time};
                bool _updated = false;
                bool _done = false;
        };

        class UIRunState : public UIState
        {
            public:
                UIRunState(UI & ui) : UIState(ui) {}

                virtual void uisBegin(void) = 0;
                virtual void uisWait(void) = 0;
                virtual void uisUpdate(ev_e ev) = 0;
                virtual void uisChange(void) = 0;
                virtual void uisReset(ps_e ps) = 0;
                virtual void uisEnd(void) = 0;
                virtual void uisRefresh(void) = 0;
                virtual bool uisPower(pwr_e pwr) = 0;
        };


        // Helper for Timer settings
        struct HM
        {
            public:
                HM(uint8_t min_secs = 0, bool zero = true)
                    : _min_secs(min_secs), _zero(zero) {}

                void init(uint32_t secs);
                void updateHM(ev_e ev);
                void updateMS(ev_e ev);
                void changeMS(void);
                uint32_t seconds(void) const;
                uint16_t hm(void) const { return _hm; }
                uint16_t ms(void) const { return _ms; }

            private:
                uint16_t _hm;
                uint8_t _ms;
                uint8_t _min_ms;

                uint8_t const _min_secs;
                bool const _zero;
                static constexpr uint16_t const _s_max_hm = 99 * 60;
        };

        class SetAlarm : public UISetState
        {
            public:
                SetAlarm(UI & ui);

                virtual void uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(ps_e ps);
                virtual void uisEnd(void);
                virtual void uisRefresh(void);
                virtual bool uisPower(pwr_e pwr);

            private:
                void waitHour(bool on);
                void waitMinute(bool on);
                void waitType(bool on);
                void waitSnooze(bool on);
                void waitWake(bool on);
                void waitTime(bool on);
                void waitDone(bool on);

                void updateHour(ev_e ev);
                void updateMinute(ev_e ev);
                void updateType(ev_e ev);
                void updateSnooze(ev_e ev);
                void updateWake(ev_e ev);
                void updateTime(ev_e ev);

                void display(df_t flags = DF_NONE);
                void displayClock(df_t flags = DF_NONE);
                void displayType(df_t flags = DF_NONE);
                void displaySnooze(df_t flags = DF_NONE);
                void displayWake(df_t flags = DF_NONE);
                void displayTime(df_t flags = DF_NONE);
                void displayDisabled(df_t flags = DF_NONE);
                void displayDone(df_t flags = DF_NONE);

                void toggleEnabled(void);

                enum sas_e : uint8_t
                {
                    SAS_HOUR,
                    SAS_MINUTE,
                    SAS_TYPE,
                    SAS_SNOOZE,
                    SAS_WAKE,
                    SAS_TIME,
                    SAS_DISABLED,
                    SAS_DONE,
                    SAS_CNT
                };

                sas_e const _next_states[SAS_CNT] =
                {
                    SAS_MINUTE,
                    SAS_TYPE,
                    SAS_SNOOZE,
                    SAS_WAKE,
                    SAS_TIME,
                    SAS_DONE,
                    SAS_HOUR,
                    SAS_HOUR
                };

                using sas_wait_action_t = void (SetAlarm::*)(bool on);
                sas_wait_action_t const _wait_actions[SAS_CNT] =
                {
                    &SetAlarm::waitHour,
                    &SetAlarm::waitMinute,
                    &SetAlarm::waitType,
                    &SetAlarm::waitSnooze,
                    &SetAlarm::waitWake,
                    &SetAlarm::waitTime,
                    nullptr,
                    &SetAlarm::waitDone
                };

                using sas_update_action_t = void (SetAlarm::*)(ev_e ev);
                sas_update_action_t const _update_actions[SAS_CNT] =
                {
                    &SetAlarm::updateHour,
                    &SetAlarm::updateMinute,
                    &SetAlarm::updateType,
                    &SetAlarm::updateSnooze,
                    &SetAlarm::updateWake,
                    &SetAlarm::updateTime,
                    nullptr,
                    nullptr
                };

                using sas_display_action_t = void (SetAlarm::*)(df_t flags);
                sas_display_action_t const _display_actions[SAS_CNT] =
                {
                    &SetAlarm::displayClock,
                    &SetAlarm::displayClock,
                    &SetAlarm::displayType,
                    &SetAlarm::displaySnooze,
                    &SetAlarm::displayWake,
                    &SetAlarm::displayTime,
                    &SetAlarm::displayDisabled,
                    &SetAlarm::displayDone
                };

                sas_e _state = SAS_HOUR;
                tAlarm _alarm = {};
        };

        class SetClock : public UISetState
        {
            public:
                SetClock(UI & ui) : UISetState(ui) {}

                virtual void uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(ps_e ps);
                virtual void uisEnd(void);
                virtual void uisRefresh(void);
                virtual bool uisPower(pwr_e pwr);

            private:
                void waitType(bool on);
                void waitHour(bool on);
                void waitMinute(bool on);
                void waitYear(bool on);
                void waitMonth(bool on);
                void waitDay(bool on);
                void waitDST(bool on);
                void waitDone(bool on);

                void updateType(ev_e ev);
                void updateHour(ev_e ev);
                void updateMinute(ev_e ev);
                void updateYear(ev_e ev);
                void updateMonth(ev_e ev);
                void updateDay(ev_e ev);
                void updateDST(ev_e ev);

                void display(df_t flags = DF_NONE);
                void displayType(df_t flags = DF_NONE);
                void displayTime(df_t flags = DF_NONE);
                void displayYear(df_t flags = DF_NONE);
                void displayDate(df_t flags = DF_NONE);
                void displayDST(df_t flags = DF_NONE);
                void displayDone(df_t flags = DF_NONE);

                enum scs_e : uint8_t
                {
                    SCS_TYPE,
                    SCS_HOUR,
                    SCS_MINUTE,
                    SCS_YEAR,
                    SCS_MONTH,
                    SCS_DAY,
                    SCS_DST,
                    SCS_DONE,
                    SCS_CNT
                };

                scs_e const _next_states[SCS_CNT] =
                {
                    SCS_HOUR,
                    SCS_MINUTE,
                    SCS_YEAR,
                    SCS_MONTH,
                    SCS_DAY,
                    SCS_DST,
                    SCS_DONE,
                    SCS_TYPE
                };

                using scs_wait_action_t = void (SetClock::*)(bool on);
                scs_wait_action_t const _wait_actions[SCS_CNT] =
                {
                    &SetClock::waitType,
                    &SetClock::waitHour,
                    &SetClock::waitMinute,
                    &SetClock::waitYear,
                    &SetClock::waitMonth,
                    &SetClock::waitDay,
                    &SetClock::waitDST,
                    &SetClock::waitDone
                };

                using scs_update_action_t = void (SetClock::*)(ev_e ev);
                scs_update_action_t const _update_actions[SCS_CNT] =
                {
                    &SetClock::updateType,
                    &SetClock::updateHour,
                    &SetClock::updateMinute,
                    &SetClock::updateYear,
                    &SetClock::updateMonth,
                    &SetClock::updateDay,
                    &SetClock::updateDST,
                    nullptr
                };

                using scs_display_action_t = void (SetClock::*)(df_t flags);
                scs_display_action_t const _display_actions[SCS_CNT] =
                {
                    &SetClock::displayType,
                    &SetClock::displayTime,
                    &SetClock::displayTime,
                    &SetClock::displayYear,
                    &SetClock::displayDate,
                    &SetClock::displayDate,
                    &SetClock::displayDST,
                    &SetClock::displayDone
                };

                scs_e _state = SCS_TYPE;
                tClock _clock = {};
        };

        class SetPower : public UISetState
        {
            public:
                SetPower(UI & ui);

                virtual void uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(ps_e ps);
                virtual void uisEnd(void);
                virtual void uisRefresh(void);
                virtual bool uisPower(pwr_e pwr);

            private:
                void waitOpt(bool on);
                void waitHM(bool on);
                void waitMS(bool on);
                void waitTouch(bool on);
                void waitDone(bool on);

                void updateOpt(ev_e ev);
                void updateHM(ev_e ev);
                void updateMS(ev_e ev);
                void updateTouch(ev_e ev);

                void changeNap(void);
                void changeStop(void);
                void changeSleep(void);
                void changeMS(void);
                void changeDone(void);

                void display(df_t flags = DF_NONE);
                void displayOpt(df_t flags = DF_NONE);
                void displayTimer(df_t flags = DF_NONE);
                void displayTouch(df_t flags = DF_NONE);
                void displayDone(df_t flags = DF_NONE);

                void commit(void);

                enum sopt_e : uint8_t { SOPT_NAP, SOPT_STOP, SOPT_SLEEP, SOPT_TOUCH, SOPT_CNT };

                static constexpr char const * const _s_opts[SOPT_CNT] = { "nAP", "STOP", "SLEE.", "tUCH" };

                // Set Power State
                enum sps_e : uint8_t
                {
                    SPS_OPT,
                    SPS_NAP_HM,  // Hours or Minutes
                    SPS_NAP_MS,  // Minutes or Seconds
                    SPS_STOP_HM,
                    SPS_STOP_MS,
                    SPS_SLEEP_HM,
                    SPS_SLEEP_MS,
                    SPS_TOUCH,
                    SPS_DONE,
                    SPS_CNT
                };

                sps_e const _next_states[SPS_CNT] =
                {
                    SPS_NAP_HM,
                    SPS_NAP_MS,
                    SPS_DONE,
                    SPS_STOP_MS,
                    SPS_DONE,
                    SPS_SLEEP_MS,
                    SPS_DONE,
                    SPS_DONE,
                    SPS_OPT,
                };

                using sps_wait_action_t = void (SetPower::*)(bool on);
                sps_wait_action_t const _wait_actions[SPS_CNT] =
                {
                    &SetPower::waitOpt,
                    &SetPower::waitHM,
                    &SetPower::waitMS,
                    &SetPower::waitHM,
                    &SetPower::waitMS,
                    &SetPower::waitHM,
                    &SetPower::waitMS,
                    &SetPower::waitTouch,
                    &SetPower::waitDone
                };

                using sps_update_action_t = void (SetPower::*)(ev_e ev);
                sps_update_action_t const _update_actions[SPS_CNT] =
                {
                    &SetPower::updateOpt,
                    &SetPower::updateHM,
                    &SetPower::updateMS,
                    &SetPower::updateHM,
                    &SetPower::updateMS,
                    &SetPower::updateHM,
                    &SetPower::updateMS,
                    &SetPower::updateTouch,
                    nullptr
                };

                using sps_change_action_t = void (SetPower::*)(void);
                sps_change_action_t const _change_actions[SPS_CNT] =
                {
                    nullptr,
                    &SetPower::changeNap,
                    &SetPower::changeMS,
                    &SetPower::changeStop,
                    &SetPower::changeMS,
                    &SetPower::changeSleep,
                    &SetPower::changeMS,
                    nullptr,
                    &SetPower::changeDone
                };

                using sps_display_action_t = void (SetPower::*)(df_t flags);
                sps_display_action_t const _display_actions[SPS_CNT] =
                {
                    &SetPower::displayOpt,
                    &SetPower::displayTimer,
                    &SetPower::displayTimer,
                    &SetPower::displayTimer,
                    &SetPower::displayTimer,
                    &SetPower::displayTimer,
                    &SetPower::displayTimer,
                    &SetPower::displayTouch,
                    &SetPower::displayDone
                };

                sopt_e _sopt = SOPT_NAP;
                sps_e _state = SPS_OPT;
                HM _nap{_s_min_secs, true};
                HM _stop{_s_min_secs, true};
                HM _sleep{_s_min_secs, true};
                HM * _hm = nullptr;
                ePower _power = {};
                static constexpr uint8_t const _s_min_secs = 10;
                uint8_t _touch_secs = 0;
                static constexpr uint8_t const _s_touch_min = _s_touch_sleep;
                static constexpr uint8_t const _s_touch_max = 60;
        };

        class Clock : public UIRunState
        {
            public:
                Clock(UI & ui) : UIRunState(ui) {}

                virtual void uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(ps_e ps);
                virtual void uisEnd(void);
                virtual void uisRefresh(void);
                virtual bool uisPower(pwr_e pwr);

            private:
                bool clockUpdate(bool force = false);

                void updateTrack(ev_e ev);
                void changeTrack(void);

                void display(void);
                void displayTime(void);
                void displayDate(void);
                void displayYear(void);
                void displayTrack(void);

                enum cs_e : uint8_t
                {
                    CS_TIME,
                    CS_DATE,
                    CS_YEAR,
                    CS_TRACK,
                    CS_CNT
                };

                cs_e const _next_states[CS_CNT] =
                {
                    CS_DATE,
                    CS_YEAR,
                    CS_TRACK,
                    CS_TIME,
                };

                using cs_display_action_t = void (Clock::*)(void);
                cs_display_action_t const _display_actions[CS_CNT] =
                {
                    &Clock::displayTime,
                    &Clock::displayDate,
                    &Clock::displayYear,
                    &Clock::displayTrack,
                };

                cs_e _state = CS_TIME;
                tClock _clock = {};
                df_t _dflag = DF_12H;

                // Show date / year / alarm off / track selection time
                static constexpr uint32_t const _s_flash_time = 3000;
                Toggle _alt_display{_s_flash_time};

                // Sustained touch time to enter track selection state
                static constexpr uint32_t const _s_touch_time = _s_touch_activate * 1000;
                // Time out for inactivity after track number update
                static constexpr uint32_t const _s_track_idle_time = 15000;
                uint16_t _track = 0;
                bool _track_updated = false;
        };

        class Timer : public UIRunState
        {
            public:
                Timer(UI & ui) : UIRunState(ui) {}

                virtual void uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(ps_e ps);
                virtual void uisEnd(void);
                virtual void uisRefresh(void);
                virtual bool uisPower(pwr_e pwr);

                bool running(void) const { return _state == TS_RUNNING; }
                bool alerting(void) const { return _state == TS_ALERT; }
                bool active(void) const { return running() || alerting(); }
                bool clock(void) const { return _show_clock; }

            private:
                void waitHM(void);  // Hour or Minute
                void waitMS(void);  // Minute or Second

                void updateHM(ev_e ev);
                void updateMS(ev_e ev);

                void changeHM(void);

                void displayTimer(df_t flags = DF_NONE);

                void start(void);
                void reset(void);
                void run(void);
                void pause(void);
                void resume(void);
                void alert(void);
                void stop(void);

                void timerUpdate(bool force = false);
                void clockUpdate(bool force = false);
                void hueUpdate(void);

                enum ts_e : uint8_t
                {
                    TS_SET_HM,  // Set Hour or Minute
                    TS_SET_MS,  // Set Minute or Second
                    TS_WAIT,
                    TS_RUNNING,
                    TS_PAUSED,
                    TS_ALERT,
                    TS_CNT
                };

                ts_e const _next_states[TS_CNT] =
                {
                    TS_SET_MS,
                    TS_RUNNING,
                    TS_RUNNING,
                    TS_PAUSED,
                    TS_RUNNING,
                    TS_WAIT
                };

                using ts_wait_action_t = void (Timer::*)(void);
                ts_wait_action_t const _wait_actions[TS_CNT] =
                {
                    &Timer::waitHM,
                    &Timer::waitMS,
                    nullptr,
                    &Timer::run,
                    nullptr,
                    &Timer::alert
                };

                using ts_update_action_t = void (Timer::*)(ev_e ev);
                ts_update_action_t const _update_actions[TS_CNT] =
                {
                    &Timer::updateHM,
                    &Timer::updateMS,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                };

                using ts_change_action_t = void (Timer::*)(void);
                ts_change_action_t const _change_actions[TS_CNT] =
                {
                    &Timer::changeHM,
                    &Timer::start,
                    &Timer::start,
                    &Timer::pause,
                    &Timer::resume,
                    &Timer::reset
                };

                ts_e _state = TS_WAIT;

                HM _hm{1, false};
                bool _hm_on = true;

                uint8_t _last_hue = 0;
                uint32_t _last_second = 0;
                Pit * _display_timer = nullptr;
                Pit * _led_timer = nullptr;

                // For showing the clock while in timer state
                bool _show_clock = false;
                static constexpr uint32_t const _s_turn_wait = 300;
                static constexpr int8_t const _s_turns = 3;

                // Start with blue hue and decrease to red hue
                static constexpr uint8_t const _s_hue_max = 160; // blue
                static constexpr uint8_t const _s_hue_min = 0;   // red
                static constexpr uint32_t _s_seconds_interval = 1000000;
                static constexpr uint32_t _s_us_per_hue = _s_seconds_interval / _s_hue_max;

                // For setting timer
                static constexpr uint32_t const _s_blink = 500;
                Toggle _blink{_s_blink};

                // For showing timer if currently showing the clock and pausing/resuming
                static constexpr uint32_t const _s_show_timer = 3000;
                Toggle _show_timer{_s_show_timer};

                // For lighting
                static constexpr uint32_t const _s_touch_time = _s_touch_activate * 1000;
                bool _show_lighting = true;
                bool _touching = false;

                // ISR
                static void timerDisplay(void);
                static void timerLeds(void);

                static uint32_t volatile _s_seconds;
                static uint8_t volatile _s_hue;
                static uint32_t volatile _s_hue_calls;
                static uint8_t volatile _s_hue_interval;
        };

        class SetTouch : public UISetState
        {
            public:
                SetTouch(UI & ui) : UISetState(ui) {}

                virtual void uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(ps_e ps);
                virtual void uisEnd(void);
                virtual void uisRefresh(void);
                virtual bool uisPower(pwr_e pwr);

                bool calibrating(void) const { return (_state >= TS_CAL_START) && (_state <= TS_CAL_TOUCHED); }
                bool testing(void) const { return _state == TS_TEST; }

            private:
                struct TouchCal
                {
                    uint16_t lo, hi;
                    TouchCal(void) { reset(); }
                    void reset(void) { lo = UINT16_MAX; hi = 0; }
                    uint16_t avg(void) { return (lo + hi) / 2; }
                };

                void waitOpt(bool on);
                void waitNscn(bool on);
                void waitPs(bool on);
                void waitRefChrg(bool on);
                void waitExtChrg(bool on);
                void waitCalStart(bool on);
                void waitCalUntouched(bool on);
                void waitCalThreshold(bool on);
                void waitCalTouched(bool on);
                void waitRead(bool on);
                void waitTest(bool on);
                void waitDone(bool on);

                void updateOpt(ev_e val);
                void updateNscn(ev_e val);
                void updatePs(ev_e val);
                void updateRefChrg(ev_e val);
                void updateExtChrg(ev_e val);
                void updateThreshold(ev_e val);

                void changeNscn(void);
                void changeCalStart(void);
                void changeCalThreshold(void);
                void changeRead(void);
                void changeDone(void);

                void display(df_t flags = DF_NONE);
                void displayOpt(df_t flags = DF_NONE);
                void displayNscn(df_t flags = DF_NONE);
                void displayPs(df_t flags = DF_NONE);
                void displayRefChrg(df_t flags = DF_NONE);
                void displayExtChrg(df_t flags = DF_NONE);
                void displayCalStart(df_t flags = DF_NONE);
                void displayCalThreshold(df_t flags = DF_NONE);
                void displayCalWait(df_t flags = DF_NONE);
                void displayThreshold(df_t flags = DF_NONE);
                void displayDone(df_t flags = DF_NONE);
                void displayDisabled(df_t flags = DF_NONE);

                void reset(void);
                void toggleEnabled(void);

                enum topt_e : uint8_t { TOPT_CAL, TOPT_READ, TOPT_CONF, TOPT_DEF, TOPT_DIS, TOPT_CNT };

                static constexpr char const * const _s_topts[TOPT_CNT] = { "CAL.", "rEAd", "ConF.", "dEF.", "OFF" };

                enum ts_e : uint8_t
                {
                    TS_OPT,

                    // Configure
                    TS_NSCN,
                    TS_PS,
                    TS_REFCHRG,
                    TS_EXTCHRG,

                    // Calibration
                    TS_CAL_START,
                    TS_CAL_UNTOUCHED,
                    TS_CAL_THRESHOLD,
                    TS_CAL_TOUCHED,

                    TS_READ,

                    // Test and adjustment
                    TS_TEST,

                    TS_DONE,
                    TS_DISABLED,

                    TS_CNT
                };

                ts_e const _next_states[TS_CNT] =
                {
                    TS_NSCN,
                    TS_PS,
                    TS_REFCHRG,
                    TS_EXTCHRG,
                    TS_CAL_START,
                    TS_CAL_UNTOUCHED,
                    TS_CAL_THRESHOLD,
                    TS_CAL_TOUCHED,
                    TS_TEST,
                    TS_TEST,
                    TS_DONE,
                    TS_OPT,
                    TS_OPT,
                };

                using ts_wait_action_t = void (SetTouch::*)(bool on);
                ts_wait_action_t const _wait_actions[TS_CNT] =
                {
                    &SetTouch::waitOpt,
                    &SetTouch::waitNscn,
                    &SetTouch::waitPs,
                    &SetTouch::waitRefChrg,
                    &SetTouch::waitExtChrg,
                    &SetTouch::waitCalStart,
                    &SetTouch::waitCalUntouched,
                    &SetTouch::waitCalThreshold,
                    &SetTouch::waitCalTouched,
                    &SetTouch::waitRead,
                    &SetTouch::waitTest,
                    &SetTouch::waitDone,
                    nullptr,
                };

                using ts_update_action_t = void (SetTouch::*)(ev_e ev);
                ts_update_action_t const _update_actions[TS_CNT] =
                {
                    &SetTouch::updateOpt,
                    &SetTouch::updateNscn,
                    &SetTouch::updatePs,
                    &SetTouch::updateRefChrg,
                    &SetTouch::updateExtChrg,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    &SetTouch::updateThreshold,
                    nullptr,
                    nullptr,
                };

                using ts_change_action_t = void (SetTouch::*)(void);
                ts_change_action_t const _change_actions[TS_CNT] =
                {
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    &SetTouch::changeCalStart,
                    nullptr,
                    &SetTouch::changeCalThreshold,
                    nullptr,
                    &SetTouch::changeRead,
                    nullptr,
                    &SetTouch::changeDone,
                    nullptr,
                };

                using ts_display_action_t = void (SetTouch::*)(df_t flags);
                ts_display_action_t const _display_actions[TS_CNT] =
                {
                    &SetTouch::displayOpt,
                    &SetTouch::displayNscn,
                    &SetTouch::displayPs,
                    &SetTouch::displayRefChrg,
                    &SetTouch::displayExtChrg,
                    &SetTouch::displayCalStart,
                    &SetTouch::displayCalWait,
                    &SetTouch::displayCalThreshold,
                    &SetTouch::displayCalWait,
                    &SetTouch::displayThreshold,
                    &SetTouch::displayThreshold,
                    &SetTouch::displayDone,
                    &SetTouch::displayDisabled,
                };

                ts_e _state = TS_CAL_START;
                topt_e _topt = TOPT_CAL;
                tTouch _touch = {};
                TouchCal _untouched = {};
                TouchCal _touched_amp_off = {};
                TouchCal _touched_amp_on = {};
                TouchCal * _touched = &_touched_amp_off;
                uint32_t _readings = 0;
                TouchCal _read = {};

                Tsi & _tsi = Tsi::acquire();
                Tsi::Channel & _tsi_channel = _tsi.acquire < PIN_TOUCH > ();

                // Need to calibrate with Amp turned on since when it's on, for
                // some reason, it gives a wide and varying range of readings.
                TAmp & _amp = TAmp::acquire();

                // To prevent beeper from switching on/off too quickly
                static constexpr uint32_t const _s_beeper_wait = 30;
                static constexpr uint32_t const _s_readings = UINT16_MAX;

                static constexpr uint32_t const _s_read_wait = 80;
                Toggle _read_toggle{_s_read_wait};
        };

        class SetNLC : public UISetState
        {
            public:
                SetNLC(UI & ui);

                virtual void uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(ps_e ps);
                virtual void uisEnd(void);
                virtual void uisRefresh(void);
                virtual bool uisPower(pwr_e pwr);

            private:
                void waitIndex(bool on);
                void waitType(bool on);
                void waitColor(bool on);
                void waitRGB(bool on);
                void waitHSV(bool on);
                void waitDone(bool on);

                void updateIndex(ev_e ev);
                void updateType(ev_e ev);
                void updateColor(ev_e ev);
                void updateRGB(ev_e ev);
                void updateHSV(ev_e ev);

                void changeIndex(void);
                void changeType(void);
                void changeColor(void);
                void changeRGB(void);
                void changeHSV(void);
                void changeDone(void);

                void display(df_t flags = DF_NONE);
                void displayIndex(df_t flags = DF_NONE);
                void displayType(df_t flags = DF_NONE);
                void displayColor(df_t flags = DF_NONE);
                void displayRGB(df_t flags = DF_NONE);
                void displayHSV(df_t flags = DF_NONE);
                void displayDone(df_t flags = DF_NONE);

                void commit(void);

                enum sls_e : uint8_t
                {
                    SLS_INDEX,
                    SLS_TYPE,
                    SLS_COLOR,
                    SLS_RGB,
                    SLS_HSV,
                    SLS_DONE,
                    SLS_CNT
                };

                enum slt_e : uint8_t
                {
                    SLT_COLOR,
                    SLT_RGB,
                    SLT_HSV,
                };

                sls_e _next_type_states[SLT_HSV+1] =
                {
                    SLS_COLOR,
                    SLS_RGB,
                    SLS_HSV,
                };

                using sls_wait_action_t = void (SetNLC::*)(bool on);
                sls_wait_action_t const _wait_actions[SLS_CNT] =
                {
                    &SetNLC::waitIndex,
                    &SetNLC::waitType,
                    &SetNLC::waitColor,
                    &SetNLC::waitRGB,
                    &SetNLC::waitHSV,
                    &SetNLC::waitDone,
                };

                using sls_update_action_t = void (SetNLC::*)(ev_e ev);
                sls_update_action_t const _update_actions[SLS_CNT] =
                {
                    &SetNLC::updateIndex,
                    &SetNLC::updateType,
                    &SetNLC::updateColor,
                    &SetNLC::updateRGB,
                    &SetNLC::updateHSV,
                    nullptr,
                };

                using sls_change_action_t = void (SetNLC::*)(void);
                sls_change_action_t const _change_actions[SLS_CNT] =
                {
                    &SetNLC::changeIndex,
                    &SetNLC::changeType,
                    &SetNLC::changeColor,
                    &SetNLC::changeRGB,
                    &SetNLC::changeHSV,
                    &SetNLC::changeDone,
                };

                using sls_display_action_t = void (SetNLC::*)(df_t flags);
                sls_display_action_t const _display_actions[SLS_CNT] =
                {
                    &SetNLC::displayIndex,
                    &SetNLC::displayType,
                    &SetNLC::displayColor,
                    &SetNLC::displayRGB,
                    &SetNLC::displayHSV,
                    &SetNLC::displayDone
                };

                sls_e _state = SLS_INDEX;
                slt_e _type = SLT_COLOR;
                uint8_t _sub_state = 0;

                CRGB _color = {};

                uint8_t _index = 0;
                uint8_t _cindex = 0;

                CRGB const _default_rgb{ 255, 255, 255 };
                CRGB _rgb{_default_rgb};
                char const * const _rgb_opts[3] = { "R", "G", "B" };

                CHSV const _default_hsv{ 0, 255, 255 };
                CHSV _hsv{_default_hsv};
                char const * const _hsv_opts[3] = { "H", "S", "B" };

                uint8_t * _val = nullptr;
                char const * _opt = nullptr;

                uint32_t _nl_colors[EE_NLC_CNT];

                static constexpr CRGB::Color const _s_colors[] =
                {
                    CRGB::WHITE,                     CRGB::KHAKI,                     CRGB::FAIRY_LIGHT,
                    CRGB::PEACH_PUFF,                CRGB::GOLDENROD,                 CRGB::PERU,
                    CRGB::DARK_SALMON,               CRGB::FLORAL_WHITE,              CRGB::LEMON_CHIFFON,
                    CRGB::SIENNA,                    CRGB::BURLY_WOOD,                CRGB::DARK_GOLDENROD,
                    CRGB::SADDLE_BROWN,              CRGB::PAPAYA_WHIP,               CRGB::DARK_ORANGE,
                    CRGB::TOMATO,                    CRGB::SALMON,                    CRGB::GOLD,
                    CRGB::BLANCHED_ALMOND,           CRGB::LIGHT_SALMON,              CRGB::CORAL,
                    CRGB::ORANGE_RED,                CRGB::TAN,                       CRGB::PLAID,
                    CRGB::MISTY_ROSE,                CRGB::SANDY_BROWN,               CRGB::ORANGE,
                    CRGB::MOCCASIN,                  CRGB::DARK_KHAKI,                CRGB::CHOCOLATE,
                    CRGB::PALE_GOLDENROD,            CRGB::MAROON,                    CRGB::DARK_RED,
                    CRGB::RED,                       CRGB::BROWN,                     CRGB::FIRE_BRICK,
                    CRGB::INDIAN_RED,                CRGB::LIGHT_CORAL,               CRGB::ROSY_BROWN,
                    CRGB::SNOW,                      CRGB::MEDIUM_VIOLET_RED,         CRGB::CRIMSON,
                    CRGB::DEEP_PINK,                 CRGB::PALE_VIOLET_RED,           CRGB::ORCHID,
                    CRGB::HOT_PINK,                  CRGB::LIGHT_PINK,                CRGB::LAVENDER_BLUSH,
                    CRGB::LIGHT_YELLOW,              CRGB::BEIGE,                     CRGB::OLIVE,
                    CRGB::YELLOW,                    CRGB::GREEN_YELLOW,              CRGB::CHARTREUSE,
                    CRGB::YELLOW_GREEN,              CRGB::DARK_OLIVE_GREEN,          CRGB::LAWN_GREEN,
                    CRGB::OLIVE_DRAB,                CRGB::DARK_GREEN,                CRGB::GREEN,
                    CRGB::FOREST_GREEN,              CRGB::LIME_GREEN,                CRGB::DARK_SEA_GREEN,
                    CRGB::LIME,                      CRGB::LIGHT_GREEN,               CRGB::HONEYDEW,
                    CRGB::SEA_GREEN,                 CRGB::LIGHT_SEA_GREEN,           CRGB::MEDIUM_SEA_GREEN,
                    CRGB::MEDIUM_AQUAMARINE,         CRGB::MEDIUM_TURQUOISE,          CRGB::MEDIUM_SPRING_GREEN,
                    CRGB::SPRING_GREEN,              CRGB::AQUAMARINE,                CRGB::MINT_CREAM,
                    CRGB::DARK_SLATE_GREY,           CRGB::TEAL,                      CRGB::AQUA,
                    CRGB::PALE_TURQUOISE,            CRGB::LIGHT_CYAN,                CRGB::GHOST_WHITE,
                    CRGB::MEDIUM_BLUE,               CRGB::DARK_BLUE,                 CRGB::MIDNIGHT_BLUE,
                    CRGB::NAVY,                      CRGB::BLUE,                      CRGB::LAVENDER,
                    CRGB::ROYAL_BLUE,                CRGB::STEEL_BLUE,                CRGB::LIGHT_SLATE_GREY,
                    CRGB::CADET_BLUE,                CRGB::CORNFLOWER_BLUE,           CRGB::DODGER_BLUE,
                    CRGB::DARK_TURQUOISE,            CRGB::DEEP_SKY_BLUE,             CRGB::SKY_BLUE,
                    CRGB::LIGHT_STEEL_BLUE,          CRGB::POWDER_BLUE,               CRGB::ALICE_BLUE,
                    CRGB::INDIGO,                    CRGB::DARK_VIOLET,               CRGB::DARK_SLATE_BLUE,
                    CRGB::BLUE_VIOLET,               CRGB::DARK_ORCHID,               CRGB::SLATE_BLUE,
                    CRGB::MEDIUM_ORCHID,             CRGB::MEDIUM_SLATE_BLUE,         CRGB::AMETHYST,
                    CRGB::MEDIUM_PURPLE,             CRGB::DARK_MAGENTA,              CRGB::FUCHSIA,
                    CRGB::VIOLET,                    CRGB::PLUM,                      CRGB::THISTLE,
                    CRGB::DIM_GREY,                  CRGB::DARK_GREY,                 CRGB::SILVER,
                    CRGB::LIGHT_GREY,                CRGB::GAINSBORO,                 CRGB::WHITE_SMOKE,
                    CRGB::NAVAJO_WHITE,              CRGB::BLACK,
                };
        };

        class Player
        {
            friend class UI;

            public:
                Player(UI & ui);

                void process(void);
                void play(void);
                void pause(void) { _paused = true; }
                bool paused(void) const { return _paused; }
                bool running(void) const { return _audio.running(); }
                bool stopped(void) const { return !running(); }
                void stop(void) { if (running()) _audio.stop(); _paused = true; _stopping = false; }
                bool playing(void) const { return running() && !paused(); }
                bool stopping(void) { return _stopping; }
                bool skipping(void) { return _next_track != _current_track; }
                bool occupied(void) const;
                bool disabled(void) const { return _disabled; }
                uint16_t numTracks(void) const { return _num_tracks; }
                uint16_t currentTrack(void) const { return _current_track; }
                uint16_t nextTrack(void) const { return _next_track; }
                bool setTrack(uint16_t track);

            private:
                bool newTrack(void);
                void disable(void);
                void start(void) { if (!running()) _audio.start(); _paused = false; }
                bool rewind(void);
                bool pressing(void);
                int32_t skip(uint32_t t);
                uint16_t skipTracks(int32_t skip);

                TAudio & _audio = TAudio::acquire();
                TFs & _fs = TFs::acquire();

                UI & _ui;

                File * _track = nullptr;
                bool _paused = false;
                bool _stopping = false;
                bool _disabled = false;

                // 2 or more seconds of continuous press on play/pause pushbutton stops player
                static constexpr uint32_t const _s_stop_time = 2000;
                static constexpr uint32_t const _s_skip_msecs = 1024;
                static constexpr uint16_t const _s_max_tracks = 4096;
                char const * const _track_exts[3] = { "MP3", "M4A", nullptr };
                FileInfo _tracks[_s_max_tracks] = {};
                uint16_t _num_tracks = 0;
                uint16_t _current_track = 0;
                uint16_t _next_track = 0;
        };

        class Alarm
        {
            public:
                enum as_e : uint8_t
                {
                    AS_OFF,
                    AS_STARTED,
                    AS_WOKE,
                    AS_AWAKE,
                    AS_SNOOZED,
                    AS_SNOOZE,
                    AS_STOPPED,
                };

                Alarm(UI & ui);
                void process(bool snooze, bool stop);
                bool snoozed(void) const { return _state == AS_SNOOZED; }
                bool snoozing(void) const { return snoozed() || (_state == AS_SNOOZE); }
                bool started(void) const { return _state == AS_STARTED; }
                bool woke(void) const { return _state == AS_WOKE; }
                bool awake(void) const { return started() || woke() || (_state == AS_AWAKE); }
                bool alerting(void) const { return awake(); }
                bool stopped(void) const { return _state == AS_STOPPED; }
                bool off(void) const { return stopped() || (_state == AS_OFF); }
                bool inProgress(void) const { return snoozing() || awake(); }
                bool beeping(void) const { return alerting() && !_alarm_music; }
                bool playing(void) const { return alerting() && _alarm_music; }

            private:
                void check(void);
                bool snooze(bool force = false);
                void wake(void);
                void stop(void);
                bool interrupt(void);

                UI & _ui;
                as_e _state = AS_OFF;
                bool _alarm_music = false;
        };

        class Power
        {
            public:
                enum ps_e : uint8_t
                {
                    PS_AWAKE,
                    PS_NAPPING,
                    PS_NAPPED,
                    PS_SLEPT,
                };

                Power(UI & ui);
                void process(UIState & uis);
                void update(ePower const & power) { _power = power; }
                bool napping(void) const { return _state == PS_NAPPING; }
                bool napped(void) const { return _state == PS_NAPPED; }
                bool slept(void) const { return _state == PS_SLEPT; }
                bool awake(void) const { return napped() || slept() || (_state == PS_AWAKE); }

            private:
                void updateMarks(void);
                bool touchSleep(void);
                bool canSleep(void);
                bool canNap(void);
                bool canStop(void);
                void nap(UIState & uis);
                void wake(void);
                void sleep(UIState & uis, bool touch);

                Llwu & _llwu = Llwu::acquire();
                UI & _ui;
                ps_e _state = PS_AWAKE;
                uint32_t _nap_mark = 0;
                uint32_t _stop_mark = 0;
                uint32_t _sleep_mark = 0;
                uint32_t _touch_mark = 0;
                ePower _power = {};
        };

        class Lighting
        {
            public:
                Lighting(uint8_t brightness = _default_brightness);

                bool valid(void) { return _leds.valid(); }

                void onNL(bool force = false);
                void offNL(void);
                void toggleNL(void);
                void toggleAni(void);
                bool isOnNL(void) const { return _nl_on; }
                bool isAniNL(void) const { return _nl_ani; }
                void cycleNL(ev_e ev);
                void setNL(uint8_t index, CRGB const & c);
                void updateNL(void);
                void paletteNL(void);
                uint8_t indexNL(void) { return (_active_index == 0) ? 0 : _active_index - 1; }

                void off(void) { _leds.blank(); brightness(0); }
                void on(void) { brightness(_brightness); _leds.show(); }
                bool isOn(void) { return !_leds.isClear(); }

                void up(void);
                void down(void);
                void brightness(uint8_t b);
                uint8_t brightness(void) const { return _brightness; }

                void setColor(CRGB const & c);

            private:
                void removeDups(uint8_t keep_index = 0);

                TLeds & _leds = TLeds::acquire();
                Eeprom & _eeprom = Eeprom::acquire();
                bool _nl_on = false;
                bool _nl_ani = false;
                uint8_t _ani_index = 0;  // Lighting Animation
                static constexpr uint8_t const _default_brightness = 128;
                uint8_t _brightness = _default_brightness;
                static constexpr CRGB::Color const _s_default_color = CRGB::WHITE;
                CRGB _nl_colors[EE_NLC_CNT+1] = {_s_default_color, CRGB::BLACK, CRGB::BLACK, CRGB::BLACK};
                CRGB _active_colors[EE_NLC_CNT+1];
                uint8_t _active_index = 0;
                uint8_t _active_cnt = 1; 
        };

        Controls _controls;
        Lighting _lighting{_brightness};

        Player _player{*this};
        Alarm _alarm{*this};
        Power _power{*this};

        SetAlarm _set_alarm{*this};
        SetClock _set_clock{*this};
        SetTouch _set_touch{*this};
        Clock _clock{*this};
        Timer _timer{*this};
        SetPower _set_power{*this};
        SetNLC _set_nlc{*this};

        UIState * const _states[UIS_CNT] =
        {
            &_set_alarm,
            &_set_clock,
            &_set_touch,
            &_clock,
            &_timer,
            &_set_power,
            &_set_nlc,
        };
};

#endif
