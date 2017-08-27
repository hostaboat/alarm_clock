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
            UIS_SET_LEDS,
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
            {     UIS_TOUCH,     UIS_TOUCH, UIS_CLOCK, UIS_SET_CLOCK }, // UIS_TOUCH
            {  UIS_SET_LEDS, UIS_SET_ALARM, UIS_CLOCK,  UIS_SET_LEDS }  // UIS_SET_LEDS
        };

        uis_e _state;

        bool resetting(es_e encoder_state, uint32_t depressed_time);
        bool resting(es_e encoder_state, bool ui_state_changed, bool brightness_changed);
        void nightLight(void);
        void updateBrightness(ev_e ev);
        void error(uis_e state = UIS_CNT);

        static constexpr uint32_t const _s_set_blink_time = 500; // milliseconds
        static constexpr uint32_t const _s_reset_blink_time = 300;

        // 2 seconds of continuous pushing of encoder switch
        static constexpr uint32_t const _s_medium_reset_time = 2000;

        // 6 seconds of continuous pushing of encoder switch
        static constexpr uint32_t const _s_long_reset_time = 6000;

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
        bool _nl_on = false;

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
                uint32_t _alarm_start = 0, _alarm_time = 0;
                uint32_t _wake_start = 0, _wake_time = 0;
                uint32_t _snooze_start = 0, _snooze_time = 600000;
                bool _alarm_music = false;
                uint16_t _touch_threshold = 0;
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
                bool _updated = false;
                bool _done = false;
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
                void waitDone(bool on);

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

        class SetLeds : public UISetState
        {
            public:
                SetLeds(UI & ui) : UISetState(ui) {}

                virtual bool uisBegin(void);
                virtual void uisWait(void);
                virtual void uisUpdate(ev_e ev);
                virtual void uisChange(void);
                virtual void uisReset(void);
                virtual void uisEnd(void);
                virtual bool uisSleep(void);

            private:
                void waitType(bool on);
                void waitColor(bool on);
                void waitRGB(bool on);
                void waitHSV(bool on);
                void waitDone(bool on);

                void updateType(ev_e ev);
                void updateColor(ev_e ev);
                void updateRGB(ev_e ev);
                void updateHSV(ev_e ev);

                void display(df_t flags = DF_NONE);
                void displayType(df_t flags = DF_NONE);
                void displayColor(df_t flags = DF_NONE);
                void displayRGB(df_t flags = DF_NONE);
                void displayHSV(df_t flags = DF_NONE);
                void displayDone(df_t flags = DF_NONE);

                void displayLeds(void);

                enum sls_e : uint8_t
                {
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

                using sls_wait_action_t = void (SetLeds::*)(bool on);
                sls_wait_action_t const _wait_actions[SLS_CNT] =
                {
                    &SetLeds::waitType,
                    &SetLeds::waitColor,
                    &SetLeds::waitRGB,
                    &SetLeds::waitHSV,
                    &SetLeds::waitDone,
                };

                using sls_update_action_t = void (SetLeds::*)(ev_e ev);
                sls_update_action_t const _update_actions[SLS_CNT] =
                {
                    &SetLeds::updateType,
                    &SetLeds::updateColor,
                    &SetLeds::updateRGB,
                    &SetLeds::updateHSV,
                    nullptr,
                };

                using sls_display_action_t = void (SetLeds::*)(df_t flags);
                sls_display_action_t const _display_actions[SLS_CNT] =
                {
                    &SetLeds::displayType,
                    &SetLeds::displayColor,
                    &SetLeds::displayRGB,
                    &SetLeds::displayHSV,
                    &SetLeds::displayDone
                };

                sls_e _state = SLS_TYPE;
                slt_e _type = SLT_COLOR;
                uint8_t _sub_state = 0;

                CRGB _color = {};

                uint8_t _cindex = 0;

                CRGB const _default_rgb{ 255, 255, 255 };
                CRGB _rgb{_default_rgb};
                char const * const _rgb_opts[3] = { "R", "G", "B" };

                CHSV const _default_hsv{ 0, 255, 255 };
                CHSV _hsv{_default_hsv};
                char const * const _hsv_opts[3] = { "H", "S", "B" };

                uint8_t * _val = nullptr;
                char const * _opt = nullptr;

                static constexpr CRGB::Color const _s_colors[] =
                {
                    CRGB::NAVAJO_WHITE,           CRGB::KHAKI,                  CRGB::FAIRY_LIGHT,
                    CRGB::PEACH_PUFF,             CRGB::GOLDENROD,              CRGB::WHEAT,
                    CRGB::PERU,                   CRGB::DARK_SALMON,            CRGB::FLORAL_WHITE,
                    CRGB::LEMON_CHIFFON,          CRGB::SIENNA,                 CRGB::CORNSILK,
                    CRGB::SEASHELL,               CRGB::BURLY_WOOD,             CRGB::OLD_LACE,
                    CRGB::DARK_GOLDENROD,         CRGB::SADDLE_BROWN,           CRGB::PAPAYA_WHIP,
                    CRGB::DARK_ORANGE,            CRGB::TOMATO,                 CRGB::LINEN,
                    CRGB::SALMON,                 CRGB::GOLD,                   CRGB::BLANCHED_ALMOND,
                    CRGB::LIGHT_SALMON,           CRGB::CORAL,                  CRGB::ORANGE_RED,
                    CRGB::ANTIQUE_WHITE,          CRGB::TAN,                    CRGB::PLAID,
                    CRGB::MISTY_ROSE,             CRGB::SANDY_BROWN,            CRGB::ORANGE,
                    CRGB::BISQUE,                 CRGB::MOCCASIN,               CRGB::DARK_KHAKI,
                    CRGB::CHOCOLATE,              CRGB::PALE_GOLDENROD,         CRGB::MAROON,
                    CRGB::DARK_RED,               CRGB::RED,                    CRGB::BROWN,
                    CRGB::FIRE_BRICK,             CRGB::INDIAN_RED,             CRGB::LIGHT_CORAL,
                    CRGB::ROSY_BROWN,             CRGB::SNOW,                   CRGB::MEDIUM_VIOLET_RED,
                    CRGB::CRIMSON,                CRGB::DEEP_PINK,              CRGB::PALE_VIOLET_RED,
                    CRGB::ORCHID,                 CRGB::HOT_PINK,               CRGB::LIGHT_PINK,
                    CRGB::PINK,                   CRGB::LAVENDER_BLUSH,         CRGB::IVORY,
                    CRGB::LIGHT_YELLOW,           CRGB::LIGHT_GOLDENROD_YELLOW, CRGB::BEIGE,
                    CRGB::OLIVE,                  CRGB::YELLOW,                 CRGB::GREEN_YELLOW,
                    CRGB::CHARTREUSE,             CRGB::YELLOW_GREEN,           CRGB::DARK_OLIVE_GREEN,
                    CRGB::LAWN_GREEN,             CRGB::OLIVE_DRAB,             CRGB::DARK_GREEN,
                    CRGB::GREEN,                  CRGB::FOREST_GREEN,           CRGB::LIME_GREEN,
                    CRGB::DARK_SEA_GREEN,         CRGB::LIME,                   CRGB::LIGHT_GREEN,
                    CRGB::PALE_GREEN,             CRGB::HONEYDEW,               CRGB::SEA_GREEN,
                    CRGB::LIGHT_SEA_GREEN,        CRGB::MEDIUM_SEA_GREEN,       CRGB::MEDIUM_AQUAMARINE,
                    CRGB::MEDIUM_TURQUOISE,       CRGB::TURQUOISE,              CRGB::MEDIUM_SPRING_GREEN,
                    CRGB::SPRING_GREEN,           CRGB::AQUAMARINE,             CRGB::MINT_CREAM,
                    CRGB::DARK_SLATE_GREY,        CRGB::TEAL,                   CRGB::DARK_CYAN,
                    CRGB::AQUA,                   CRGB::PALE_TURQUOISE,         CRGB::LIGHT_CYAN,
                    CRGB::AZURE,                  CRGB::GHOST_WHITE,            CRGB::MEDIUM_BLUE,
                    CRGB::DARK_BLUE,              CRGB::MIDNIGHT_BLUE,          CRGB::NAVY,
                    CRGB::BLUE,                   CRGB::LAVENDER,               CRGB::ROYAL_BLUE,
                    CRGB::STEEL_BLUE,             CRGB::LIGHT_SLATE_GREY,       CRGB::SLATE_GREY,
                    CRGB::CADET_BLUE,             CRGB::CORNFLOWER_BLUE,        CRGB::DODGER_BLUE,
                    CRGB::DARK_TURQUOISE,         CRGB::DEEP_SKY_BLUE,          CRGB::SKY_BLUE,
                    CRGB::LIGHT_SKY_BLUE,         CRGB::LIGHT_STEEL_BLUE,       CRGB::POWDER_BLUE,
                    CRGB::LIGHT_BLUE,             CRGB::ALICE_BLUE,             CRGB::INDIGO,
                    CRGB::DARK_VIOLET,            CRGB::DARK_SLATE_BLUE,        CRGB::BLUE_VIOLET,
                    CRGB::DARK_ORCHID,            CRGB::SLATE_BLUE,             CRGB::MEDIUM_ORCHID,
                    CRGB::MEDIUM_SLATE_BLUE,      CRGB::AMETHYST,               CRGB::MEDIUM_PURPLE,
                    CRGB::DARK_MAGENTA,           CRGB::PURPLE,                 CRGB::FUCHSIA,
                    CRGB::VIOLET,                 CRGB::PLUM,                   CRGB::THISTLE,
                    CRGB::DIM_GREY,               CRGB::GREY,                   CRGB::DARK_GREY,
                    CRGB::SILVER,                 CRGB::LIGHT_GREY,             CRGB::GAINSBORO,
                    CRGB::WHITE_SMOKE,            CRGB::WHITE,
                };
        };

        Player _player;
        SetAlarm _set_alarm{*this};
        SetClock _set_clock{*this};
        SetTimer _set_timer{*this};
        Clock _clock{*this};
        Timer _timer{*this};
        Touch _touch{*this};
        SetLeds _set_leds{*this};

        UIState * const _states[UIS_CNT] =
        {
            &_set_alarm,
            &_set_clock,
            &_set_timer,
            &_clock,
            &_timer,
            &_touch,
            &_set_leds,
        };
};

#endif
