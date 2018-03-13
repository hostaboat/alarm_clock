#include "armv7m.h"
#include "utility.h"
#include "ui.h"
#include "usb.h"

extern "C" int main(void)
{
    SysTick::start(TICKS_PER_MSEC);
    NVIC::init();

    // ~1170us wait else cycle count doesn't increment - wait for a bit longer
    delay_msecs(10);

    Usb & usb = Usb::acquire();
    UI & ui = UI::acquire();

    while (true)
    {
        usb.process();
        ui.process();
    }

    return 0;
}
