/*
 * bl-hook.dylib: iOS's brightness slider on raphael (Redmi K20 Pro), for backboardd.
 *
 * raphael's panel has no backlight service iOS can use (with the kernel's AppleARMBacklight service CoreBrightness
 * crashes backboardd, docs/IOS18.md), so iOS finds brightness control unavailable. Every change of the slider (and of
 * auto-brightness, the lock screen, ...) ends in backboardd's
 * -[BKDisplayBrightnessController setBrightnessLevel:reason:options:]. As HakiOS's userland hook does
 * (userland/src/inject.c, brightness_hook_install()), this library, linked into backboardd (LC_LOAD_DYLIB):
 *   - replaces that method: calls the original, then writes the level, 0..1000, to kern.sweet_brightness
 *     (bsd/dev/sweet_dsibl.c, patch 0265; boot-arg sweetbl=2), whose driver sets the panel;
 *   - makes isBrightnessLevelControlAvailable answer yes, so the slider moves;
 *   - makes brightnessLevel return the level last set, so the slider stays where it was put.
 * A method is only touched when its type encoding is the expected one. One line per event goes to
 * /private/var/tmp/sweet-bl-hook.log (the first 200 level changes).
 *
 *   xcrun -sdk iphoneos clang -arch arm64e -mios-version-min=18.0 -O2 -dynamiclib \
 *       -install_name /usr/local/lib/sweet/bl-hook.dylib -o bl-hook.dylib sweet_bl_hook.c -lobjc
 *   python3 -I tools/renegade/pacstrip.py --sign bl-hook.dylib
 */
#include <dispatch/dispatch.h>
#include <errno.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

static void (*orig_set)(id, SEL, float, id, unsigned long long);
static float (*orig_level)(id, SEL);
static float last_level = -1.0f;        /* what iOS last set; -1 nothing yet */
static unsigned logged;

static void
say(const char *fmt, ...)
{
	FILE *f = fopen("/private/var/tmp/sweet-bl-hook.log", "a");
	va_list ap;

	if (f == NULL) {
		return;
	}
	fprintf(f, "[%d] ", getpid());
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

static BOOL
available_hook(id self, SEL sel)
{
	(void)self;
	(void)sel;
	return YES;
}

static float
level_hook(id self, SEL sel)
{
	return last_level >= 0.0f ? last_level : orig_level(self, sel);
}

static void
set_hook(id self, SEL sel, float level, id reason, unsigned long long options)
{
	int permille = (int)(level * 1000.0f + 0.5f);
	int error = 0;

	last_level = level;
	orig_set(self, sel, level, reason, options);
	permille = permille < 0 ? 0 : permille > 1000 ? 1000 : permille;
	if (sysctlbyname("kern.sweet_brightness", NULL, NULL, &permille, sizeof(permille)) != 0) {
		error = errno;
	}
	if (logged < 200) {
		logged++;
		say("level %.3f -> kern.sweet_brightness=%d%s%d", (double)level, permille, error ? ": errno " : " ok ", error);
	}
}

static bool
install(void)
{
	Class cls = objc_getClass("BKDisplayBrightnessController");
	Method m = cls ? class_getInstanceMethod(cls, sel_registerName("setBrightnessLevel:reason:options:")) : NULL;
	const char *enc = m ? method_getTypeEncoding(m) : NULL;

	if (cls == NULL) {
		return false;
	}
	if (enc == NULL || strcmp(enc, "v36@0:8f16@20Q28") != 0) {
		say("setBrightnessLevel:reason:options: not as expected (encoding %s): left alone", enc ? enc : "none");
		return true;
	}
	orig_set = (void (*)(id, SEL, float, id, unsigned long long))method_getImplementation(m);
	method_setImplementation(m, (IMP)set_hook);
	say("hooked -[BKDisplayBrightnessController setBrightnessLevel:reason:options:]");
	m = class_getInstanceMethod(cls, sel_registerName("isBrightnessLevelControlAvailable"));
	enc = m ? method_getTypeEncoding(m) : NULL;
	if (enc != NULL && strcmp(enc, "B16@0:8") == 0) {
		method_setImplementation(m, (IMP)available_hook);
		say("isBrightnessLevelControlAvailable answers yes");
	} else {
		say("isBrightnessLevelControlAvailable not as expected (encoding %s)", enc ? enc : "none");
	}
	m = class_getInstanceMethod(cls, sel_registerName("brightnessLevel"));
	enc = m ? method_getTypeEncoding(m) : NULL;
	if (enc != NULL && strcmp(enc, "f16@0:8") == 0) {
		orig_level = (float (*)(id, SEL))method_getImplementation(m);
		method_setImplementation(m, (IMP)level_hook);
		say("brightnessLevel returns the level last set");
	} else {
		say("brightnessLevel not as expected (encoding %s)", enc ? enc : "none");
	}
	return true;
}

static void
try_install(int left)
{
	if (install() || left <= 0) {
		if (left <= 0) {
			say("BKDisplayBrightnessController not found: nothing hooked");
		}
		return;
	}
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC), dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
		try_install(left - 1);
	});
}

__attribute__((constructor))
static void
sweet_bl_hook_init(void)
{
	int level = -2;
	size_t len = sizeof(level);

	if (sysctlbyname("kern.sweet_brightness", &level, &len, NULL, 0) != 0) {
		say("no kern.sweet_brightness (errno %d): not hooking", errno);
		return;
	}
	try_install(20);
}
