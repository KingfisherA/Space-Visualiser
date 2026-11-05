#include "Trackpad.hpp"

#import <AppKit/AppKit.h>

namespace
{
	id scrollMonitor = nil;
	id magnifyMonitor = nil;
	bool lastScrollWasPrecise = false;
	float magnification = 0.0f;
}

namespace SV::Trackpad
{
	void Install()
	{
		if (scrollMonitor != nil)
			return;

		// Local monitors see each event before it is dispatched to the window (and so before GLFW's
		// scroll callback). The events are passed on unchanged.
		scrollMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
															  handler:^NSEvent*(NSEvent* event) {
																  lastScrollWasPrecise = [event hasPreciseScrollingDeltas];
																  return event;
															  }];
		magnifyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMagnify
															   handler:^NSEvent*(NSEvent* event) {
																   magnification += (float)[event magnification];
																   return event;
															   }];
	}

	void Uninstall()
	{
		if (scrollMonitor != nil)
			[NSEvent removeMonitor:scrollMonitor];
		if (magnifyMonitor != nil)
			[NSEvent removeMonitor:magnifyMonitor];
		scrollMonitor = nil;
		magnifyMonitor = nil;
	}

	bool LastScrollWasPrecise()
	{
		return lastScrollWasPrecise;
	}

	float ConsumeMagnification()
	{
		const float value = magnification;
		magnification = 0.0f;
		return value;
	}
}
