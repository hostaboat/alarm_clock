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

        // Switch states
        enum ss_e : uint8_t
        {
            SS_NONE,
            SS_LEFT,
            SS_MIDDLE,
            SS_RIGHT,
            SS_CNT
        };

        // Encoder states
        enum es_e : uint8_t
        {
            ES_NONE,
            ES_TURNED,
            ES_DEPRESSED,
            ES_SPRESS,
            ES_MPRESS,
            ES_LPRESS
        };

        // UI states
        enum uis_e : uint8_t
        {
            UIS_SET_ALARM,
            UIS_SET_CLOCK,
            UIS_SET_TIMER,
            UIS_CLOCK,
            UIS_TIMER,
            UIS_TOUCH,
            UIS_CNT
        };

        uis_e const _next_states[UIS_CNT][SS_CNT] =
        {
            //      SS_NONE        SS_LEFT  SS_MIDDLE       SS_RIGHT
            { UIS_SET_ALARM, UIS_SET_ALARM, UIS_CLOCK, UIS_SET_CLOCK }, // UIS_SET_ALARM
            { UIS_SET_CLOCK, UIS_SET_ALARM, UIS_CLOCK, UIS_SET_CLOCK }, // UIS_SET_CLOCK
            { UIS_SET_TIMER, UIS_SET_ALARM, UIS_TIMER, UIS_SET_TIMER }, // UIS_SET_TIMER
            {     UIS_CLOCK, UIS_SET_ALARM, UIS_CLOCK, UIS_SET_CLOCK }, // UIS_CLOCK
            {     UIS_TIMER, UIS_SET_ALARM, UIS_TIMER, UIS_SET_TIMER }, // UIS_TIMER
            {     UIS_TOUCH,     UIS_TOUCH, UIS_CLOCK, UIS_SET_CLOCK }  // UIS_TOUCH
        };

        uis_e _state;

        bool resetting(es_e encoder_state, uint32_t depressed_time);
        bool resting(es_e encoder_state, bool ui_state_changed, bool brightness_changed);
        void nightLight(void);
        void updateBrightness(ev_e ev);
        void error(uis_e state = UIS_CNT);

        static constexpr uint32_t const _s_set_blink_time = 500; // milliseconds
        static constexpr uint32_t const _s_reset_blink_time = 300;

        //  2 seconds of continuous pushing of encoder switch
        static constexpr uint32_t const _s_medium_reset_time = 2000;

        // 10 seconds of continuous pushing of encoder switch
        static constexpr uint32_t const _s_long_reset_time = 10000;

        // 30 seconds before resting, i.e. turning display and leds off
        // and skipping UI state processing.
        static constexpr uint32_t const _s_rest_time = 30000;

        TRsw & _select = TRsw::acquire();
        TBrEnc & _br_adjust = TBrEnc::acquire(IRQC_INTR_CHANGE);
        TUiEncSw & _enc_swi = TUiEncSw::acquire(IRQC_INTR_CHANGE, IRQC_INTR_CHANGE);

        TDisplay & _display = TDisplay::acquire();
        TBeep & _beeper = TBeep::acquire();
        TLeds & _leds = TLeds::acquire();
        Rtc & _rtc = Rtc::acquire();
        Eeprom & _eeprom = Eeprom::acquire();
        Tsi::Channel & _tsi_channel = Tsi::acquire < PIN_TOUCH > ();
        Llwu & _llwu = Llwu::acquire();

        CRGB _night_light = CRGB::WHITE;

        // Want to have display and night light brightness synced.
        // The display levels are about 1/16 of the night light levels
        // off by one - 17 display levels and 256 night light levels.
        // The value 120 is midway between 112 and 128. See updateBrightness().
        uint8_t _brightness = 120;

        enum play_err_e
        {
            PLAY_ERR_NONE,
            PLAY_ERR_INVALID,
            PLAY_ERR_GET_FILES,
            PLAY_ERR_OPEN_FILE,
            PLAY_ERR_AUDIO,
        };

        class Player
        {
            friend class UI;

            public:
                Player(void);

                void process(void);
                void play(void);
                void pause(void) { _paused = true; }
                bool paused(void) const { return _paused; }
                bool running(void) const { return _audio.running(); }
                void stop(void) { if (running()) _audio.stop(); _paused = true; }
                bool playing(void) const { return running() && !paused(); }
                bool occupied(void) const;
                bool disabled(void) const { return _error != PLAY_ERR_NONE; }
                play_err_e error(void) const { return _error; }

            private:
                bool newTrack(int16_t inc);
                void disable(void);
                void start(void) { if (!running()) _audio.start(); _paused = false; }

                TAudio & _audio = TAudio::acquire();
                TFs & _fs = TFs::acquire();
                TSwPrev & _prev = TSwPrev::acquire(IRQC_INTR_CHANGE);
                TSwPlay & _play = TSwPlay::acquire(IRQC_INTR_CHANGE);
                TSwNext & _next = TSwNext::acquire(IRQC_INTR_CHANGE);
                Eeprom & _eeprom = Eeprom::acquire();

                File * _file = nullptr;
                bool _paused = false;
                bool _disabled = false;

                // 2 or more seconds of continuous press on play/pause pushbutton stops player
                static constexpr uint32_t _s_stop_time = 2000;
                // 15 minutes of inactivity before turning audio off
                static constexpr uint32_t _s_sleep_time = 15 * 60000; 
                static constexpr uint16_t _s_max_files = 4096;
                char const * const _exts[3] = { "MP3", "M4A", nullptr };
                FileInfo _files[_s_max_files] = {};
                uint16_t _num_files = 0;
                uint16_t _file_index = 0;
                play_err_e _error = PLAY_ERR_NONE;
        };

        class Alarm
        {
            public:
                Alarm(UI & ui) : _ui{ui} {}

                void begin(void);
                void update(uint8_t hour, uint8_t minute);
                bool snooze(bool force = false);
                void stop(void);
                bool enabled(void);
                bool inProgress(void);

            private:
                void check(uint8_t hour, uint8_t minute);
                void wake(void);
                void done(uint8_t hour, uint8_t minute);

                enum as_e : uint8_t
                {
                    AS_OFF,
                    AS_ON,
                    AS_SNOOZE,
                    AS_DONE
                };

                UI & _ui;
                as_e _state = AS_OFF;
                tAlarm _alarm = {};
                uint32_t _alarm_start = 0;
                uint32_t _snooze_start = 0;
                bool _alarm_music = false;
                uint16_t _touch_threshold = 0;
                static constexpr uint32_t const _s_snooze_time = 600000;  // 10 minutes
                static constexpr uint32_t const _s_alarm_time = 7200000;  // 2 hours max
        };

        class UIState
        {
            public:
                UIState(UI & ui) : _ui{ui} {}

                virtual bool uisBegin(void) = 0;
                virtual void uisWait(void) = 0;
                virtual void uisUpdate(ev_e ev) = 0;
                virtual void uisChange(void) = 0;
                virtual void uisReset(void) = 0;
                virtual void uisEnd(void) = 0;
                virtual bool uisSleep(void) = 0;

            protected:
                UI & _ui;
        };

        class UISetState : public UIState
        {
            public:
                UISetState(UI & ui) : UIState(ui) {}

                virtual bool uisBegin(void) = 0;
                virtual void uisWait(void) = 0;
                virtual void uisUpdate(ev_e ev) = 0;
                virtual void uisChange(void) = 0;
                virtual void uisReset(void) = 0;
                virtual void uisEnd(void) = 0;
                virtual bool uisSleep(void) = 0;

            protected:
                Toggle _blink{500};
        };

        class UIRunState : public UIState
        {
            public:
                UIRunState(UI & ui) : UIState(ui) {}

                virtual bool uisBegin(void) = 0;
                virtual void uisWait(void) = 0;
                virtual void uisUpdate(ev_e ev) = 0;
                virtual void uisChange(void) = 0;
                virtual void uisReset(void) = 0;
                virtual void uisEnd(void) = 0;
                virtual bool uisSleep(void) = 0;
        };

        class SetAlarm : public UISetState
        {
            public:
                SetAlarm(UI & ui) : UISetState(ui) {}

                virtual bool uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(void);
                virtual void uisEnd(void);
                virtual bool uisSleep(void);

            private:
                void waitHour(bool on);
                void waitMinute(bool on);
                void waitType(bool on);
                void updateHour(ev_e ev);
                void updateMinute(ev_e ev);
                void updateType(ev_e unused);
                void display(df_t flags = DF_NONE);
                void displayClock(df_t flags = DF_NONE);
                void displayType(df_t flags = DF_NONE);
                void displayDisabled(df_t flags = DF_NONE);
                void toggleEnabled(void);

                enum sas_e : uint8_t
                {
                    SAS_HOUR,
                    SAS_MINUTE,
                    SAS_TYPE,
                    SAS_DISABLED,
                    SAS_DONE,
                    SAS_CNT
                };

                sas_e const _next_states[SAS_CNT] =
                {
                    SAS_MINUTE,
                    SAS_TYPE,
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
                    nullptr,
                    nullptr
                };

                using sas_update_action_t = void (SetAlarm::*)(ev_e ev);
                sas_update_action_t const _update_actions[SAS_CNT] =
                {
                    &SetAlarm::updateHour,
                    &SetAlarm::updateMinute,
                    &SetAlarm::updateType,
                    nullptr,
                    nullptr,
                };

                using sas_display_action_t = void (SetAlarm::*)(df_t flags);
                sas_display_action_t const _display_actions[SAS_CNT] =
                {
                    &SetAlarm::displayClock,
                    &SetAlarm::displayClock,
                    &SetAlarm::displayType,
                    &SetAlarm::displayDisabled,
                    &SetAlarm::displayClock
                };

                sas_e _state = SAS_HOUR;
                tAlarm _alarm = {};
        };

        class SetClock : public UISetState
        {
            public:
                SetClock(UI & ui) : UISetState(ui) {}

                virtual bool uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(void);
                virtual void uisEnd(void);
                virtual bool uisSleep(void);

            private:
                void waitType(bool on);
                void waitHour(bool on);
                void waitMinute(bool on);
                void waitYear(bool on);
                void waitMonth(bool on);
                void waitDay(bool on);
                void waitDST(bool on);
                void updateType(ev_e unused);
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
                    nullptr
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
                    &SetClock::displayTime
                };

                scs_e _state = SCS_TYPE;
                tClock _clock = {};
        };

        class SetTimer : public UISetState
        {
            public:
                SetTimer(UI & ui) : UISetState(ui) {}

                virtual bool uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(void);
                virtual void uisEnd(void);
                virtual bool uisSleep(void);

            private:
                void waitMinutes(bool on);
                void waitSeconds(bool on);
                void updateMinutes(ev_e ev);
                void updateSeconds(ev_e ev);
                void display(df_t flags = DF_NONE);
                void displayTimer(df_t flags = DF_NONE);

                enum sts_e : uint8_t
                {
                    STS_MINUTES,
                    STS_SECONDS,
                    STS_DONE,
                    STS_CNT
                };

                sts_e const _next_states[STS_CNT] =
                {
                    STS_SECONDS,
                    STS_DONE,
                    STS_MINUTES
                };

                using sts_wait_action_t = void (SetTimer::*)(bool on);
                sts_wait_action_t const _wait_actions[STS_CNT] =
                {
                    &SetTimer::waitMinutes,
                    &SetTimer::waitSeconds,
                    nullptr
                };

                using sts_update_action_t = void (SetTimer::*)(ev_e ev);
                sts_update_action_t const _update_actions[STS_CNT] =
                {
                    &SetTimer::updateMinutes,
                    &SetTimer::updateSeconds,
                    nullptr
                };

                using sts_display_action_t = void (SetTimer::*)(df_t flags);
                sts_display_action_t const _display_actions[STS_CNT] =
                {
                    &SetTimer::displayTimer,
                    &SetTimer::displayTimer,
                    &SetTimer::displayTimer
                };

                sts_e _state = STS_MINUTES;
                tTimer _timer = {};
        };

        class Clock : public UIRunState
        {
            public:
                Clock(UI & ui) : UIRunState(ui) {}

                virtual bool uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(void);
                virtual void uisEnd(void);
                virtual bool uisSleep(void);

            private:
                bool clockUpdate(bool force = false);
                void display(void);
                void displayTime(void);
                void displayDate(void);
                void displayYear(void);

                enum cs_e : uint8_t
                {
                    CS_TIME,
                    CS_DATE,
                    CS_YEAR,
                    CS_CNT
                };

                cs_e const _next_states[CS_CNT] =
                {
                    CS_DATE,
                    CS_YEAR,
                    CS_TIME
                };

                using cs_display_action_t = void (Clock::*)(void);
                cs_display_action_t const _display_actions[CS_CNT] =
                {
                    &Clock::displayTime,
                    &Clock::displayDate,
                    &Clock::displayYear
                };

                cs_e _state = CS_TIME;
                tClock _clock = {};
                Alarm _alarm{_ui};
                Toggle _date_display{3000};  // Show date/year for 3 seconds
                df_t _dflag = DF_12H;
        };

        class Timer : public UIRunState
        {
            public:
                Timer(UI & ui) : UIRunState(ui) {}

                virtual bool uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(void);
                virtual void uisEnd(void);
                virtual bool uisSleep(void);

                bool running(void) { return (_state == TS_RUNNING) || (_state == TS_ALERT); }

            private:
                void start(void);
                void reset(void);
                void run(void);
                void pause(void);
                void resume(void);
                void alert(void);
                void stop(void);

                void timer(bool force = false);
                void clock(bool force = false);

                enum ts_e : uint8_t
                {
                    TS_WAIT,
                    TS_RUNNING,
                    TS_PAUSED,
                    TS_ALERT,
                    TS_CNT
                };

                ts_e const _next_states[TS_CNT] =
                {
                    TS_RUNNING,
                    TS_PAUSED,
                    TS_RUNNING,
                    TS_WAIT
                };

                using ts_wait_action_t = void (Timer::*)(void);
                ts_wait_action_t const _wait_actions[TS_CNT] =
                {
                    nullptr,
                    &Timer::run,
                    nullptr,
                    &Timer::alert
                };

                using ts_change_action_t = void (Timer::*)(void);
                ts_change_action_t const _change_actions[TS_CNT] =
                {
                    &Timer::start,
                    &Timer::pause,
                    &Timer::resume,
                    &Timer::reset
                };

                ts_e _state = TS_WAIT;
                tTimer _timer = {};
                uint32_t _second = 0;
                uint8_t _minute = 0;
                uint8_t _last_hue = 0;
                uint32_t _last_second = 0;
                Pit * _display_timer = nullptr;
                Pit * _led_timer = nullptr;

                // For showing the clock while in timer state
                bool _show_clock = false;
                static constexpr uint32_t const _s_turn_wait = 300;
                static constexpr int8_t const _s_turns = 3;

                // Start with blue hue and decrease to red hue
                static constexpr uint8_t const _s_timer_hue_max = 160; // blue
                static constexpr uint8_t const _s_timer_hue_min = 0;   // red
                static constexpr uint32_t _s_seconds_interval = 1000000;
                static constexpr uint32_t _s_hue_interval = _s_seconds_interval / _s_timer_hue_max;

                // ISR
                static void timerDisplay(void);
                static void timerLeds(void);

                static uint32_t volatile _s_timer_seconds;
                static uint8_t volatile _s_timer_hue;
        };
       
        class Touch : public UISetState
        {
            public:
                Touch(UI & ui) : UISetState(ui) {}

                virtual bool uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(void);
                virtual void uisEnd(void);
                virtual bool uisSleep(void);

            private:
                void waitRead(bool unused);
                void waitThreshold(bool on);
                void waitTest(bool unused);
                void updateThreshold(ev_e val);
                void changeRead(void);
                void changeThreshold(void);
                void changeTest(void);
                void changeDone(void);
                void display(df_t flags = DF_NONE);
                void displayRead(df_t flags = DF_NONE);
                void displayThreshold(df_t flags = DF_NONE);

                enum ts_e : uint8_t
                {
                    TS_READ,
                    TS_THRESHOLD,
                    TS_TEST,
                    TS_DONE,
                    TS_CNT
                };

                ts_e const _next_states[TS_CNT] =
                {
                    TS_THRESHOLD,
                    TS_TEST,
                    TS_DONE,
                    TS_READ
                };

                using ts_wait_action_t = void (Touch::*)(bool on);
                ts_wait_action_t const _wait_actions[TS_CNT] =
                {
                    &Touch::waitRead,
                    &Touch::waitThreshold,
                    &Touch::waitTest,
                    nullptr
                };

                using ts_update_action_t = void (Touch::*)(ev_e ev);
                ts_update_action_t const _update_actions[TS_CNT] =
                {
                    nullptr,
                    &Touch::updateThreshold,
                    &Touch::updateThreshold,
                    nullptr
                };

                using ts_change_action_t = void (Touch::*)(void);
                ts_change_action_t const _change_actions[TS_CNT] =
                {
                    &Touch::changeRead,
                    &Touch::changeThreshold,
                    &Touch::changeTest,
                    &Touch::changeDone,
                };

                using ts_display_action_t = void (Touch::*)(df_t flags);
                ts_display_action_t const _display_actions[TS_CNT] =
                {
                    &Touch::displayRead,
                    &Touch::displayThreshold,
                    &Touch::displayThreshold,
                    &Touch::displayThreshold,
                };

                ts_e _state = TS_READ;
                uint16_t _touch_read = 0;
                static constexpr uint32_t const _s_touch_read_interval = 500;
        };

        Player _player;
        SetAlarm _set_alarm{*this};
        SetClock _set_clock{*this};
        SetTimer _set_timer{*this};
        Clock _clock{*this};
        Timer _timer{*this};
        Touch _touch{*this};

        UIState * const _states[UIS_CNT] =
        {
            &_set_alarm,
            &_set_clock,
            &_set_timer,
            &_clock,
            &_timer,
            &_touch
        };
};

#endif
